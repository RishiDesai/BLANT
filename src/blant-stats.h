/* blant-stats.h -- Simple statistics for BLANT confidence intervals
 * Replaces libwayne's stats.h/stats.c (only the subset used by BLANT)
 */
#ifndef BLANT_STATS_H
#define BLANT_STATS_H

#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "blant-fatal.h"

typedef struct _statistic {
    int n;
    bool geom;
    double sum, sum2, min, max;
    double weightedSum, weightedSum2, totalWeight;
} STAT;

static inline STAT *StatAlloc(int unused1, double unused2, double unused3, bool geom, bool unused4) {
    (void)unused1; (void)unused2; (void)unused3; (void)unused4;
    STAT *s = (STAT*)calloc(1, sizeof(STAT));
    if (!s) Fatal("StatAlloc: out of memory");
    s->geom = geom;
    s->min = 1e300;
    s->max = -1e300;
    return s;
}

static inline STAT *StatReset(STAT *s) {
    s->n = 0;
    s->sum = s->sum2 = 0;
    s->weightedSum = s->weightedSum2 = s->totalWeight = 0;
    s->min = 1e300;
    s->max = -1e300;
    return s;
}

static inline void StatFree(STAT *s) { free(s); }

static inline void StatAddSample(STAT *s, double sample) {
    s->n++;
    s->sum += sample;
    s->sum2 += sample * sample;
    if (sample < s->min) s->min = sample;
    if (sample > s->max) s->max = sample;
}

static inline void StatAddWeightedSample(STAT *s, double weight, double sample) {
    s->n++;
    s->totalWeight += weight;
    s->weightedSum += weight * sample;
    s->weightedSum2 += weight * sample * sample;
    s->sum += sample;
    s->sum2 += sample * sample;
    if (sample < s->min) s->min = sample;
    if (sample > s->max) s->max = sample;
}

static inline void StatDelSample(STAT *s, double sample) {
    s->n--;
    s->sum -= sample;
    s->sum2 -= sample * sample;
}

static inline double StatMean(STAT *s) {
    if (s->n == 0) return 0;
    if (s->totalWeight > 0) return s->weightedSum / s->totalWeight;
    return s->sum / s->n;
}

#define StatN(s) ((s)->n)
#define StatMin(s) ((s)->min)
#define StatMax(s) ((s)->max)

static inline double StatVariance(STAT *s) {
    if (s->n < 2) return 0;
    double mean = StatMean(s);
    return (s->sum2 / s->n - mean * mean) * s->n / (s->n - 1);
}

static inline double StatStdDev(STAT *s) { return sqrt(StatVariance(s)); }

/* Approximate t-distribution quantile (two-tailed) for confidence intervals */
static inline double _StatTQuantile(double confidence, int df) {
    /* Use normal approximation for large df */
    double alpha = 1.0 - confidence;
    /* Abramowitz and Stegun approximation for inverse normal */
    double p = 1.0 - alpha / 2.0;
    double t = sqrt(-2.0 * log(1.0 - p));
    double c0 = 2.515517, c1 = 0.802853, c2 = 0.010328;
    double d1 = 1.432788, d2 = 0.189269, d3 = 0.001308;
    double z = t - (c0 + c1*t + c2*t*t) / (1 + d1*t + d2*t*t + d3*t*t*t);
    /* Adjust for t-distribution with finite df */
    if (df < 120) {
        z = z * (1.0 + 1.0/(4.0*df));
    }
    return z;
}

static inline double StatConfInterval(STAT *s, double confidence) {
    if (s->n < 2) return 1e30;
    double se = StatStdDev(s) / sqrt(s->n);
    double t = _StatTQuantile(confidence, s->n - 1);
    return t * se;
}

static inline double StatTDistP2Z(double quantile, long freedom) {
    return _StatTQuantile(quantile, freedom);
}

#endif /* BLANT_STATS_H */
