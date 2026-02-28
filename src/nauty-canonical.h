#ifndef NAUTY_CANONICAL_H
#define NAUTY_CANONICAL_H

#include "blant.h"

/*
 * Compute the canonical form of a TINY_GRAPH using nauty's densenauty().
 *
 * Parameters:
 *   tg    - input TINY_GRAPH (not modified)
 *   k     - number of nodes in the graph
 *   perm  - output array of size k; perm[i] = the original node that maps to
 *           canonical position i (can2non mapping, matching BLANT's PERMS_CAN2NON=1)
 *
 * Returns:
 *   The Gint_type integer encoding of the canonical TINY_GRAPH, computed via
 *   TinyGraph2Int() so the bit layout matches BLANT's LOWER_TRIANGLE convention.
 */
Gint_type NautyCanonical(TINY_GRAPH *tg, int k, unsigned char perm[]);

#endif /* NAUTY_CANONICAL_H */
