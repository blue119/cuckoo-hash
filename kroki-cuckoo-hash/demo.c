#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include "cuckoo_hash.h"

typedef struct data_s {
    char key[16];
    uint32_t value;
} data_t;

data_t *
_prepare_data(uint32_t num)
{
    uint32_t i;

    data_t *d = (data_t *)malloc(sizeof(data_t) * num);
    for (i = 0; i < num; ++i) {
        sprintf((d+i)->key, "my-key: %d", i);
        (d+i)->value = i;
    }

    return d;
}

uint32_t
hash_walkthru(struct cuckoo_hash *hash)
{
    struct cuckoo_hash_item *item = NULL;
    uint32_t c = 0;

    while(1){
        item = cuckoo_hash_next(hash, item);
        if (item) {
            c++;
            DEBUG_PRINT("%p\n", item->value);
            continue;
        }
        break;
    }

    return c;
}

uint32_t
hash_walkthru_n_del(struct cuckoo_hash *hash)
{
    struct cuckoo_hash_item *item = NULL;
    uint32_t c = 0;

    while(1){
        item = cuckoo_hash_next(hash, item);
        if (item) {
            c++;
            DEBUG_PRINT("%p\n", item->value);
            cuckoo_hash_remove(hash, item);
            continue;
        }
        break;
    }

    return c;
}

void cuckoo_test(uint32_t num)
{
    struct cuckoo_hash *hash = calloc(1, sizeof(struct cuckoo_hash));
    struct cuckoo_hash_item *item = calloc(1, sizeof(struct cuckoo_hash_item));
    data_t *d;
    uint32_t i, result;
    clock_t c_s, c_e;

    cuckoo_hash_init(hash, 1);

    // -----------------------------------------------------------------------
    c_s = clock();
    d = _prepare_data(num);
    c_e = clock();
    printf("prepare data ETA: %lu\n", c_e - c_s);

    // -----------------------------------------------------------------------
    c_s = clock();
    for (i = 0; i < num; ++i) {
        item = cuckoo_hash_insert(hash, (void *)(d+i)->key, strlen((d+i)->key), (void *)d+i);
    }
    c_e = clock();
    printf("insert data ETA: %lu\n", c_e - c_s);

    /* item = cuckoo_hash_lookup(hash, (d+2)->key, strlen((d+2)->key));
    printf("%p\n", item->value);                                        */

    // -----------------------------------------------------------------------
    c_s = clock();
    result = hash_walkthru(hash);
    c_e = clock();
    if (result != num) {
        printf("result(%u) is not equal to num(%u)\n", result, num);
    }
    printf("walkthru data ETA: %lu\n", c_e - c_s);

    // -----------------------------------------------------------------------
    // load factor
    /* printf("load factor %f\n", cuckoo_hash_count(hash)/(hash->bin_size << hash->power)); */
    printf("%f\n", (float)cuckoo_hash_count(hash)/(float)(hash->bin_size << hash->power));


    // -----------------------------------------------------------------------
    c_s = clock();
    result = hash_walkthru_n_del(hash);
    c_e = clock();
    if (result != num) {
        printf("result(%u) is not equal to num(%u)\n", result, num);
    }
    printf("walkthru and del data ETA: %lu\n", c_e - c_s);

    // -----------------------------------------------------------------------
    c_s = clock();
    result = hash_walkthru(hash);
    c_e = clock();
    if (result != 0) {
        printf("result(%u) is not equal to num(%u)\n", result, num);
    }
    printf("walkthru data ETA: %lu\n", c_e - c_s);

}

int main(int argc, char *argv[])
{
    uint32_t test_num;

    if (argc != 2 || atoi(argv[1]) == 0) {
        printf("%s\n", "./demo <num>");
        exit(-1);
    }

    test_num = atoi(argv[1]);

    cuckoo_test(test_num);

    return 0;
}

