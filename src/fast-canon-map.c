// This software is part of github.com/waynebhayes/BLANT, and is Copyright(C) Wayne B. Hayes 2025, under the GNU LGPL 3.0
// (GNU Lesser General Public License, version 3, 2007), a copy of which is contained at the top of the repo.
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#include "blant.h"

#if !LOWER_TRIANGLE
#error "fast-canon-map currently requires LOWER_TRIANGLE=1"
#endif

#define FAST_CANON_MAX_K 8
#define MAX_CANONICALS_K8 12346

#define CANON_BITS 14u
#define CANON_MASK ((1u << CANON_BITS) - 1u)

#define LUT_CHUNK_BITS 7u
#define LUT_CHUNK_SIZE (1u << LUT_CHUNK_BITS)
#define LUT_CHUNKS 4u
#define LUT_WORDS_PER_PERM (LUT_CHUNK_SIZE * LUT_CHUNKS)

#define HASH_BITS 16u
#define HASH_SIZE (1u << HASH_BITS)
#define HASH_MASK (HASH_SIZE - 1u)

static int k;
static int numBits;               // k*(k-1)/2
static unsigned long numBitValues; // 2^(k choose 2)
static int f;                     // k!

// Packed as [perm_index: bits 14..] | [canon_index: bits 0..13].
// For k<=8, perm_index <= 40319 and canon_index <= 12345 so uint32_t is sufficient.
static uint32_t *packedData;

// done bitset
static uint64_t *doneBits;
static size_t doneWordCount;

static unsigned long canonicalDecimal[MAX_CANONICALS_K8];

// permutations[p*k + i] is the i-th element of permutation p
static uint8_t *permutations;
static char *permStrings; // permutations as strings, built after optional inversion

// For each permutation, a 4x128 table for fast 28-bit bit-permutation.
// permLUT[p][chunk][value] flattened into one array.
static uint32_t *permLUT;
static uint32_t *permutedNums; // scratch: permuted graph number per permutation index

// Mapping from lower-triangle bit index -> edge endpoints (u>v), using LSB-first ordering.
static uint8_t edgeU[FAST_CANON_MAX_K * (FAST_CANON_MAX_K - 1) / 2];
static uint8_t edgeV[FAST_CANON_MAX_K * (FAST_CANON_MAX_K - 1) / 2];
static int edgeIndex[FAST_CANON_MAX_K][FAST_CANON_MAX_K];

// Per-canonical local dedup hash table: map permuted num -> first nP that produced it.
static uint32_t *hashKeys;
static uint16_t *hashFirstPerm;
static uint16_t *hashEpochs;
static uint16_t *usedSlots;
static uint16_t hashEpoch = 0;

typedef struct {
    int participants; // includes main thread as participant 0
    int numPermutations;
    const uint32_t *lut;
    uint32_t *out;
    volatile uint32_t currentGint;
    volatile uint32_t workEpoch;
    volatile int workersDone;
    volatile int workersReady;
    volatile int shutdownFlag;
    pthread_t *threads;
    int workerCount;
} PermutePool;

typedef struct {
    PermutePool *pool;
    int participantId; // 1..participants-1
} WorkerArg;

static PermutePool gPool;
static WorkerArg *gWorkerArgs;

static void *xmalloc(size_t nBytes, const char *what)
{
    if (nBytes == 0) nBytes = 1;
    void *p = malloc(nBytes);
    if (!p) {
        fprintf(stderr, "fast-canon-map: failed to allocate %s (%zu bytes)\n", what, nBytes);
        exit(1);
    }
    return p;
}

static void *xcalloc(size_t n, size_t elemSize, const char *what)
{
    if (n == 0) n = 1;
    void *p = calloc(n, elemSize);
    if (!p) {
        fprintf(stderr, "fast-canon-map: failed to allocate %s (%zu bytes)\n", what, n * elemSize);
        exit(1);
    }
    return p;
}

static inline bool DoneGet(unsigned long idx)
{
    return (doneBits[idx >> 6] >> (idx & 63UL)) & 1ULL;
}

static inline void DoneSet(unsigned long idx)
{
    doneBits[idx >> 6] |= (1ULL << (idx & 63UL));
}

static int factorial_int(int n)
{
    int r = 1;
    for (int i = 2; i <= n; ++i) r *= i;
    return r;
}

static bool nextPermutation(int permutation[])
{
    for (int i = k - 1; i > 0; --i) {
        if (permutation[i] > permutation[i - 1]) {
            for (int j = k - 1; j > i - 1; --j) {
                if (permutation[i - 1] < permutation[j]) {
                    int t = permutation[i - 1];
                    permutation[i - 1] = permutation[j];
                    permutation[j] = t;
                    break;
                }
            }
            for (int left = i, right = k - 1; left < right; ++left, --right) {
                int t = permutation[left];
                permutation[left] = permutation[right];
                permutation[right] = t;
            }
            return true;
        }
    }
    return false;
}

static void BuildPermutations(void)
{
    int tmpPerm[FAST_CANON_MAX_K];
    for (int i = 0; i < k; ++i) tmpPerm[i] = i;

    for (int p = 0; p < f; ++p) {
        uint8_t *dst = permutations + (size_t)p * (size_t)k;
        for (int j = 0; j < k; ++j) dst[j] = (uint8_t)tmpPerm[j];
        (void)nextPermutation(tmpPerm);
    }
}

static void BuildEdgeMaps(void)
{
    for (int i = 0; i < FAST_CANON_MAX_K; ++i)
        for (int j = 0; j < FAST_CANON_MAX_K; ++j)
            edgeIndex[i][j] = -1;

    int bit = 0;
    for (int i = k - 1; i >= 1; --i) {
        for (int j = i - 1; j >= 0; --j) {
            edgeU[bit] = (uint8_t)i;
            edgeV[bit] = (uint8_t)j;
            edgeIndex[i][j] = bit;
            edgeIndex[j][i] = bit;
            ++bit;
        }
    }
}

static inline uint32_t PermuteWithLUT(const uint32_t *base, uint32_t g)
{
    return base[g & 0x7Fu] |
           base[128u + ((g >> 7) & 0x7Fu)] |
           base[256u + ((g >> 14) & 0x7Fu)] |
           base[384u + ((g >> 21) & 0x7Fu)];
}

static void BuildPermutationLUT(void)
{
    for (int p = 0; p < f; ++p) {
        uint32_t *base = permLUT + (size_t)p * LUT_WORDS_PER_PERM;
        memset(base, 0, LUT_WORDS_PER_PERM * sizeof(uint32_t));

        const uint8_t *perm = permutations + (size_t)p * (size_t)k;
        for (int outBit = 0; outBit < numBits; ++outBit) {
            int i = edgeU[outBit], j = edgeV[outBit];
            int a = perm[i], b = perm[j];
            if (a < b) {
                int t = a;
                a = b;
                b = t;
            }

            int srcBit = edgeIndex[a][b];
            int chunk = srcBit / (int)LUT_CHUNK_BITS;
            int inChunkBit = srcBit % (int)LUT_CHUNK_BITS;
            uint32_t outMask = 1u << outBit;
            uint32_t bitMask = 1u << inChunkBit;
            uint32_t *table = base + (size_t)chunk * LUT_CHUNK_SIZE;

            for (int v = 0; v < (int)LUT_CHUNK_SIZE; ++v) {
                if ((v & bitMask) != 0u) table[v] |= outMask;
            }
        }
    }
}

static inline void CpuRelax(void)
{
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause");
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield");
#endif
}

static inline void ComputePermutationChunk(PermutePool *pool, int participantId, uint32_t gint)
{
    int total = pool->numPermutations - 1; // skip identity permutation 0
    int start = 1 + (total * participantId) / pool->participants;
    int end = 1 + (total * (participantId + 1)) / pool->participants;

    uint32_t *out = pool->out;
    const uint32_t *base = pool->lut + (size_t)start * LUT_WORDS_PER_PERM;
    for (int nP = start; nP < end; ++nP, base += LUT_WORDS_PER_PERM) {
        out[nP] = PermuteWithLUT(base, gint);
    }
}

static void *PermutationWorker(void *arg)
{
    WorkerArg *w = (WorkerArg *)arg;
    PermutePool *pool = w->pool;

    uint32_t seenEpoch = 0;
    __atomic_fetch_add(&pool->workersReady, 1, __ATOMIC_RELEASE);
    for (;;) {
        uint32_t epoch;
        while ((epoch = __atomic_load_n(&pool->workEpoch, __ATOMIC_ACQUIRE)) == seenEpoch) {
            if (__atomic_load_n(&pool->shutdownFlag, __ATOMIC_RELAXED)) return NULL;
            CpuRelax();
        }

        seenEpoch = epoch;
        if (__atomic_load_n(&pool->shutdownFlag, __ATOMIC_RELAXED)) return NULL;
        uint32_t gint = __atomic_load_n(&pool->currentGint, __ATOMIC_ACQUIRE);
        ComputePermutationChunk(pool, w->participantId, gint);
        __atomic_fetch_add(&pool->workersDone, 1, __ATOMIC_RELEASE);
    }
}

static int ParsePositiveInt(const char *s)
{
    if (!s || !*s) return 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v <= 0 || v > INT_MAX) return 0;
    return (int)v;
}

static int DetectParticipants(void)
{
    int n = ParsePositiveInt(getenv("FAST_CANON_THREADS"));
    if (n > 0) return n;

#ifdef __APPLE__
    int ncpu = 0;
    size_t len = sizeof(ncpu);
    if (sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0) == 0 && ncpu > 0) return ncpu;
#endif

    long online = sysconf(_SC_NPROCESSORS_ONLN);
    if (online > 0 && online <= INT_MAX) return (int)online;
    return 1;
}

static void InitPermutePool(PermutePool *pool, int participants)
{
    pool->participants = participants;
    pool->numPermutations = f;
    pool->lut = permLUT;
    pool->out = permutedNums;
    pool->currentGint = 0;
    pool->workEpoch = 0;
    pool->workersDone = 0;
    pool->workersReady = 0;
    pool->shutdownFlag = 0;
    pool->threads = NULL;
    pool->workerCount = participants - 1;

    if (participants <= 1) return;

    pool->threads = (pthread_t *)xmalloc((size_t)pool->workerCount * sizeof(*pool->threads), "worker threads");
    gWorkerArgs = (WorkerArg *)xmalloc((size_t)pool->workerCount * sizeof(*gWorkerArgs), "worker args");

    for (int i = 0; i < pool->workerCount; ++i) {
        gWorkerArgs[i].pool = pool;
        gWorkerArgs[i].participantId = i + 1;
        if (pthread_create(&pool->threads[i], NULL, PermutationWorker, &gWorkerArgs[i]) != 0) {
            fprintf(stderr, "fast-canon-map: failed to create worker thread %d\n", i);
            exit(1);
        }
    }
    while (__atomic_load_n(&pool->workersReady, __ATOMIC_ACQUIRE) < pool->workerCount) CpuRelax();
}

static void DestroyPermutePool(PermutePool *pool)
{
    if (pool->participants <= 1) return;

    __atomic_store_n(&pool->shutdownFlag, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&pool->workEpoch, 1, __ATOMIC_RELEASE); // wake workers

    for (int i = 0; i < pool->workerCount; ++i) pthread_join(pool->threads[i], NULL);

    free(pool->threads);
    pool->threads = NULL;

    free(gWorkerArgs);
    gWorkerArgs = NULL;
}

static inline void ComputePermutationsForCanonical(PermutePool *pool, uint32_t gint)
{
    if (pool->participants <= 1) {
        ComputePermutationChunk(pool, 0, gint);
        return;
    }

    __atomic_store_n(&pool->workersDone, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&pool->currentGint, gint, __ATOMIC_RELEASE);
    __atomic_fetch_add(&pool->workEpoch, 1, __ATOMIC_RELEASE);
    ComputePermutationChunk(pool, 0, gint);
    while (__atomic_load_n(&pool->workersDone, __ATOMIC_ACQUIRE) < pool->workerCount) CpuRelax();
}

static inline uint16_t NextHashEpoch(void)
{
    ++hashEpoch;
    if (hashEpoch == 0) {
        memset(hashEpochs, 0, HASH_SIZE * sizeof(*hashEpochs));
        hashEpoch = 1;
    }
    return hashEpoch;
}

static inline void HashInsertFirstSeen(uint32_t num, uint16_t nP, uint16_t epoch, uint16_t *usedCount)
{
    uint32_t idx = (num * 2654435761u) & HASH_MASK;
    while (hashEpochs[idx] == epoch) {
        if (hashKeys[idx] == num) return; // already inserted by smaller nP
        idx = (idx + 1u) & HASH_MASK;
    }

    hashEpochs[idx] = epoch;
    hashKeys[idx] = num;
    hashFirstPerm[idx] = nP;
    usedSlots[(*usedCount)++] = (uint16_t)idx;
}

static unsigned ComputeCanonMap(void)
{
    canonicalDecimal[0] = 0;
    DoneSet(0);
    packedData[0] = 0;

    unsigned numCanon = 0;
    uint64_t lastMask = ~0ULL;
    unsigned long remainder = numBitValues & 63UL;
    if (remainder) lastMask = (1ULL << remainder) - 1ULL;

    for (size_t w = 0; w < doneWordCount; ++w) {
        for (;;) {
            uint64_t notDone = ~doneBits[w];
            if (w + 1 == doneWordCount) notDone &= lastMask;
            if (notDone == 0) break;

            unsigned bit = (unsigned)__builtin_ctzll(notDone);
            uint32_t t = (uint32_t)(((unsigned long)w << 6) + (unsigned long)bit);

            DoneSet(t); // this is the next canonical by ascending construction
            ++numCanon;
            if (numCanon >= MAX_CANONICALS_K8) {
                fprintf(stderr, "fast-canon-map: canonical count exceeded expected k=8 limit\n");
                exit(1);
            }
            canonicalDecimal[numCanon] = t;
            packedData[t] = numCanon; // permutation index 0

            ComputePermutationsForCanonical(&gPool, t);

            // Deduplicate images of this canonical locally, preserving first nP.
            uint16_t epoch = NextHashEpoch();
            uint16_t usedCount = 0;
            for (int nP = 1; nP < f; ++nP) {
                uint32_t num = permutedNums[nP];
                if (num == t) continue; // automorphism to self
                HashInsertFirstSeen(num, (uint16_t)nP, epoch, &usedCount);
            }

            // Classes are disjoint, so every inserted num is new globally.
            for (uint16_t i = 0; i < usedCount; ++i) {
                uint32_t idx = usedSlots[i];
                uint32_t num = hashKeys[idx];
                DoneSet(num);
                packedData[num] = ((uint32_t)hashFirstPerm[idx] << CANON_BITS) | numCanon;
            }
        }
    }

    return numCanon;
}

static void InvertPermutationsIfNeeded(void)
{
    if (!PERMS_CAN2NON) return;

    int inv[FAST_CANON_MAX_K];
    for (int p = 0; p < f; ++p) {
        uint8_t *perm = permutations + (size_t)p * (size_t)k;
        for (int j = 0; j < k; ++j) inv[perm[j]] = j;
        for (int j = 0; j < k; ++j) perm[j] = (uint8_t)inv[j];
    }
}

static void BuildPermutationStrings(void)
{
    for (int p = 0; p < f; ++p) {
        const uint8_t *perm = permutations + (size_t)p * (size_t)k;
        char *dst = permStrings + (size_t)p * ((size_t)k + 1u);
        for (int j = 0; j < k; ++j) dst[j] = (char)('0' + perm[j]);
        dst[k] = '\0';
    }
}

static void WriteCanonMap(unsigned numCanon)
{
    FILE *fcanon = stdout;
    setvbuf(fcanon, NULL, _IOFBF, 1u << 20);

    typedef struct {
        char text[24];
        uint8_t len;
    } CanonPrefix;
    CanonPrefix *canonPrefixes = (CanonPrefix *)xmalloc((size_t)(numCanon + 1u) * sizeof(*canonPrefixes), "canonical prefixes");
    for (unsigned i = 0; i <= numCanon; ++i) {
        int len = snprintf(canonPrefixes[i].text, sizeof(canonPrefixes[i].text), "%lu\t", canonicalDecimal[i]);
        if (len <= 0 || len >= (int)sizeof(canonPrefixes[i].text)) {
            fprintf(stderr, "fast-canon-map: internal error building canonical prefixes\n");
            exit(1);
        }
        canonPrefixes[i].len = (uint8_t)len;
    }

    const size_t outCap = 1u << 22; // 4MB buffered streaming for non-canonical rows
    char *outBuf = (char *)xmalloc(outCap, "output buffer");
    size_t outLen = 0;

    TINY_GRAPH *G = TinyGraphAlloc(k);
    int nodeArray[FAST_CANON_MAX_K], distArray[FAST_CANON_MAX_K];

    for (unsigned long i = 0; i < numBitValues; ++i) {
        uint32_t packed = packedData[i];
        uint32_t canonDec = packed & CANON_MASK;
        uint32_t canonPerm = packed >> CANON_BITS;
        const char *permStr = permStrings + (size_t)canonPerm * ((size_t)k + 1u);

        if (canonPerm != 0) {
            const CanonPrefix *prefix = &canonPrefixes[canonDec];
            size_t need = (size_t)prefix->len + (size_t)k + 1u;
            if (outLen + need > outCap) {
                fwrite(outBuf, 1, outLen, fcanon);
                outLen = 0;
            }
            memcpy(outBuf + outLen, prefix->text, prefix->len);
            outLen += prefix->len;
            memcpy(outBuf + outLen, permStr, (size_t)k);
            outLen += (size_t)k;
            outBuf[outLen++] = '\n';
            continue;
        }

        if (outLen) {
            fwrite(outBuf, 1, outLen, fcanon);
            outLen = 0;
        }

        fprintf(fcanon, "%lu\t%s", canonicalDecimal[canonDec], permStr);
        {
            TinyGraphEdgesAllDelete(G);
            Int2TinyGraph(G, (Gint_type)i);
            int connected = (TinyGraphBFS(G, 0, k, nodeArray, distArray) == k);
            fprintf(fcanon, "\t%c %d", (char)('0' + connected), TinyGraphNumEdges(G));
            int sep = '\t';
            for (int u = 0; u < k; ++u) for (int v = u + 1; v < k; ++v) if (TinyGraphAreConnected(G, u, v)) {
                fprintf(fcanon, "%c%d,%d", sep, u, v);
                sep = ' ';
            }
        }
        putc('\n', fcanon);
    }

    if (outLen) fwrite(outBuf, 1, outLen, fcanon);

    TinyGraphFree(G);
    free(outBuf);
    free(canonPrefixes);
}

static char USAGE[] = "USAGE: $0 k";

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "expecting exactly one argument, which is k\n%s\n", USAGE);
        return 1;
    }

    k = atoi(argv[1]);
    if (k < 1 || k > FAST_CANON_MAX_K) {
        fprintf(stderr, "fast-canon-map currently supports 1 <= k <= %d\n", FAST_CANON_MAX_K);
        return 1;
    }

    numBits = (k * (k - 1)) / 2;
    if (numBits >= (int)(sizeof(unsigned long) * CHAR_BIT)) {
        fprintf(stderr, "k=%d is too large for this build\n", k);
        return 1;
    }
    numBitValues = (1UL << numBits);
    f = factorial_int(k);

    packedData = (uint32_t *)xmalloc((size_t)numBitValues * sizeof(*packedData), "packedData");
    doneWordCount = ((size_t)numBitValues + 63u) >> 6;
    doneBits = (uint64_t *)xcalloc(doneWordCount, sizeof(*doneBits), "doneBits");

    permutations = (uint8_t *)xmalloc((size_t)f * (size_t)k * sizeof(*permutations), "permutations");
    permStrings = (char *)xmalloc((size_t)f * ((size_t)k + 1u) * sizeof(*permStrings), "permutation strings");
    permLUT = (uint32_t *)xmalloc((size_t)f * LUT_WORDS_PER_PERM * sizeof(*permLUT), "permutation LUT");
    permutedNums = (uint32_t *)xmalloc((size_t)f * sizeof(*permutedNums), "permuted numbers scratch");

    hashKeys = (uint32_t *)xmalloc(HASH_SIZE * sizeof(*hashKeys), "hash keys");
    hashFirstPerm = (uint16_t *)xmalloc(HASH_SIZE * sizeof(*hashFirstPerm), "hash values");
    hashEpochs = (uint16_t *)xcalloc(HASH_SIZE, sizeof(*hashEpochs), "hash epochs");
    usedSlots = (uint16_t *)xmalloc((size_t)f * sizeof(*usedSlots), "hash used slots");

    BuildPermutations();
    BuildEdgeMaps();
    BuildPermutationLUT();

    int participants = DetectParticipants();
    if (f <= 1) participants = 1;
    else if (participants > (f - 1)) participants = f - 1;
    if (participants < 1) participants = 1;

    InitPermutePool(&gPool, participants);
    unsigned numCanon = ComputeCanonMap();
    DestroyPermutePool(&gPool);

    // We generated permutations in noncanonical->canonical direction during computation.
    // For output, invert to canonical->noncanonical when requested by build settings.
    InvertPermutationsIfNeeded();
    BuildPermutationStrings();

    fprintf(stderr, "Finished computing... now writing out canon_map file\n");
    fflush(stderr);
    WriteCanonMap(numCanon);

    free(usedSlots);
    free(hashEpochs);
    free(hashFirstPerm);
    free(hashKeys);
    free(permutedNums);
    free(permLUT);
    free(permStrings);
    free(permutations);
    free(doneBits);
    free(packedData);

    return 0;
}
