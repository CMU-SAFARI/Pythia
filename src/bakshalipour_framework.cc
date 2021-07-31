#include "bakshalipour_framework.h"

/**
* A very simple and efficient hash function that:
* 1) Splits key into blocks of length `index_len` bits and computes the XOR of all blocks.
* 2) Replaces the least significant block of key with computed block.
* With this hash function, the index will depend on all bits in the key. As a consequence, entries
* will be more randomly distributed among the sets.
* NOTE: Applying this hash function twice with the same `index_len` acts as the identity function.
*/
uint64_t hash_index(uint64_t key, int index_len) {
   if (index_len == 0)
   return key;
   for (uint64_t tag = (key >> index_len); tag > 0; tag >>= index_len)
   key ^= tag & ((1 << index_len) - 1);
   return key;
}
