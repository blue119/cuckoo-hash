#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if ! defined(_CUCKOO_HASH_H)
#define _CUCKOO_HASH_H

typedef struct cuckoo_elem_s {
    void *key;
    void *value;
    uint32_t key_len;

    uint32_t hash1;
    uint32_t hash2;
} cuckoo_elem_t;

typedef struct cuckoo_hash_s {
    uint32_t max_loop; // max depth to 2^8
    uint32_t power_of; // tbl size equal 2^power_of
    uint32_t hash_mask; // mask equal 2^power_of - 1
    uint32_t refresh_stats;

    cuckoo_elem_t *tbl1;
    cuckoo_elem_t *tbl2;
} cuckoo_hash_t;

typedef struct cuckoo_stats_s {
    uint32_t tbl_refresh;
    uint32_t *hist;
} cuckoo_stats_t;

typedef enum{
    TRUE,
    FALSE,
    TBL_FULL,
    OVER_MAX_LOOP
} cuckoo_ret;

extern void hashlittle2(const void *key,
        size_t length,
        uint32_t *pc,
        uint32_t *pb);

#endif // _CUCKOO_HASH_H
