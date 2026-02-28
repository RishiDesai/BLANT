#ifndef BLANT_HIGHK_H
#define BLANT_HIGHK_H

#include "blant-fundamentals.h"

#if HIGH_K_SUPPORTED

#include "blant.h"
#include "tinygraph.h"

/* High-k runtime mode:
 * 0 = flat table (k<=8, existing code path)
 * 1 = hash map loaded from canon_list file (k=9-11)
 * 2 = on-the-fly: nauty per sample, streaming ordinal assignment (k>=12)
 */
extern int _highK_mode;

/* Initialize the high-k canonical mapping system.
 * Called from SetGlobalCanonMaps() when k > 8.
 * For k=9-11: loads canon_list{k}.txt into a hash map.
 * For k>=12: initializes empty hash map for streaming discovery.
 */
void HighK_Init(int k);

/* Look up or assign an ordinal for a canonical Gint.
 * In mode 1 (hash from file): looks up in preloaded hash map.
 * In mode 2 (on-the-fly): assigns new ordinal if not seen before.
 * Returns the ordinal, or (Gordinal_type)-1 on error.
 */
Gordinal_type HighK_LookupOrdinal(Gint_type canon_gint);

/* Compute canonical form using nauty + look up ordinal.
 * This is the high-k replacement for ExtractPerm().
 * Fills perm[] with the can2non permutation.
 * Returns the canonical ordinal.
 */
Gordinal_type HighK_ExtractPerm(unsigned char perm[], Gint_type Gint, int k);

/* Load canon_list file into hash map (for k=9-11). */
void HighK_LoadCanonList(const char *filename, int k);

/* Load orbit_map file (for k=9-11). */
void HighK_LoadOrbitMap(const char *filename, int k);

/* Get number of canonicals discovered so far (on-the-fly mode). */
Gordinal_type HighK_NumCanonicals(void);

/* Get number of orbits loaded/discovered. */
Gint_type HighK_NumOrbits(void);

/* Dynamically allocated arrays for high-k mode */
extern Gint_type *_dyn_canonList;
extern char *_dyn_canonNumEdges;
extern Gint_type **_dyn_orbitList;   /* _dyn_orbitList[canonical][node] = orbit ID */
extern Gordinal_type *_dyn_orbitCanonMapping;
extern char *_dyn_orbitCanonNodeMapping;
extern Gint_type *_dyn_alphaList;

/* Clean up high-k resources */
void HighK_Cleanup(void);

#endif /* HIGH_K_SUPPORTED */

#endif /* BLANT_HIGHK_H */
