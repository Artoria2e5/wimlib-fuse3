#include "wimlib/lz_mf.h"

extern const struct lz_mf_ops lz_null_ops;
extern const struct lz_mf_ops lz_brute_force_ops;
extern const struct lz_mf_ops lz_hash_chains_ops;
extern const struct lz_mf_ops lz_binary_trees_ops;
extern const struct lz_mf_ops lz_lcp_interval_tree_ops;
extern const struct lz_mf_ops lz_linked_suffix_array_ops;
extern const struct lz_mf_ops lz_hash_arrays_ops;
extern const struct lz_mf_ops lz_hash_arrays_64_ops;
