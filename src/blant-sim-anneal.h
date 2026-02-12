/* blant-sim-anneal.h -- Simulated Annealing for BLANT
 * Replaces libwayne's sim_anneal.h/sim_anneal.c
 * Only used when DYNAMIC_CANON_MAP is enabled.
 */
#ifndef BLANT_SIM_ANNEAL_H
#define BLANT_SIM_ANNEAL_H

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include "blant-fatal.h"
#include "blant-graph.h" /* for foint */

typedef double (*pScoreFunc)(bool global, const foint);
typedef double (*pMoveFunc)(const foint solution);
typedef bool   (*pAcceptFunc)(const bool accept, const foint solution);
typedef void   (*pReportFunc)(int iter, foint f);

typedef struct _sim_anneal {
    unsigned long iter, maxIters;
    int direction;
    double temperature, tInitial, tDecay, currentScore;
    foint currentSolution;
    pMoveFunc Move;
    pAcceptFunc Accept;
    pScoreFunc Score;
    pReportFunc Report;
} SIM_ANNEAL;

static inline SIM_ANNEAL *SimAnnealAlloc(double direction, foint initSol, pMoveFunc Move,
    pScoreFunc Score, pAcceptFunc Accept, unsigned long maxIters, double pBadStart, double pBadEnd, pReportFunc Report) {
    (void)pBadStart; (void)pBadEnd;
    SIM_ANNEAL *sa = (SIM_ANNEAL*)calloc(1, sizeof(SIM_ANNEAL));
    if (!sa) Fatal("SimAnnealAlloc: out of memory");
    sa->direction = (direction < 0) ? -1 : 1;
    sa->currentSolution = initSol;
    sa->Move = Move;
    sa->Score = Score;
    sa->Accept = Accept;
    sa->Report = Report;
    sa->maxIters = maxIters;
    sa->currentScore = Score(true, initSol);
    return sa;
}

static inline bool SimAnnealSetSchedule(SIM_ANNEAL *sa, double tInitial, double tDecay) {
    sa->tInitial = tInitial;
    sa->tDecay = tDecay;
    sa->temperature = tInitial;
    return true;
}

static inline void SimAnnealAutoSchedule(SIM_ANNEAL *sa) {
    sa->tInitial = fabs(sa->currentScore) + 1;
    sa->tDecay = 2.0;
    sa->temperature = sa->tInitial;
}

static inline int SimAnnealRun(SIM_ANNEAL *sa) {
    extern double drand48(void);
    for (unsigned long i = 0; i < sa->maxIters; i++) {
        sa->iter++;
        sa->temperature = sa->tInitial * exp(-sa->tDecay * (double)sa->iter / sa->maxIters);
        double newScore = sa->Move(sa->currentSolution);
        double delta = (newScore - sa->currentScore) * sa->direction;
        bool accept = false;
        if (delta <= 0) accept = true;
        else if (sa->temperature > 0 && drand48() < exp(-delta / sa->temperature)) accept = true;
        sa->Accept(accept, sa->currentSolution);
        if (accept) sa->currentScore = newScore;
    }
    return 1;
}

static inline foint SimAnnealSol(SIM_ANNEAL *sa) { return sa->currentSolution; }
static inline void SimAnnealFree(SIM_ANNEAL *sa) { free(sa); }

#endif /* BLANT_SIM_ANNEAL_H */
