# BLANT k=9/k=10 Extension: Full Implementation Plan

## Goal

Extend BLANT to support **k=9 and k=10** graphlet sampling on commodity hardware (8+ GB RAM). The existing flat lookup table approach (which stores one entry per possible adjacency matrix) cannot scale beyond k=8 on normal machines. The solution integrates the **nauty** library for on-the-fly canonical labeling, replacing the flat table with a compact hash/binary-search lookup.

- **k=9** (primary target): 274,668 canonicals, ~4.4 MB hash table, ~50 MB total RAM
- **k=10** (stretch target): 12,005,168 canonicals, ~192 MB hash table, ~2.4 GB total RAM

The implementation is parameterized by k — the same nauty-based code path handles k=9, k=10, and in principle any higher k, with no k-specific logic.

## Motivation

The BLANT Part 2 paper (Hayes et al., 2022, arXiv:2207.04351) demonstrates that **k=9 seeds produce the best local network alignments** in the BLANT-extend pipeline. Section 5.2 shows that alignment quality (measured by S3, EC, and LCCS) peaks at k=8-9 for seed mining, with k=9 seeds yielding superior alignment coverage on the BioGRID yeast-human network pair. However, the paper acknowledges that k=9 support required a 256+ GB server ("odin") to host the brute-force lookup table — making k=9 inaccessible to typical users.

By replacing the flat table with nauty-based canonical labeling, this extension makes k=9 practical on any laptop and **enables k=10 for the first time ever** — something the flat-table approach can never achieve (140 TB table).

## Prior Work in the BLANT Codebase

Wayne Hayes and students have made several attempts at k>8 support, but none produced a usable solution for normal machines:

| Date | Commit | What Happened |
|---|---|---|
| 2018-03-27 | `36f41459` | "Trying to get k=9 to work" — modified `create-canon-map.c` to use 64-bit integers, hardcoded `canonicalDecimal[274668]`. Still brute-force. |
| 2023-10-28 | `04d67b05` | Added `DYNAMIC_CANON_MAP` flag with `FIXME: this is where we need to insert the search`. Used `BINTREE` but **cheats by falling back to `_K[Gint]`** (the flat table). |
| 2024-01-02 | `9a0e25d9` | Tested `DYNAMIC_CANON_MAP` with BinTree ("MUCH faster than hash map") but still requires the flat table as oracle. |
| 2024-01-18 | `a83ccade` | Turned off `DYNAMIC_CANON_MAP` by default — "let's keep it off to be safe". |
| 2024-08-01 | `7f34e240` | Major cleanup: eliminated `K_GE_9` in favor of `MAX_K`, fixed `orbitpair_bits` for k=10, added `MAX_CANONICALS` for k=9 through k=12. |
| 2024-09-13 | `4b141652` | **"set MAX_K to 9, which now works on odin"** — simply changed the `MAX_K` define. Requires a 275 GB `canon_map9.bin` on a server with 256+ GB RAM. |
| 2024-09-15 | `5ab39e85` | **Two days later: reverted MAX_K back to 8.** Nobody else could use it. |

**Key insight:** The codebase already has the *constants* for k=9 through k=12 (canonical counts, orbit counts, type widths), but the **runtime mechanism** — actually computing canonical forms without a 275 GB flat table — was never implemented. The `DYNAMIC_CANON_MAP` path has a literal `// FIXME` marking where the nauty integration should go. Our plan fills exactly this gap.

## The Problem in Numbers

| k | Adjacency bits | Flat table entries | Flat table size | Canonicals | Hash table size | Status |
|---|---|---|---|---|---|---|
| 7 | 21 | 2M | 4 MB | 1,044 | — | Works (flat table) |
| 8 | 28 | 268M | 536 MB | 12,346 | — | Works (flat table, ~1hr to generate) |
| **9** | **36** | **68.7 billion** | **275 GB** | **274,668** | **4.4 MB** | **Only works on 256GB server; target of this plan** |
| **10** | **45** | **35.2 trillion** | **140 TB** | **12,005,168** | **192 MB** | **Impossible with flat table; stretch target** |
| 11 | 55 | 36 quadrillion | 144 PB | 1,018,997,864 | 16 GB | Future work |
| 12 | 66 | — | — | 165,091,172,592 | — | Far future |

## Architecture: Before and After

### Current (k <= 8): Flat Array Lookup

```
Sample graphlet -> TinyGraph2Int() -> Gint (28-bit for k=8)
                                        |
                                   _K[Gint] -> ordinal     <-- 536 MB mmap'd file
                                        |
                                   Permutations[Gint]       <-- 768 MB mmap'd file
                                        |
                                   ExtractPerm() -> perm[8] (3 bits each, packed in 3 bytes)
```

### New (k > 8): nauty On-the-Fly Canonical Labeling

```
Sample graphlet -> build nauty dense graph (k nodes)
                        |
                   densenauty() -> canonical labeling (lab[] array = the permutation)
                        |
                   Apply lab[] to TINY_GRAPH -> canonical TINY_GRAPH
                        |
                   TinyGraph2Int(canonical) -> canonical_Gint (36-bit for k=9, 45-bit for k=10)
                        |
                   hash_lookup[canonical_Gint] -> ordinal   <-- 274,668 entries for k=9 (~4 MB)
                        |                                       12,005,168 entries for k=10 (~192 MB)
                   lab[] array is already the permutation   <-- no perm_map file needed
```

For k <= 8, the existing flat-table approach is kept unchanged (it's faster). The nauty path is only used when k > 8. The code is fully parameterized — the same functions handle k=9, k=10, or any higher k.

---

## Phase 1: Type Widening (Mechanical, ~50 locations)

### 1.1 Set TINY_SET_SIZE to 16 for k>8 builds

**File: Makefile**

Add `k9` and `k10` targets that compile with `-DTINY_SET_SIZE=16`. This cascades through the type system:
- `TSET` becomes `uint16_t` (was `uint8_t`)
- `MAX_TSET` becomes 16 (was 8)

The existing code in `libwayne/include/sets.h` already handles `TINY_SET_SIZE == 16`:
```c
// sets.h line 206-207
#elif TINY_SET_SIZE == 16
    typedef uint16_t TSET;
```

**Decision: compile-time or runtime?**

Option A (simpler): Build a separate `blant9`/`blant10` binary with `-DTINY_SET_SIZE=16 -DMAX_K=9` (or 10). Keep the existing `blant` binary for k<=8.

Option B (harder): Make k runtime-selectable with dynamic dispatch. Much more invasive.

**Recommendation: Option A.** Build separate binaries. The Makefile already builds separate `create-bin-data3` through `create-bin-data8` binaries. This follows the same pattern. A single `blant-large-k` binary with `-DMAX_K=10` works for both k=9 and k=10 at runtime (k is a CLI argument).

### 1.2 Fix Gint_type and Gordinal_type

**File: `src/blant.h` lines 48-86**

The `#if TINY_SET_SIZE == 16` branch (lines 48-73) already exists but needs adjustment. Currently it tries to use `__uint128` or checks for 120-bit `long_width`. For our targets:

- **k=9:** `Gint_type` needs 36 bits, `Gordinal_type` needs 19 bits (274,668 canonicals)
- **k=10:** `Gint_type` needs 45 bits, `Gordinal_type` needs 24 bits (12,005,168 canonicals)
- Both fit in `uint64_t` / `uint32_t`

Add a cleaner conditional:
```c
#elif TINY_SET_SIZE == 16
  #if MAX_K <= 10
    typedef uint64_t Gint_type;     // 64 bits covers k=10 (needs 45 bits)
    #define GINT_FMT "%lu"
    typedef uint32_t Gordinal_type; // 32 bits covers k=10 (needs 24 bits, max ordinal 12,005,167)
    #define GORDINAL_FMT "%u"
    #define MAX_BINTREE_K 10
  #elif MAX_K <= 11
    // ... existing wider type handling for k=11 (55-bit Gint, 30-bit ordinal)
  #endif
#endif
```

### 1.3 Fix MAX_K definition

**File: `src/blant-fundamentals.h` line 20**

Currently: `#define MAX_K (8-SELF_LOOPS)` (wrapped in `#ifndef MAX_K`)

For k>8 builds: define `MAX_K` via the Makefile (`-DMAX_K=10`). The `#ifndef` guard already present means the Makefile `-D` override takes precedence. A single binary with `MAX_K=10` supports runtime k=3 through k=10.

The downstream constants already handle this:
```c
// blant-fundamentals.h (already present)
#elif MAX_K == 9
  #define MAX_CANONICALS  274668
  #define MAX_ORBITS      2208612
#elif MAX_K == 10
  #define MAX_CANONICALS  12005168
  #define MAX_ORBITS      113743760
```

### 1.4 Fix kperm encoding (3 bytes -> 5 bytes)

**Files affected (5 total):**
- `src/blant-utils.c` (lines 14, 182-188)
- `src/create-bin-data.c` (lines 37, 44-58)
- `src/test-bin-data.c` (lines 23, 30-36)
- `src/synthetic.c` (line 26)
- `src/blant-predict-release.c` (line 76)

**Current encoding (k<=8):** 3 bits per node x 8 nodes = 24 bits = 3 bytes
```c
typedef unsigned char kperm[3];
// Extract: perm[j] = (i32 >> 3*j) & 7
// Encode:  i32 |= (perm[j] << 3*j)
```

**New encoding (k>8):** 4 bits per node x k nodes = 40 bits for k=10 = 5 bytes
```c
typedef unsigned char kperm[5]; // when MAX_K > 8
// Extract: perm[j] = (i64 >> 4*j) & 0xF
// Encode:  i64 |= ((uint64_t)perm[j] << 4*j)
```

Note: 4 bits per node covers node indices 0-15, sufficient for k<=16. Both k=9 (36 bits) and k=10 (40 bits) fit in 5 bytes.

**Implementation:** Make it conditional on `MAX_K`:
```c
#if MAX_K <= 8
  typedef unsigned char kperm[3];
  #define BITS_PER_NODE 3
  #define NODE_MASK 7
  #define KPERM_BYTES 3
#else
  typedef unsigned char kperm[5];
  #define BITS_PER_NODE 4
  #define NODE_MASK 0xF
  #define KPERM_BYTES 5
#endif
```

Then rewrite `ExtractPerm` and `EncodePerm` to use these constants.

**Note for k>8:** When using nauty, permutations come from `lab[]` directly — the kperm encoding is only needed if the `Permutations` mmap path is active (k<=8). For k>8, `Permutations` is NULL and nauty provides permutations on-the-fly, so kperm is unused at runtime. The kperm changes are only needed for compile-time correctness of shared code.

### 1.5 Fix hardcoded k<=8 assertions

**Locations (~10):**
- `src/create-bin-data.c` line 31: `#error define kk as an integer between 3 and 8`
- `src/test-bin-data.c` line 17: same
- `src/slow-canon-maps.c` line 66: `assert(k > 2 && k <= 8)`
- `src/makeEHD.c` line 21: `assert(3 <= k && k <= 8)`
- Various `assert(k <= MAX_K)` calls (these are fine if MAX_K is increased)

### 1.6 Fix `maxBk` overflow

**File: `src/blant-fundamentals.h` line 23**

Currently: `#define maxBk (1U << (8*(8-1)/2 + 8*SELF_LOOPS))`

This is hardcoded for k=8. For k=9: `1UL << 36` which needs `unsigned long`.

Change to: `#define maxBk (1UL << (MAX_K*(MAX_K-1)/2 + MAX_K*SELF_LOOPS))`

**Warning:** For k=9, `maxBk = 2^36 = 68,719,476,736`. Arrays of size `maxBk` will NOT be allocated (that's the whole point of this effort). This constant is only used for the k<=8 flat-table path.

---

## Phase 2: Integrate nauty (Core Algorithmic Change)

### 2.1 Add nauty source to the repo

Download nauty 2.8.9 (or latest) from https://pallini.di.uniroma1.it/

Files needed (minimal set for dense graph canonical labeling):
```
nauty/nauty.h
nauty/nauty.c
nauty/nautil.h
nauty/nautil.c
nauty/nausparse.h
nauty/nausparse.c
nauty/schreier.h
nauty/schreier.c
nauty/naurng.h
nauty/naurng.c
nauty/gtools.h
nauty/gtools.c        (for geng integration, optional)
nauty/naututil.h
nauty/naututil.c
nauty/naugroup.h
nauty/naugroup.c
```

Place in `src/nauty/` or `nauty/` subdirectory.

Add to Makefile: compile nauty objects, link into blant9.

### 2.2 Write the nauty canonical wrapper

**New file: `src/nauty-canonical.c`**

```c
#include "nauty/nauty.h"
#include "blant.h"

// Thread-local nauty workspace to avoid repeated allocation
static _Thread_local int *nauty_lab, *nauty_ptn, *nauty_orbits;
static _Thread_local graph *nauty_g, *nauty_cg;
static _Thread_local Boolean nauty_initialized = false;

static void nauty_init(int k) {
    if (nauty_initialized) return;
    int m = SETWORDSNEEDED(k);
    nauty_lab = calloc(k, sizeof(int));
    nauty_ptn = calloc(k, sizeof(int));
    nauty_orbits = calloc(k, sizeof(int));
    nauty_g = calloc(m * k, sizeof(graph));
    nauty_cg = calloc(m * k, sizeof(graph));
    nauty_initialized = true;
}

// Compute canonical form of a TINY_GRAPH using nauty.
// Returns the canonical Gint and fills perm[] with the canonical labeling.
Gint_type NautyCanonical(TINY_GRAPH *tg, int k, unsigned char perm[]) {
    nauty_init(k);
    int m = SETWORDSNEEDED(k);

    // Convert TINY_GRAPH to nauty dense graph format
    EMPTYGRAPH(nauty_g, m, k);
    for (int i = 0; i < k; i++)
        for (int j = i+1; j < k; j++)
            if (TinyGraphAreConnected(tg, i, j)) {
                ADDONEEDGE(nauty_g, i, j, m);
            }

    // Run nauty
    DEFAULTOPTIONS_GRAPH(options);
    options.getcanon = TRUE;
    statsblk stats;
    densenauty(nauty_g, nauty_lab, nauty_ptn, nauty_orbits,
               &options, &stats, m, k, nauty_cg);

    // nauty_lab[i] = "which original node goes to position i in the canonical form"
    // We need the inverse: "original node j goes to canonical position inv_lab[j]"
    unsigned char inv_lab[k];
    for (int i = 0; i < k; i++)
        inv_lab[nauty_lab[i]] = i;

    // Build canonical TINY_GRAPH and compute its Gint
    TINY_GRAPH *canon_tg = TinyGraphAlloc(k);
    for (int i = 0; i < k; i++)
        for (int j = i+1; j < k; j++)
            if (TinyGraphAreConnected(tg, i, j))
                TinyGraphConnect(canon_tg, inv_lab[i], inv_lab[j]);

    Gint_type canon_gint = TinyGraph2Int(canon_tg, k);
    TinyGraphFree(canon_tg);

    // Fill perm[] with the canonical-to-noncanonical mapping
    // (BLANT convention: perm maps canonical node positions to original positions)
    for (int i = 0; i < k; i++)
        perm[i] = nauty_lab[i];  // canonical position i came from original node lab[i]

    return canon_gint;
}
```

**CRITICAL NOTE:** The perm convention (can2non vs non2can) MUST match BLANT's `PERMS_CAN2NON` setting. Verify against known k<=8 results.

### 2.3 Build the canonical ordinal hash table

**New file: `src/canon-hash.c`**

At startup (when k=9), load `canon_list9.txt` into a hash table mapping `canonical_Gint -> ordinal`.

```c
#include "uthash.h"  // already in BLANT's source tree

typedef struct {
    Gint_type canon_gint;   // key
    Gordinal_type ordinal;  // value
    UT_hash_handle hh;
} CanonHashEntry;

static CanonHashEntry *canon_hash = NULL;

void BuildCanonHash(Gint_type *canon_list, Gordinal_type numCanon) {
    for (Gordinal_type i = 0; i < numCanon; i++) {
        CanonHashEntry *entry = malloc(sizeof(CanonHashEntry));
        entry->canon_gint = canon_list[i];
        entry->ordinal = i;
        HASH_ADD(hh, canon_hash, canon_gint, sizeof(Gint_type), entry);
    }
}

Gordinal_type CanonHashLookup(Gint_type canon_gint) {
    CanonHashEntry *entry;
    HASH_FIND(hh, canon_hash, &canon_gint, sizeof(Gint_type), entry);
    assert(entry != NULL);  // every canonical must be in the table
    return entry->ordinal;
}
```

**Alternative:** Since `canon_list` is sorted, binary search (`bsearch`) is simpler and already used in `create-bin-data.c`:
```c
Gordinal_type canon2ordinal(Gordinal_type numCanon, Gint_type *canon_list, Gint_type canonical) {
    Gint_type *found = bsearch(&canonical, canon_list, numCanon, sizeof(canon_list[0]), siCmp);
    return found - canon_list;
}
```
Binary search over 274,668 entries = ~18 comparisons. At ~5ns each = ~90ns. Perfectly fine.

### 2.4 Replace `L_K(Gint)` for k>8

**File: `src/blant.h` lines 131-134**

Currently:
```c
#define L_K(Gint) (_K ? _K[Gint] : L_K_Func(Gint))
```

Change to:
```c
#if MAX_K <= 8
  #define L_K(Gint) (_K ? _K[Gint] : L_K_Func(Gint))
#else
  // For k>8, flat table doesn't exist; use nauty
  Gordinal_type L_K_nauty(Gint_type Gint);
  #define L_K(Gint) (_K ? _K[Gint] : L_K_nauty(Gint))
#endif
```

Where `L_K_nauty` does:
1. `Int2TinyGraph(tg, Gint)` to reconstruct the TINY_GRAPH
2. `NautyCanonical(tg, k, perm)` to get canonical Gint
3. `canon2ordinal(numCanon, canon_list, canonical_gint)` to get ordinal

### 2.5 Replace `ExtractPerm()` for k>8

**File: `src/blant-utils.c` line 179-188**

Currently:
```c
Gordinal_type ExtractPerm(unsigned char perm[_k], Gint_type Gint) {
    if(Permutations) {
        // ... extract from mmap'd perm_map.bin
    } else return canon_to_ordinal(smaller_canon_map(Gint, _k, perm), _k);
}
```

For k>8, `Permutations` will be NULL (no perm_map9.bin exists). The fallback path calls `canon_to_ordinal(smaller_canon_map(...))` which is unimplemented (stubs that `Fatal()`).

**Fix:** When `Permutations` is NULL and k>8, call nauty:
```c
Gordinal_type ExtractPerm(unsigned char perm[_k], Gint_type Gint) {
    if (Permutations) {
        // existing mmap path (k<=8)
        int j;
        uint64_t packed = 0;
        for (j = 0; j < KPERM_BYTES; j++)
            packed |= ((uint64_t)Permutations[Gint][j] << (j*8));
        for (j = 0; j < _k; j++)
            perm[j] = (packed >> (BITS_PER_NODE*j)) & NODE_MASK;
        return _K[Gint];
    } else {
        // nauty path (k>8)
        Gint_type canon_gint = NautyCanonical(/*reconstruct tg from Gint*/, _k, perm);
        return canon2ordinal(_numCanon, _canonList, canon_gint);
    }
}
```

### 2.6 Skip `mapCanonMap()` and perm_map mmap for k>8

**File: `src/blant-utils.c` `SetGlobalCanonMaps()` (line 127-154)**

Currently unconditionally calls `mapCanonMap()` (line 141) and mmaps perm_map (line 145). For k>8, these files don't exist and the arrays would be 275+ GB.

Add guards:
```c
if (_k <= 8) {
    _K = (Gordinal_type*) mapCanonMap(BUF, _K, _k);
    // ... mmap Permutations ...
} else {
    _K = NULL;          // signal to use nauty path
    Permutations = NULL; // signal to use nauty path
}
```

---

## Phase 3: Generate Reference Files (k=9 and k=10)

The generator programs are **parameterized by k** — the same `generate-canon-list` and `generate-orbit-map` programs produce output for any k. They take k as a command-line argument.

### 3.1 Generate `canon_list{k}.txt`

**Format** (from existing files):
```
<numCanonicals>
<decimal_gint>\t<connected: 0|1> <num_edges>[\t<edge_list>]
...
```

**How to generate:** Use nauty's `geng` tool to enumerate all non-isomorphic graphs on k vertices, then for each:
1. Convert to TINY_GRAPH
2. Compute `TinyGraph2Int()` to get the canonical Gint
3. Check connectivity via BFS
4. Count edges

```bash
# geng generates all non-isomorphic graphs on n vertices
nauty/geng 9 | ./generate-canon-list 9 > canon_maps/canon_list9.txt   # seconds
nauty/geng 10 | ./generate-canon-list 10 > canon_maps/canon_list10.txt # minutes
```

**Expected output:**

| k | Canonicals | File size | Generation time |
|---|---|---|---|
| 9 | 274,668 | ~19 MB | ~5-30 seconds |
| 10 | 12,005,168 | ~300 MB | ~1-5 minutes |

### 3.2 Generate `orbit_map{k}.txt`

**Format:**
```
<numOrbits>
<orbit_id_for_node_0> <orbit_id_for_node_1> ... <orbit_id_for_node_k-1>
... (one line per canonical)
```

Orbits are determined by the automorphism group of each canonical graphlet. nauty computes automorphism groups as part of `densenauty()` — the `orbits[]` output array gives exactly this.

**How to generate:** For each canonical graphlet:
1. Run `densenauty()` to get `orbits[]`
2. Assign global orbit IDs based on (canonical_ordinal, local_orbit_id)

The existing `make-orbit-maps.c` does this by iterating permutations — infeasible for k>8 (362,880 perms x 274,668 canonicals). nauty's automorphism group computation replaces this and is much faster.

**Expected output:**

| k | Orbits | File size | Generation time |
|---|---|---|---|
| 9 | 2,208,612 | ~17 MB | ~10-60 seconds |
| 10 | 113,743,760 | ~1.1 GB | ~5-15 minutes |

### 3.3 Generate `canon_map{k}.txt` and `canon_map{k}.bin` / `perm_map{k}.bin`

**Skip all of these for k>8.** The flat `canon_map` format has one line per possible adjacency matrix — 68.7 billion lines for k=9, 35.2 trillion for k=10. The flat binary files (`canon_map{k}.bin`, `perm_map{k}.bin`) would be 275 GB / 140 TB. The nauty runtime path + hash table replaces all of them.

### 3.4 Generate alpha correction files

**Files:** `alpha_list_{NBE,EBE,MCMC}{k}.txt` for k=9 and k=10.

These contain sampling bias correction factors for each canonical. Generated by the existing `compute-alphas-{NBE,EBE,MCMC}` programs.

**These programs need the type widening from Phase 1** to compile for k>8. Once they compile, they can be run on test networks to compute the alpha values.

**Note:** These programs load the canon_map via mmap. For k>8, they need to be modified to use the nauty path instead.

### 3.5 Generate `canon-ordinal-to-signature{k}.txt`

Contains the "signature" (edge pattern) for each canonical. Can be generated alongside `canon_list{k}.txt`.

### 3.6 k=10 Resource Budget

Running BLANT at k=10 requires more memory than k=9. Here is the breakdown:

| Resource | k=9 | k=10 | Notes |
|---|---|---|---|
| Hash table (canon->ordinal) | 4.4 MB | 192 MB | Sorted array + bsearch also works |
| `_canonList[MAX_CANONICALS]` | 2.2 MB | 96 MB | `Gint_type` (8 bytes) per entry |
| `_orbitList[MAX_CANONICALS][k]` | 20 MB | 960 MB | Largest single array |
| `_orbitCanonMapping[MAX_ORBITS]` | 8.8 MB | 455 MB | `Gordinal_type` (4 bytes) |
| `_orbitDegreeVector` pointers | 17.6 MB | 910 MB | Per-node per-orbit (GDV/ODV mode) |
| **Total static overhead** | **~53 MB** | **~2.6 GB** | |
| Per-sample nauty call | ~2-5 μs | ~5-20 μs | Negligible vs graph construction |

k=10 fits comfortably on any machine with 4+ GB RAM (not all arrays are allocated simultaneously; GDV/ODV arrays are only allocated in those output modes).

---

## Phase 4: Makefile Changes

### 4.1 Add nauty build targets

```makefile
# nauty library
NAUTY_DIR = src/nauty
NAUTY_SRCS = nauty.c nautil.c nausparse.c schreier.c naurng.c naugroup.c naututil.c
NAUTY_OBJS = $(addprefix $(OBJDIR)/, $(NAUTY_SRCS:.c=.o))

$(OBJDIR)/%.o: $(NAUTY_DIR)/%.c
    $(GCC) -O3 -c $< -o $@ -I $(NAUTY_DIR) -DWORDSIZE=64
```

### 4.2 Add k>8 build target

A single `blant-large-k` binary with `MAX_K=10` handles both k=9 and k=10 at runtime (k is a command-line argument). The only build-time distinction is `TINY_SET_SIZE=16` and `MAX_K=10`.

```makefile
# Build BLANT with k>8 support (uses nauty for canonical labeling)
LARGE_K_CFLAGS = -DTINY_SET_SIZE=16 -DMAX_K=10
blant-large-k: libwayne canon_maps/canon_list9.txt canon_maps/orbit_map9.txt $(NAUTY_OBJS)
    $(CC) $(LARGE_K_CFLAGS) -o $@ $(BLANT_SRCS) $(NAUTY_OBJS) $(LIBWAYNE_BOTH)

# Convenience aliases
blant9: blant-large-k
    ln -sf blant-large-k blant9
blant10: blant-large-k canon_maps/canon_list10.txt canon_maps/orbit_map10.txt
    ln -sf blant-large-k blant10

.PHONY: k9 k10
k9: blant9 canon_maps/canon_list9.txt canon_maps/orbit_map9.txt
k10: blant10 canon_maps/canon_list10.txt canon_maps/orbit_map10.txt
```

### 4.3 Add reference file generation targets

The generator programs are parameterized — they take k as an argument:

```makefile
# Canon list generation (uses geng + nauty)
canon_maps/canon_list%.txt: generate-canon-list
    nauty/geng $* | ./generate-canon-list $* > $@

# Orbit map generation (uses nauty automorphism groups)
canon_maps/orbit_map%.txt: generate-orbit-map canon_maps/canon_list%.txt
    ./generate-orbit-map $* < canon_maps/canon_list$*.txt > $@

generate-canon-list: src/generate-canon-list.c $(NAUTY_OBJS)
    $(CC) $(LARGE_K_CFLAGS) -o $@ $< $(NAUTY_OBJS) $(LIBWAYNE_BOTH)

generate-orbit-map: src/generate-orbit-map.c $(NAUTY_OBJS)
    $(CC) $(LARGE_K_CFLAGS) -o $@ $< $(NAUTY_OBJS) $(LIBWAYNE_BOTH)
```

---

## Phase 5: Testing and Verification

No one has implemented k=9/k=10 graphlet sampling on commodity hardware before. The testing strategy uses **seven independent verification tiers** that together provide very high confidence without needing a k=9 or k=10 oracle.

### Tier 1: Mathematical Constants (instant, deterministic)

The number of non-isomorphic graphs on n vertices is a **known combinatorial constant** independently computed by mathematicians and published in the OEIS.

| What | OEIS Sequence | k=9 Value | k=10 Value | Test |
|---|---|---|---|---|
| Total non-isomorphic graphs | A000088 | 274,668 | 12,005,168 | Count lines in `canon_list{k}.txt` |
| Connected non-isomorphic graphs | A001349 | 261,080 | 11,716,571 | Count connected entries |
| Total orbits | (from `blant-fundamentals.h`) | 2,208,612 | 113,743,760 | Parse header of `orbit_map{k}.txt` |

If the implementation produces exactly the right canonical/connected counts, it is almost certainly correct — these numbers are extremely hard to hit by accident.

```python
EXPECTED = {
    9:  {"canonicals": 274668,   "connected": 261080,   "orbits": 2208612},
    10: {"canonicals": 12005168, "connected": 11716571, "orbits": 113743760},
}

def test_canonical_count_k9():
    _check_canonical_count(9)

def test_canonical_count_k10():
    _check_canonical_count(10)

def _check_canonical_count(k):
    lines = open(f"/app/canon_maps/canon_list{k}.txt").readlines()
    num_canon = int(lines[0].strip())
    assert num_canon == EXPECTED[k]["canonicals"]
    assert len(lines) == EXPECTED[k]["canonicals"] + 1

def _check_connected_count(k):
    lines = open(f"/app/canon_maps/canon_list{k}.txt").readlines()[1:]
    connected = sum(1 for line in lines if line.split('\t')[1].startswith('1'))
    assert connected == EXPECTED[k]["connected"]

def _check_orbit_count(k):
    header = open(f"/app/canon_maps/orbit_map{k}.txt").readline().strip()
    assert int(header) == EXPECTED[k]["orbits"]
```

### Tier 2: Cross-Validation Against Flat Table for k<=8 (exhaustive)

This is the strongest correctness test. For k=3 through k=8, BLANT has a **known-correct flat lookup table** (used for years). Test that the nauty-based code path gives the **identical ordinal** for every possible input.

```python
def test_nauty_vs_flat_k5():
    """For ALL 1,024 possible k=5 graphlets, nauty must match the flat table."""
    # Runs blant9 in cross-validation mode: for each Gint 0..1023,
    # computes ordinal via nauty AND via flat table, asserts equality
    result = subprocess.run(
        ["./cross-validate-nauty", "5"],
        capture_output=True, timeout=30
    )
    assert result.returncode == 0

def test_nauty_vs_flat_k6():
    """For ALL 32,768 possible k=6 graphlets."""
    # ~1 second

def test_nauty_vs_flat_k7():
    """For ALL 2,097,152 possible k=7 graphlets."""
    # ~10 seconds

def test_nauty_vs_flat_k8():
    """For ALL 268,435,456 possible k=8 graphlets."""
    # ~5 minutes — but exhaustive and bulletproof
```

The key insight: if nauty agrees with the flat table on all 268 million k=8 inputs, the canonical labeling code is correct. Then k=9 uses the **exact same code path** — just with k=9 inputs. This is the single most important test.

The `cross-validate-nauty` program is a small C tool built during Dockerfile setup:
```c
// For every Gint in 0..2^(k*(k-1)/2)-1:
//   ordinal_flat  = _K[Gint]          (from mmap'd canon_map.bin)
//   ordinal_nauty = NautyCanonical(Gint) -> hash_lookup
//   assert(ordinal_flat == ordinal_nauty)
```

### Tier 3: Self-Consistency Tests (probabilistic, no reference needed)

These verify structural properties that **must** hold if the implementation is correct, without needing any external reference for k=9.

```python
def test_canonical_is_idempotent():
    """The canonical form of a canonical form must be itself.
    For all 274,668 canonicals: L_K(canon_list[i]) == i."""
    result = subprocess.run(
        ["./test-idempotent", "9"],
        capture_output=True, timeout=120
    )
    assert result.returncode == 0

def test_isomorphism_invariant():
    """Randomly permute nodes of a graphlet; ordinal must not change.
    Repeat 1,000,000 times with random graphlets and random permutations."""
    result = subprocess.run(
        ["./test-isomorphism-invariant", "9", "1000000"],
        capture_output=True, timeout=120
    )
    assert result.returncode == 0

def test_canonical_is_minimum():
    """The canonical Gint must be the minimum over all node permutations.
    For 10,000 random graphlets, try 10,000 random permutations each and
    verify none produces a smaller Gint than the canonical."""
    result = subprocess.run(
        ["./test-canonical-minimum", "9", "10000", "10000"],
        capture_output=True, timeout=300
    )
    assert result.returncode == 0

def test_unique_canonicals():
    """No two distinct ordinals in canon_list9.txt share the same Gint value."""
    lines = open("/app/canon_maps/canon_list9.txt").readlines()[1:]
    gints = [int(line.split('\t')[0]) for line in lines]
    assert len(gints) == len(set(gints)), "Duplicate canonical Gints found"
```

The **isomorphism invariance test** is particularly powerful: if randomly permuting nodes always gives the same ordinal across 1 million trials, the canonical labeling is correct with overwhelming probability.

### Tier 4: Orbit Structural Properties (deterministic)

Orbits are determined by the automorphism group. These tests verify structural invariants:

```python
def test_orbit_degree_consistency():
    """Nodes assigned to the same orbit within a canonical graphlet must
    have the same degree. This is a necessary (not sufficient) condition."""
    result = subprocess.run(
        ["./test-orbit-degrees", "9"],
        capture_output=True, timeout=60
    )
    assert result.returncode == 0

def test_orbit_assignments_cover_all_nodes():
    """Every canonical must have exactly 9 orbit assignments (one per node)."""
    lines = open("/app/canon_maps/orbit_map9.txt").readlines()
    num_orbits = int(lines[0].strip())
    # Each subsequent line has k=9 space-separated orbit IDs
    for i, line in enumerate(lines[1:], 1):
        orbit_ids = line.strip().split()
        assert len(orbit_ids) == 9, f"Canonical {i-1} has {len(orbit_ids)} orbit assignments, expected 9"
        for oid in orbit_ids:
            assert 0 <= int(oid) < num_orbits, f"Orbit ID {oid} out of range"

def test_edge_count_matches():
    """Edge count reported in canon_list9.txt must match actual edges in the graphlet."""
    # For each canonical: reconstruct TINY_GRAPH, count edges, compare to declared count
    result = subprocess.run(
        ["./test-edge-counts", "9"],
        capture_output=True, timeout=60
    )
    assert result.returncode == 0
```

### Tier 5: Sampling Correctness (statistical)

Run k=9 and k=10 sampling on a real network and verify statistical properties:

```python
def test_k9_sampling_valid_ordinals():
    """All sampled graphlet ordinals must be in range [0, 274667]."""
    output = subprocess.run(
        ["./blant-large-k", "-sNBE", "-k9", "-n100000", "-mf", "networks/syeast.el"],
        capture_output=True, text=True, timeout=120
    )
    for line in output.stdout.strip().split('\n'):
        parts = line.split()
        ordinal = int(parts[0])
        assert 0 <= ordinal < 274668

def test_k10_sampling_valid_ordinals():
    """All sampled graphlet ordinals must be in range [0, 12005167]."""
    output = subprocess.run(
        ["./blant-large-k", "-sNBE", "-k10", "-n10000", "-mf", "networks/syeast.el"],
        capture_output=True, text=True, timeout=300
    )
    for line in output.stdout.strip().split('\n'):
        parts = line.split()
        ordinal = int(parts[0])
        assert 0 <= ordinal < 12005168

def test_k9_connected_dominates():
    """On a connected network, >99% of sampled graphlets should be connected."""
    freqs = parse_frequency_output(run_blant_freq(9, "syeast.el"))
    connected_total = sum(v for k, v in freqs.items() if is_connected_canonical(k))
    total = sum(freqs.values())
    assert connected_total / total > 0.99

def test_k9_k8_subgraph_consistency():
    """Cross-validate k=9 against trusted k=8: every k=9 graphlet contains
    9 subgraphlets of size k=8 (one per removed node). The distribution of
    these k=8 subgraphlets implied by k=9 sampling must approximately match
    the k=8 frequency distribution from direct k=8 sampling.

    This is the deepest consistency check: it verifies k=9 correctness
    by reduction to the trusted k=8 implementation."""
    # 1. Run k=9 sampling, collect 100K graphlets
    # 2. For each k=9 graphlet, remove each of the 9 nodes to get 9 k=8 subgraphlets
    # 3. Look up each k=8 subgraphlet's ordinal using the trusted k=8 flat table
    # 4. Accumulate k=8 frequency distribution
    # 5. Compare against direct k=8 sampling on the same network
    # 6. Assert correlation > 0.95 (exact match not expected due to sampling bias)
    result = subprocess.run(
        ["./test-subgraph-consistency", "9", "networks/syeast.el", "100000"],
        capture_output=True, timeout=300
    )
    assert result.returncode == 0

def test_k10_k9_subgraph_consistency():
    """Same chain-of-trust test: every k=10 graphlet contains 10 k=9 subgraphlets.
    Verifies k=10 by reduction to k=9 (which is verified by reduction to k=8)."""
    result = subprocess.run(
        ["./test-subgraph-consistency", "10", "networks/syeast.el", "10000"],
        capture_output=True, timeout=600
    )
    assert result.returncode == 0
```

The **subgraph consistency test** is the most elegant: it creates a **chain of trust** from k=3 (trivially correct) through k=8 (exhaustively tested flat table) to k=9 to k=10. Each level is verified by reduction to the level below it.

### Tier 6: Regression (k=3 through k=8 unchanged)

The k<=8 code path must remain completely unaffected. All existing regression tests must pass:

```python
def test_k8_regression_freq():
    """k=8 frequency output unchanged from baseline."""
    # Run existing regression-tests/0-sanity/test1_freq.sh
    result = subprocess.run(
        ["bash", "regression-tests/0-sanity/test1_freq.sh"],
        capture_output=True, timeout=120,
        cwd="/app"
    )
    assert result.returncode == 0

def test_k8_regression_gdv():
    """k=8 GDV output unchanged from baseline."""
    result = subprocess.run(
        ["bash", "regression-tests/0-sanity/test2_GDV.sh"],
        capture_output=True, timeout=120,
        cwd="/app"
    )
    assert result.returncode == 0

def test_k8_index_mode():
    """k=8 INDEX mode output byte-identical to baseline."""
    result = subprocess.run(
        ["bash", "canon_maps/test_index_mode"],
        capture_output=True, timeout=120,
        cwd="/app"
    )
    assert result.returncode == 0

def test_k5_canon_maps_unchanged():
    """k=5 canon_map.bin must be byte-identical to original."""
    assert filecmp.cmp(
        "/app/canon_maps/canon_map5.bin",
        "/baselines/canon_maps/canon_map5.bin"
    )
```

### Tier 7: Independent Verification via nauty's `geng`

nauty ships with `geng`, a tool that independently enumerates all non-isomorphic graphs on n vertices. It has been validated by mathematicians for decades. Use it as ground truth:

```bash
# k=9: verify all 274,668 canonical graphs
nauty/geng 9 > all_9node_graphs.g6
wc -l all_9node_graphs.g6   # must be 274,668
./verify-against-geng 9 all_9node_graphs.g6 canon_maps/canon_list9.txt

# k=10: verify all 12,005,168 canonical graphs (takes a few minutes)
nauty/geng 10 > all_10node_graphs.g6
wc -l all_10node_graphs.g6  # must be 12,005,168
./verify-against-geng 10 all_10node_graphs.g6 canon_maps/canon_list10.txt
```

This provides a completely independent check: `geng` enumerates the canonical graphs using nauty's internal algorithms, while BLANT generates them through its own pipeline. If both produce the same Gint values, the canon list is correct.

### Test Summary

| Tier | What It Tests | Reference Needed? | Strength | Applies to |
|---|---|---|---|---|
| 1. Mathematical constants | Canonical/orbit counts | No (OEIS) | Catches gross errors | k=9, k=10 |
| 2. Cross-validate k<=8 | nauty code path correctness | No (uses existing flat tables) | **Exhaustive** for the algorithm | k=3-8 (proves nauty path) |
| 3. Self-consistency | Idempotency, isomorphism invariance, minimality | No (algebraic properties) | Very strong (probabilistic) | k=9, k=10 |
| 4. Orbit structure | Degree consistency, coverage | No (graph theory invariants) | Catches orbit assignment bugs | k=9, k=10 |
| 5. Sampling statistics | Frequency distributions, subgraph consistency | No (statistical + chain of trust) | Catches sampling bugs | k=9, k=10 |
| 6. Regression | k<=8 unchanged | No (existing baselines) | Catches collateral damage | k=3-8 |
| 7. geng cross-check | Canon list completeness | No (nauty geng is independent) | Independent verification | k=9, k=10 |

**No test requires a pre-existing k=9 or k=10 implementation.** Correctness is verified from first principles: mathematical constants, exhaustive cross-validation on k<=8, algebraic invariants, statistical consistency, and independent geng enumeration. The **chain of trust** runs: k=3 (trivial) -> k=4-8 (exhaustive flat-table cross-validation) -> k=9 (subgraph reduction to k=8) -> k=10 (subgraph reduction to k=9).

---

## Estimated Implementation Effort

| Phase | Description | Lines Changed/Added | Time |
|---|---|---|---|
| 1.1-1.3 | Type system (TSET, Gint, Gordinal, MAX_K) | ~30 lines changed | 20 min |
| 1.4 | kperm encoding (5 files) | ~50 lines changed | 20 min |
| 1.5-1.6 | Hardcoded limits, maxBk | ~15 lines changed | 10 min |
| 2.1 | Add nauty source files | 0 lines (file copy) | 10 min |
| 2.2 | nauty canonical wrapper | ~80 lines new | 30 min |
| 2.3 | Canon hash/binary-search table | ~30 lines new | 15 min |
| 2.4-2.6 | Replace L_K, ExtractPerm, SetGlobalCanonMaps | ~40 lines changed | 20 min |
| 3.1-3.2 | Generate canon_list + orbit_map (parameterized) | ~200 lines new (generator programs) | 45 min |
| 3.4 | Modify compute-alphas for k>8 | ~20 lines changed | 15 min |
| 4 | Makefile changes | ~50 lines new | 15 min |
| 5.1-5.7 | Testing and debugging (k=9 + k=10) | ~150 lines (test scripts) | 45-90 min |
| **Total** | | **~650 lines** | **~4-6 hours** |

**k=10 adds ~0-30 minutes** beyond k=9 because:
- The generator programs are parameterized (run with argument `10` instead of `9`)
- The same binary handles both k=9 and k=10 at runtime
- Generation of k=10 reference files takes ~5-15 minutes (vs seconds for k=9)
- Additional tests are parameterized versions of k=9 tests

---

## Risk Register

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| nauty canonical convention doesn't match BLANT's | Medium | High (silent wrong results) | Tier 2 cross-validates exhaustively for all k<=8 entries |
| Permutation direction (can2non vs non2can) mismatch | Medium | High | Verify against known k<=8 perm_map values |
| make-orbit-maps too slow for k>8 | Medium | Medium | Use nauty's automorphism group output instead |
| alpha computation programs crash on k>8 | Low | Medium | They need Phase 1 type widening applied |
| Thread-safety of nauty calls | Low | Low | nauty is reentrant; wrapper uses thread-local storage |
| Performance regression for k<=8 | Very Low | High | k<=8 path is unchanged (flat table still used) |
| k=10 exceeds Docker container memory (2.6 GB static) | Medium | Low | k=10 is a stretch target; skip GDV/ODV modes to reduce RAM |
| k=10 orbit_map generation takes too long | Low | Low | ~15 min is acceptable; can be pre-generated in Dockerfile |
| k=10 `_orbitList` array (960 MB) causes OOM | Medium | Medium | Guard with `if(k<=9)` or allocate dynamically |

---

## Files Modified (Summary)

**Modified existing files:**
- `src/blant-fundamentals.h` — MAX_K, maxBk
- `src/blant.h` — Gint_type, Gordinal_type, L_K macro
- `src/blant-utils.h` — already handles k>8
- `src/blant-utils.c` — kperm, ExtractPerm, SetGlobalCanonMaps
- `src/blant.c` — assert limits, k>8 build path
- `src/libblant.c` — mapCanonMap guard for k>8
- `src/create-bin-data.c` — error check, kperm, bit packing
- `src/test-bin-data.c` — error check, kperm, bit packing
- `src/blant-predict-release.c` — kperm
- `src/synthetic.c` — kperm
- `src/slow-canon-maps.c` — assert limit
- `src/makeEHD.c` — assert limit
- `Makefile` — k9/k10 targets, nauty build rules, parameterized generators

**New files:**
- `src/nauty/` — nauty library source (external, ~15 files)
- `src/nauty-canonical.c` — wrapper for nauty canonical labeling
- `src/nauty-canonical.h` — header
- `src/canon-hash.c` — canonical Gint -> ordinal lookup (or binary search)
- `src/generate-canon-list.c` — generates `canon_list{k}.txt` (parameterized by k)
- `src/generate-orbit-map.c` — generates `orbit_map{k}.txt` (parameterized by k)

**Generated files (k=9):**
- `canon_maps/canon_list9.txt` — 274,668 canonical graphlets (~19 MB)
- `canon_maps/orbit_map9.txt` — 2,208,612 orbits (~17 MB)
- `canon_maps/canon-ordinal-to-signature9.txt` — signatures
- `canon_maps/alpha_list_{NBE,EBE,MCMC}9.txt` — sampling correction factors

**Generated files (k=10, stretch target):**
- `canon_maps/canon_list10.txt` — 12,005,168 canonical graphlets (~300 MB)
- `canon_maps/orbit_map10.txt` — 113,743,760 orbits (~1.1 GB)
- `canon_maps/canon-ordinal-to-signature10.txt` — signatures
- `canon_maps/alpha_list_{NBE,EBE,MCMC}10.txt` — sampling correction factors
