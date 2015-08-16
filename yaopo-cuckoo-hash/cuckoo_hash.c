#include "cuckoo_hash.h"

cuckoo_stats_t stats;
#define TBL_ELEM(tbl, hash, mask) (tbl + (hash & mask))
#define TBL_ELEM_KEY(tbl, hash, mask) TBL_ELEM(tbl, hash, mask)->key
uint32_t MAX_TBL_REFRESH_TIME = 16;
cuckoo_elem_t orig_insert_elem;

void elem_dump(cuckoo_elem_t *e)
{
    printf(">>  Key(%s@%p), Value(%s@%p), Key_len(%u), hash1(0x%08x), hash2(0x%08x)\n",
            e->key, e->key,
            e->value, e->value,
            e->key_len, e->hash1, e->hash2);
}

void hash_dump(cuckoo_hash_t *hash)
{
    uint32_t i, tbl_len;

    if(hash == NULL) {
        return;
    }

    printf("max_loop: %u\n", hash->max_loop);
    printf("power_of: %u\n", hash->power_of);
    printf("hash_mask: %x\n", hash->hash_mask);

    printf("\n");
    tbl_len = 1 << hash->power_of;
    printf("table size: %u\n", tbl_len);

#if 0
    printf("tbl1: %p\n", hash->tbl1);
    for (i = 0; i < tbl_len; ++i) {
        printf("[%6u] ", i);
        elem_dump(hash->tbl1 + i);
        /* printf(">> 0x%08x 0x%08x\n", (hash->tbl1 + i)->hash1,
                                 (hash->tbl1 + i)->hash2);       */
    }

    printf("tbl2: %p\n", hash->tbl2);
    for (i = 0; i < tbl_len; ++i) {
        printf("[%6u] ", i);
        elem_dump(hash->tbl2 + i);
        /* printf(">> 0x%08x 0x%08x\n", (hash->tbl2 + i)->hash1,
                                 (hash->tbl2 + i)->hash2);       */
    }
#endif

    // stats dump
    printf("\n==================== stats dump\n");
    printf("tbl_refresh: %d\n", stats.tbl_refresh);
    for (i = 0; i < hash->max_loop; ++i) {
        printf("hist.depth%d: %d\n", i, *(stats.hist + i));
    }
}

int32_t
IS_AT(cuckoo_elem_t *elem, cuckoo_elem_t *tbl, uint32_t hash, uint32_t mask)
{
    if (elem == (tbl + (hash & mask)))
        return 0;
    else
       return -1;
}


static inline int32_t
elem_swap(cuckoo_elem_t *e1, cuckoo_elem_t *e2)
{
    cuckoo_elem_t t;
    t = *e1;
    *e1 = *e2;
    *e2 = t;

    return 0;
}

int32_t hash_init(cuckoo_hash_t *hash, uint8_t power_of, uint32_t max_loop)
{
    if(hash == NULL) {
        return -1;
    }

    if (power_of > 32) {
        printf("The power_of value has to less than 33\n");
        return -1;
    }

    // default power_of to 3;
    /* hash->max_loop = 8; */
    /* hash->max_loop = 64; */
    hash->max_loop = max_loop;
    hash->power_of = power_of;
    hash->hash_mask = (1 << hash->power_of) - 1;

    hash->tbl1 = calloc(1 << hash->power_of, sizeof(cuckoo_elem_t));
    hash->tbl2 = calloc(1 << hash->power_of, sizeof(cuckoo_elem_t));

    stats.hist = calloc(sizeof(uint32_t), hash->max_loop);

    return 0;
}

// extend tbl to double and hash refresh
cuckoo_ret tbl_refresh(cuckoo_hash_t *hash)
{
    uint32_t shift_to_half = (1 << hash->power_of);
    uint32_t new_bit;
    uint32_t i;
    cuckoo_elem_t *new_tbl;
    hash->refresh_stats++;
    if (hash->refresh_stats > MAX_TBL_REFRESH_TIME) {
        return FALSE;
    }

    new_bit = (1 << hash->power_of) - 1;

    hash->power_of++;
    hash->hash_mask = (1 << hash->power_of) - 1;
    new_bit ^= hash->hash_mask;

    new_tbl = realloc(hash->tbl1, (1 << hash->power_of) * sizeof(cuckoo_elem_t));
    if (new_tbl == NULL) {
        return FALSE;
    }
    hash->tbl1 = new_tbl;
    // shift bottom half and reset to 0
    memset(hash->tbl1 + shift_to_half, 0, shift_to_half * sizeof(cuckoo_elem_t));
    // hash refresh
    // 1. scan previous keys from begin to half
    // 2. if new_bit is set
    //    2.1 swap the elem to bottom half
    for (i = 0; i < shift_to_half; ++i) {
        if((hash->tbl1 + i)->hash1 & new_bit) {
            elem_swap(hash->tbl1 + i, hash->tbl1 + i + shift_to_half);
        }
    }

    /* hash->tbl2 = realloc(hash->tbl2, 1 << hash->power_of * sizeof(cuckoo_elem_t)); */
    new_tbl = realloc(hash->tbl2, (1 << hash->power_of) * sizeof(cuckoo_elem_t));
    if (new_tbl == NULL) {
        return FALSE;
    }
    hash->tbl2 = new_tbl;
    // shift bottom half and reset to 0
    memset(hash->tbl2 + shift_to_half, 0, shift_to_half * sizeof(cuckoo_elem_t));

    for (i = 0; i < shift_to_half; ++i) {
        if((hash->tbl2 + i)->hash2 & new_bit) {
            elem_swap(hash->tbl2 + i, hash->tbl2 + i + shift_to_half);
        }
    }

    stats.tbl_refresh++;

    return TRUE;
}

int32_t
elem_init(  cuckoo_elem_t *elem,
            void *k, void *v,
            uint32_t key_len,
            uint32_t h1, uint32_t h2)
{
    elem->key = k;
    elem->value = v;
    elem->key_len = key_len;
    elem->hash1 = h1;
    elem->hash2 = h2;

    return 0;
}


static inline int32_t
insert(cuckoo_elem_t *tbl, uint32_t hash, uint32_t mask, cuckoo_elem_t *elem)
{
    *(tbl + (hash & mask)) = *elem;
    return 0;
}

static inline cuckoo_elem_t *
get_tbl_by_depth(cuckoo_hash_t *hash, uint32_t depth)
{
    if(depth % 2) {
        // 1, 3, 5, 7
        return hash->tbl2;
    } else {
        // 0, 2, 4, 6
        return hash->tbl1;
    }
}

static inline uint32_t
get_hash_by_depth(cuckoo_elem_t *elem, uint32_t depth)
{
    if(depth % 2) {
        // 1, 3, 5, 7
        return elem->hash2;
    } else {
        // 0, 2, 4, 6
        return elem->hash1;
    }
}

void hist_succ(uint32_t depth)
{
    *(stats.hist + depth)+=1;
}

static inline cuckoo_ret
kick_insert(cuckoo_hash_t *hash,
            cuckoo_elem_t *elem,
            uint32_t depth)
{
    cuckoo_ret ret = FALSE;
    cuckoo_elem_t *tbl = get_tbl_by_depth(hash, depth);

    //TODO: rename to hash
    uint32_t elem_hash = get_hash_by_depth(elem, depth);

    /* printf("[%u] ", depth);
    elem_dump(elem);           */

    if (depth >= hash->max_loop) {
        printf("ERROR: Reach Max LOOP %u as add \"%s\". tbl size %u\n",
                hash->max_loop, orig_insert_elem.key, 1 << hash->power_of);
        ret = OVER_MAX_LOOP;
        goto out;
    }

    if (TBL_ELEM_KEY(tbl, elem_hash, hash->hash_mask) == NULL) {
        // reach empty slot
        ret = insert(tbl, elem_hash, hash->hash_mask, elem);
        if (ret == TRUE) hist_succ(depth);
    } else {
        elem_swap(TBL_ELEM(tbl, elem_hash, hash->hash_mask), elem);

        tbl = get_tbl_by_depth(hash, depth+1);
        ret = kick_insert(hash, elem, depth+1);

        if(ret == OVER_MAX_LOOP) {
            ret = tbl_refresh(hash);
            if (ret == FALSE)
            {
                goto out;
            }
            ret = kick_insert(hash, elem, 1);
        }
    }

out:
    return ret;
}


int32_t
hash_insert(cuckoo_hash_t *hash, void *key, size_t key_len, void *value)
{
    uint32_t h1 = 0;
    uint32_t h2 = 0;
    cuckoo_ret ret = FALSE;
    cuckoo_elem_t *elem = calloc(1, sizeof(cuckoo_elem_t));
    /* cuckoo_elem_t elem; */
    /* printf("key_len(%u) key(%s)\n", key_len, key); */

    hashlittle2(key, key_len, &h1, &h2);
    /* printf("%s key(%s) key_len(%u) h1(%x) h2(%x)\n", __FUNCTION__, key, key_len, h1, h2); */

    elem_init(elem, key, value, key_len, h1, h2);

    orig_insert_elem = *elem;
    if (TBL_ELEM_KEY(hash->tbl1, h1, hash->hash_mask) == NULL) {
        // reach empty slot
        ret = insert(hash->tbl1, h1, hash->hash_mask, elem);
        if (ret == TRUE) hist_succ(0);
    } else {
        /* printf("\n[0] ");
        elem_dump(elem);     */

        elem_swap(TBL_ELEM(hash->tbl1, h1, hash->hash_mask), elem);
        ret = kick_insert(hash, elem, 1);
    }

    // replace of stack doable?
    free(elem);
    return ret;
}

cuckoo_elem_t *hash_lookup(cuckoo_hash_t *hash, void *key, uint32_t key_len)
{

    uint32_t h1 = 0;
    uint32_t h2 = 0;
    cuckoo_elem_t *elem = NULL;
    hashlittle2(key, key_len, &h1, &h2);
    /* printf("%s key(%s) key_len(%u) h1(%x) h2(%x)\n", __FUNCTION__, key, key_len, h1, h2); */

    elem = hash->tbl1 + (h1 & hash->hash_mask);
    if(elem->key && memcmp(elem->key, key, key_len) == 0) {
        /* printf("found the key(%s) at slot %d in tbl1\n", (char *)key, h1 & hash->hash_mask); */
        return  elem;
    }

    elem = hash->tbl2 + (h2 & hash->hash_mask);
    if(elem->key && memcmp(elem->key, key, key_len) == 0) {
        /* printf("found the key(%s) at slot %d in tbl2\n", (char *)key, h2 & hash->hash_mask); */
        return  elem;
    }

    /* printf("### not found the key(%s) hash_mask(%x) h1 slot(%d) h2 slot(%d)\n",
            (char *)key,
            hash->hash_mask,
            h1 & hash->hash_mask,
            h2 & hash->hash_mask);                                                 */
    return NULL;
}

cuckoo_ret hash_delete(cuckoo_hash_t *hash, void *key, uint32_t key_len)
{
    /* printf("delete key(%s) ", (char *)key); */

    cuckoo_ret ret = FALSE;

    cuckoo_elem_t *elem = hash_lookup(hash, key, key_len);
    if(elem == NULL) goto out;

    /* printf("elem->key(%s@%p)\n", (char *)elem->key, elem->key); */
    free(elem->key);
    free(elem->value);
    memset(elem, 0, sizeof(cuckoo_elem_t));
    ret = TRUE;

out:
    return ret;
}

char **build_keys(uint32_t num)
{
    uint32_t i;
    char **keys = malloc(sizeof(char *) * (num + 1/*elem EOL*/));
    char **key = keys;
    char key_str[80];
    uint32_t key_len;

    for (i = 0; i < num; ++i) {
        key_len = sprintf(key_str, "key %08d", i);
        /* key_str[key_len] = '\0'; */
        /* printf("key(%s) key_len(%d)\n", key_str, key_len); */

        *key = calloc(1, key_len + 1/* EOL */);
        memcpy(*key, key_str, key_len);
        key++;
    }
    *key = NULL;

    return keys;
}

int main(int argc, char *argv[])
{
    int32_t ret;
    cuckoo_hash_t *ht = calloc(1, sizeof(cuckoo_hash_t));;
    clock_t t_s, t_e;

    uint32_t test_scale = 1000000;
    /* char **key = keys; */
    char **key, **del_key, **lku_key, **free_key;
    key = del_key = lku_key = free_key = build_keys(test_scale);

    char **value, **free_value;

    // -----------------------------------------------------
    t_s = clock();
    value = free_value = build_keys(test_scale);
    t_e = clock();
    printf("build_keys elaspe %lu\n", t_e - t_s);

    // -----------------------------------------------------
    t_s = clock();
    ret = hash_init(ht, 6, 128);
    t_e = clock();
    printf("hash_init elaspe %lu\n", t_e - t_s);

    // -----------------------------------------------------
    t_s = clock();
    /* for (; *key; key++) { */
    /* for (i = 0; i < test_scale; key++, i++) { */
    for (; *key; key++, value++) {
        /* ret = hash_insert(ht, *key, strlen(*key), *key); */
        ret = hash_insert(ht, *key, strlen(*key), *value);
        if (ret) {
            printf("hash_insert fail as insert \"%s\". ret(%d)\n", *key, ret);
            goto out;
        }
    }
    t_e = clock();
    printf("hash_insert elaspe %lu\n", t_e - t_s);

    printf("last hash_insert insert \"%s\". ret(%d)\n", orig_insert_elem.key, ret);

    // -----------------------------------------------------
    // lookup test
    t_s = clock();
    for (; *lku_key; lku_key++) {
        hash_lookup(ht, *lku_key, strlen(*lku_key));
    }
    t_e = clock();
    printf("hash_lookup elaspe %lu\n", t_e - t_s);

    hash_dump(ht);

    // -----------------------------------------------------
    t_s = clock();
    for (; *del_key; del_key++) {
        hash_delete(ht, *del_key, strlen(*del_key));
    }
    t_e = clock();
    printf("hash_delete elaspe %lu\n", t_e - t_s);

out:
    free(free_key);
    free(free_value);
    free(ht->tbl1);
    free(ht->tbl2);
    free(stats.hist);
    free(ht);

    return 0;
}

