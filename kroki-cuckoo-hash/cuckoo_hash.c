/*
  Copyright (C) 2010 Tomash Brechko.  All rights reserved.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cuckoo_hash.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct _cuckoo_hash_elem
{
  struct cuckoo_hash_item hash_item;
  uint32_t hash1;
  uint32_t hash2;
};


static void
hash_dump(const struct cuckoo_hash *hash)
{
    unsigned int i;

    DEBUG_PRINT("%s\n", "++++++++++++++++++++++++++++++");
    DEBUG_PRINT("power: %d, bin_size: %d, count %lu, hash_len %u\n",
            hash->power, hash->bin_size, hash->count,
            hash->bin_size << hash->power);

    for (i = 0; i < hash->bin_size << hash->power; ++i) {
        struct _cuckoo_hash_elem *tbl = hash->table;

        /* if (tbl->hash_item.value == NULL) {
            continue;
        }                                      */

        DEBUG_PRINT("hash[%d]: hash1 %x, hash2 %x, key: %s, value: %p, key_len: %lu\n", i,
                ((tbl+i)->hash1), ((tbl+i)->hash2),
                (char *)(tbl+i)->hash_item.key,
                (tbl+i)->hash_item.value,
                /* *((int *)(tbl+i)->hash_item.value), */
                /* (intptr_t)(tbl+i)->hash_item.value, */
                (tbl+i)->hash_item.key_len);
    }
  DEBUG_PRINT("%s\n", "******************************");
}

static inline
void
compute_hash(const void *key, size_t key_len,
             uint32_t *h1, uint32_t *h2)
{
  extern void hashlittle2(const void *key, size_t length,
                          uint32_t *pc, uint32_t *pb);

  /* Initial values are arbitrary.  */
  *h1 = 0x3ac5d673;
  *h2 = 0x6d7839d0;
  hashlittle2(key, key_len, h1, h2);
  if (*h1 != *h2)
    {
      return;
    }
  else
    {
      *h2 = ~*h2;

    }
}


bool
cuckoo_hash_init(struct cuckoo_hash *hash, unsigned char power)
{
  if (power == 0)
    power = 1;

  hash->power = power;
  hash->bin_size = 4;
  hash->count = 0;
  hash->table = calloc((size_t) hash->bin_size << power, sizeof(*hash->table));
  if (! hash->table)
    return false;

  return true;
}


void
cuckoo_hash_destroy(const struct cuckoo_hash *hash)
{
  free(hash->table);
}


static inline
struct _cuckoo_hash_elem *
bin_at(const struct cuckoo_hash *hash, uint32_t index)
{
  return (hash->table + index * hash->bin_size);
}


static inline
struct cuckoo_hash_item *
lookup(const struct cuckoo_hash *hash, const void *key, size_t key_len,
       uint32_t h1, uint32_t h2)
{
  uint32_t mask = (1U << hash->power) - 1;

  struct _cuckoo_hash_elem *elem, *end;


  DEBUG_PRINT("Start Lookup %s/%x/%x ===============================\n",
          (char *)key, h1, h2);
  /* hash_dump(hash); */
  elem = bin_at(hash, (h1 & mask));

  end = elem + hash->bin_size;
  while (elem != end) {

      if (elem->hash2 == h2 && elem->hash1 == h1
          && elem->hash_item.key_len == key_len
          && memcmp(elem->hash_item.key, key, key_len) == 0) {

          DEBUG_PRINT("table_p %p, elem %p, h1 %u, h1 index %d, bin_size %d\n",
                  hash->table, elem, h1,
                  h1 & mask, hash->bin_size);
          DEBUG_PRINT("\n");
          DEBUG_PRINT("Stop Lookup ---------------------------------- \n\n");
          return &elem->hash_item;
        }

      ++elem;
    }

  elem = bin_at(hash, (h2 & mask));
  end = elem + hash->bin_size;
  while (elem != end) {

      if (elem->hash2 == h1 && elem->hash1 == h2
          && elem->hash_item.key_len == key_len
          && memcmp(elem->hash_item.key, key, key_len) == 0) {

          DEBUG_PRINT("table_p %p, elem %p, h2 %u, h2 index %d, bin_size %d\n",
                  hash->table, elem,h2,
                  h2 & mask, hash->bin_size);
          DEBUG_PRINT("\n");
          DEBUG_PRINT("Stop Lookup ---------------------------------- \n\n");
          return &elem->hash_item;
        }

      ++elem;
    }

    DEBUG_PRINT("\n");
    DEBUG_PRINT("Stop Lookup ---------------------------------- \n\n");
  return NULL;
}


struct cuckoo_hash_item *
cuckoo_hash_lookup(const struct cuckoo_hash *hash,
                   const void *key, size_t key_len)
{
  uint32_t h1, h2;
  compute_hash(key, key_len, &h1, &h2);

  return lookup(hash, key, key_len, h1, h2);
}


void
cuckoo_hash_remove(struct cuckoo_hash *hash,
                   const struct cuckoo_hash_item *hash_item)
{
//FIXME, only clear up hash_item, not in hash?
  if (hash_item)
    {
        /* printf("%s: remove %p\n", __FUNCTION__, hash_item); */
      struct _cuckoo_hash_elem *elem =
        ((struct _cuckoo_hash_elem *)
         ((char *) hash_item - offsetof(struct _cuckoo_hash_elem, hash_item)));
      elem->hash1 = elem->hash2 = 0;
      --hash->count;
    }
}


static
bool
grow_table(struct cuckoo_hash *hash)
{
  size_t size = ((size_t) hash->bin_size << hash->power) * sizeof(*hash->table);
  struct _cuckoo_hash_elem *table = realloc(hash->table, size * 2);
  if (! table)
    return false;

  DEBUG_PRINT("\n\n\n\nEnter grow_table\n");

#ifdef DEBUG
  hash_dump(hash);
#endif

  hash->table = table;
  memcpy((char *) hash->table + size, hash->table, size);
  ++hash->power;

  DEBUG_PRINT("------------------------------------------\n");
#ifdef DEBUG
  hash_dump(hash);
#endif
  DEBUG_PRINT("Leave grow_table\n\n\n\n");
  return true;
}


static
bool
grow_bin_size(struct cuckoo_hash *hash)
{
    /* printf("%s\n", __FUNCTION__); */
  size_t size =
    ((size_t) hash->bin_size << hash->power) * sizeof(*hash->table);
  uint32_t bin_count = 1U << hash->power;
  size_t add = bin_count * sizeof(*hash->table);
  struct _cuckoo_hash_elem *table = realloc(hash->table, size + add);
  if (! table)
    return false;

  hash->table = table;
  for (uint32_t bin = bin_count - 1; bin > 0; --bin)
    {
      struct _cuckoo_hash_elem *old = bin_at(hash, bin);
      struct _cuckoo_hash_elem *new = old + bin;
      memmove(new, old, hash->bin_size * sizeof(*hash->table));
      memset(new + hash->bin_size, 0, sizeof(*hash->table));
    }
  memset(hash->table + hash->bin_size, 0, sizeof(*hash->table));

  ++hash->bin_size;
  DEBUG_PRINT("grow_bin)table:: size: %d\n", hash->bin_size);

  return true;
}


static
bool
undo_insert(struct cuckoo_hash *hash, struct _cuckoo_hash_elem *item,
            size_t max_depth, uint32_t offset, int phase)
{
  uint32_t mask = (1U << hash->power) - 1;

  for (size_t depth = 0; depth < max_depth * phase; ++depth)
    {
      if (offset-- == 0)
        offset = hash->bin_size - 1;

      uint32_t h2m = item->hash2 & mask;
      struct _cuckoo_hash_elem *beg = bin_at(hash, h2m);

      struct _cuckoo_hash_elem victim = beg[offset];

      beg[offset].hash_item = item->hash_item;
      beg[offset].hash1 = item->hash2;
      beg[offset].hash2 = item->hash1;

      uint32_t h1m = victim.hash1 & mask;
      if (h1m != h2m)
        {
          assert(depth >= max_depth);

          return true;
        }

      *item = victim;
    }

  return false;
}


static inline
bool
insert(struct cuckoo_hash *hash, struct _cuckoo_hash_elem *item)
{
  size_t max_depth = (size_t) hash->power << 5;

  if (max_depth > (size_t) hash->bin_size << hash->power)
    max_depth = (size_t) hash->bin_size << hash->power;

  uint32_t offset = 0;
  int phase = 0;
  while (phase < 2)
    {
      uint32_t mask = (1U << hash->power) - 1;
      /* printf("mask: %d\n", mask); */

      for (size_t depth = 0; depth < max_depth; ++depth)
        {
          uint32_t h1m = item->hash1 & mask;
      /* printf("hash1: %u\n", item->hash1); */
          struct _cuckoo_hash_elem *beg = bin_at(hash, h1m);
          struct _cuckoo_hash_elem *end = beg + hash->bin_size;

          for (struct _cuckoo_hash_elem *elem = beg; elem != end; ++elem) {
              if (elem->hash1 == elem->hash2 || (elem->hash1 & mask) != h1m) {

                DEBUG_PRINT("item: hash1 %x, hash2 %x, key: %s, value: 0x%p, h1m: %x, depth %lu, mask %x\n",
                        item->hash1,
                        item->hash2,
                (char *)item->hash_item.key,
                /* (int)   item->hash_item.value, */
                item->hash_item.value,
                /* (intptr_t)   item->hash_item.value, */
                        h1m, depth, mask);

                DEBUG_PRINT("elem hash 1 %x, hash2 %x, key: %s, value: 0x%p, h1m: %x, depth %lu\n",
                        elem->hash1,
                        elem->hash2,
                (char *)elem->hash_item.key,
                elem->hash_item.value,
                /* (int)   elem->hash_item.value, */
                        h1m, depth);

                  *elem = *item;
#ifdef DEBUG
                  hash_dump(hash);
#endif

                  return true;
                }
            }


          struct _cuckoo_hash_elem victim = beg[offset];

          DEBUG_PRINT("victim key: %s, item key: %s, offset: %d\n",
          (char *)victim.hash_item.key,
          (char *)item->hash_item.key,
                  offset);

          beg[offset] = *item;

          item->hash_item = victim.hash_item;
          item->hash1 = victim.hash2;
          item->hash2 = victim.hash1;

#ifdef DEBUG
          hash_dump(hash);
#endif
          DEBUG_PRINT("\n\n\n");

          if (++offset == hash->bin_size)
            offset = 0;
        }

      /* printf("line: %d\n", __LINE__); */
      ++phase;

      if (phase == 1)
        {
          if (grow_table(hash))
            /* continue */;
          else
            break;
        }
    }

  if (grow_bin_size(hash))
    {
      uint32_t mask = (1U << hash->power) - 1;
      struct _cuckoo_hash_elem *last =
        bin_at(hash, (item->hash1 & mask) + 1) - 1;

      *last = *item;

      return true;
    }
  else
    {
      return undo_insert(hash, item, max_depth, offset, phase);
    }
}


struct cuckoo_hash_item *
cuckoo_hash_insert(struct cuckoo_hash *hash,
                   const void *key, size_t key_len, void *value)
{
  uint32_t h1, h2;
  compute_hash(key, key_len, &h1, &h2);

  struct cuckoo_hash_item *item = lookup(hash, key, key_len, h1, h2);
  if (item) {
      return item;
    }

  struct _cuckoo_hash_elem elem = {
    .hash_item = { .key = key, .key_len = key_len, .value = value },
    .hash1 = h1,
    .hash2 = h2
  };

  if (insert(hash, &elem)) {
      ++hash->count;

      return NULL;
    } else {
      assert(elem.hash_item.key == key);
      assert(elem.hash_item.key_len == key_len);
      assert(elem.hash_item.value == value);
      assert(elem.hash1 == h1);
      assert(elem.hash2 == h2);

      return CUCKOO_HASH_FAILED;
    }
}


struct cuckoo_hash_item *
cuckoo_hash_next(const struct cuckoo_hash *hash,
                 const struct cuckoo_hash_item *hash_item)
{
    if (hash_item != NULL) {
      DEBUG_PRINT(">> hash_item[%p] key: %s, value: %lu\n",
              hash_item,
      (char *)hash_item->key,
      (intptr_t)   hash_item->value);
    }
      else
          DEBUG_PRINT("NULL\n");

  struct _cuckoo_hash_elem *elem =
    (hash_item != NULL
     ? ((struct _cuckoo_hash_elem *)
        ((char *) hash_item - offsetof(struct _cuckoo_hash_elem, hash_item))
        + 1)
     : hash->table);

  uint32_t bin_count = 1U << hash->power;
  /* return (hash->table + index * hash->bin_size); */
  struct _cuckoo_hash_elem *end = bin_at(hash, bin_count);
  uint32_t mask = bin_count - 1;

  while (elem != end) {

      DEBUG_PRINT(">> elem[%p] hash 1 %x, hash2 %x, key: %s, value: %lu end[%p]\n",
              elem, elem->hash1, elem->hash2,
      (char *)elem->hash_item.key,
      (intptr_t)   elem->hash_item.value, end);

      if (elem->hash1 != elem->hash2) {
          /*
            Test that the element is valid, i.e., its hash1 matches
            the bin index it resides in.
          */
          struct _cuckoo_hash_elem *bin = bin_at(hash, (elem->hash1 & mask));

          DEBUG_PRINT(">>>> bin[%p] hash 1 %x, hash2 %x, key: %s, value: %lu, bin_size %u\n",
                  bin, bin->hash1, bin->hash2,
          (char *)bin->hash_item.key,
          (intptr_t)   bin->hash_item.value,
          hash->bin_size);

// confirm the elem if belong to this bin range.
          if (bin <= elem && elem < bin + hash->bin_size) {
            DEBUG_PRINT("\n");
            return &elem->hash_item;
          }
        }

      ++elem;
    }

  return NULL;
}
