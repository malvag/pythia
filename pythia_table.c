
#include "pythia_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"

#define MAX_LOAD (128)
#define UNINITIALIZED (NULL)
#define REVERSE(KEY) reverseBits(KEY)
#define BITSNO 64
#define GET_PARENT(BUCKET) get_parent(BUCKET)

static marked_ptr_t mark_node(marked_ptr_t ptr, int tombstone) {
  ptr->tombstone = tombstone;
  return ptr;
}
static marked_ptr_t mark_node_with_mark(struct node_type *node, int tombstone) {
  marked_ptr_t new_mp = (marked_ptr_t)malloc(sizeof(struct _marked));
  new_mp->node = node;
  new_mp->tombstone = tombstone;
  return new_mp;
}
static unsigned get_parent(unsigned b) {
  int i;
  for (i = BITSNO - 1; i >= 0; --i) {
    if (b & (1 << i))
      return b & ~(1 << i);
  }
  return 0;
}

static struct node_type *get_node(marked_ptr_t marked_ptr) {
  return marked_ptr->node;
}

static unsigned int reverseBits(unsigned int num) {
  unsigned int reverse_num = 0;
  int i;
  for (i = 0; i < BITSNO; i++) {
    if ((num & (1 << i)))
      reverse_num |= 1 << ((BITSNO - 1) - i);
  }
  return reverse_num;
}

static pythia_key_t pythia_regular_key(pythia_key_t key) {
  return REVERSE(key) | 0x1;
}
static pythia_key_t pythia_dummy_key(pythia_key_t key) {
  return REVERSE(key & ~0x80000000);
}

static struct node_type *new_node(pythia_key_t key) {
  struct node_type *node = (struct node_type *)malloc(sizeof(struct node_type));
  if (!node) {
    perror("Could not allocate node memory");
    return NULL;
  }
  node->key = key;
  node->next = NULL;
  return node;
}

static int delete_node(struct node_type *node) {
  free(node);
  return 0;
}

static int list_find(marked_ptr_t *head, pythia_key_t key) {
  /*printf("list_find key %lx\n", key);*/
try_again:
  pythia_prev = head;
  pythia_curr = *pythia_prev;
  /* printf("starting node offt %lu\n", (uint64_t)pythia_curr);  */
  while (1) {
    if (get_node(pythia_curr) == NULL)
      return 0;

    pythia_next = get_node(pythia_curr)->next;

    unsigned ckey = get_node(pythia_curr)->key;
    if (*pythia_prev != mark_node(pythia_curr, 0))
      goto try_again;
    if (pythia_next) {
      if (!pythia_next->tombstone) {
        if (ckey >= key)
          return ckey == key;
        pythia_prev = &(get_node(pythia_curr)->next);
      } else {
        /*if there is a pending deletion, proceed with that first and
          then retry*/
        if (__sync_bool_compare_and_swap(pythia_prev, mark_node(pythia_curr, 0),
                                         mark_node(pythia_next, 0)))
          delete_node(get_node(pythia_curr));
        else
          goto try_again;
      }
      pythia_curr = pythia_next;
    } else {
      return 0;
    }
  }
  assert(0);
}

static int list_delete(marked_ptr_t *head, pythia_key_t key) {
  while (1) {
    if (!list_find(head, key))
      return 0;
    if (!__sync_bool_compare_and_swap(&(pythia_curr->node),
                                      mark_node(pythia_next, 0),
                                      mark_node(pythia_next, 1)))
      continue;
    if (__sync_bool_compare_and_swap(pythia_prev, mark_node(pythia_curr, 0),
                                     mark_node(pythia_next, 0)))
      delete_node(get_node(pythia_curr));

    else
      list_find(head, key);
    return 1;
  }
}

int list_insert(marked_ptr_t *head, struct node_type *node) {
  pythia_key_t key = node->key;
  while (1) {
    if (list_find(head, key))
      return 0;
    node->next = mark_node(pythia_curr, 0);
    if (__sync_bool_compare_and_swap(pythia_prev, mark_node(pythia_curr, 0),
                                     mark_node_with_mark(node, 0)))
      return 1;
  }
}

static void resize_table(marked_ptr_t **old_table_ref, unsigned new_size) {
  marked_ptr_t *new_table =
      (marked_ptr_t *)calloc(new_size, sizeof(marked_ptr_t));
  memcpy(new_table, *old_table_ref, sizeof(marked_ptr_t *) * (new_size / 2));
  if (!__sync_bool_compare_and_swap(old_table_ref, *old_table_ref, new_table)) {
    free(new_table);
    printf("failed to ");
  }
  /*printf("resize_table\n");*/
}

/*
The function insert creates a new node and assigns it a split-order key.

Note that the keys are stored in the nodes in their split-order form.
The bucket index is computed as key mod size.

If the bucket has not been initialized yet, initialize bucket is called.
Then, the node is inserted to the bucket by using list insert.

If the insertion is successful, one can proceed to increment
the item count using a fetch-and-inc operation.
A check is then performed to test whether the load factor has been exceeded.
If so, the table size is doubled,
causing a new segment of uninitialized buckets to be appended.
*/
int pythia_insert(pythia_key_t key) {
  struct node_type *node = new_node(pythia_regular_key(key));
  uint64_t bucket_id = key % pythia_size;
  /*
          printf(
                  "-->Attempt to insert: pythia_count_and_size %lu %lu and in
     bucket "
                  "%ld\n",
                  pythia_count, pythia_size, bucket_id);*/
  if ((*T)[bucket_id] == UNINITIALIZED)
    pythia_bucket_init(bucket_id);
  if (!list_insert(&((*T)[bucket_id]), node)) {
    delete_node(node);
    return 0;
  }
  uint64_t csize = pythia_size;
  __sync_fetch_and_add(&pythia_count, 1);
  if (pythia_count / csize > MAX_LOAD) {
    __sync_val_compare_and_swap(&pythia_size, csize, 2 * csize);
    /*printf("load_factor exceeded -> %ld \n", pythia_count / csize);*/
    resize_table(T, pythia_size);
  }

  return 1;
}

int pythia_delete(pythia_key_t key) {
  uint64_t bucket_id = key % pythia_size;
  if ((*T)[bucket_id] == UNINITIALIZED)
    pythia_bucket_init(bucket_id);
  if (!list_delete(&((*T)[bucket_id]), pythia_regular_key(key)))
    return 0;

  __sync_fetch_and_sub(&pythia_count, 1);
  return 1;
}

int pythia_find(pythia_key_t key) {
  uint64_t bucket_id = key % pythia_size;
  if ((*T)[bucket_id] == UNINITIALIZED)
    pythia_bucket_init(bucket_id);

  return list_find(&((*T)[bucket_id]), pythia_regular_key(key));
}

void pythia_bucket_init(uint64_t bucket_id) {
  uint64_t parent = GET_PARENT(bucket_id);
  /*printf("initializing bucket %ld with parent %ld\n", bucket_id, parent);*/
  if ((*T)[parent] == UNINITIALIZED && parent != 0)
    pythia_bucket_init(parent);
  struct node_type *dummy = new_node(pythia_dummy_key(bucket_id));
  if (!list_insert(&((*T)[parent]), dummy)) {
    delete_node(dummy);
    dummy = pythia_curr->node;
  }
  (*T)[bucket_id] = calloc(1, sizeof(struct _marked));
  /*  GET_MARK(T[bucket_id]) = 1;*/
  (*T)[bucket_id]->node = dummy;
}

int main() {
  int i;
  pythia_size = 2;
  T = (marked_ptr_t **)malloc(sizeof(marked_ptr_t *));
  *T = (marked_ptr_t *)calloc(pythia_size, sizeof(marked_ptr_t));
  (*T)[0] = (marked_ptr_t)calloc(1, sizeof(struct _marked));

  for (i = 0; i < 20000000; i++) {
    pythia_insert(i);
    pythia_insert(201);
    pythia_insert(301);
    pythia_insert(501);
    pythia_insert(601);
    pythia_insert(701);
    pythia_insert(801);
    pythia_insert(901);
    pythia_insert(1);
    /*if (i % 100000 == 0)
      printf("%d\n", i);*/
  }
  printf("-----------------------\n");

/*   for (i = 0; i < 2000000; i++)
     pythia_find(i);*/

  return 0;
}