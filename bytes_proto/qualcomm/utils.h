/* utils.h
 * Shared helpers for the Qualcomm protocol library: ID/name mapping tables
 * and the ARRAY_SIZE macro. No Python dependency.
 */

#ifndef __DM_COLLECTOR_C_UTILS_H__
#define __DM_COLLECTOR_C_UTILS_H__

#include <vector>

// Number of elements in a locally-defined array. Does NOT work on arrays
// declared `extern T x[]` (incomplete type) — use the matching *_n constant
// exported next to such arrays instead.
#define ARRAY_SIZE(array, element_type) (sizeof(array) / sizeof(element_type))

typedef std::vector<int> IdVector;

struct ValueName {
    int val;
    const char *name;
    bool b_public;	//Yuanjie: True if exposed to public, False otherwise
};

// Find multiple IDs corresponding to name, and append all IDs to out_vector.
// Return how many IDs are found.
int find_ids (const ValueName id_to_name [], int n, const char *name, IdVector& out_vector);

// Return the name mapped to val, or NULL if not found.
const char* search_name (const ValueName id_to_name [], int n, int val);

#endif  // __DM_COLLECTOR_C_UTILS_H__