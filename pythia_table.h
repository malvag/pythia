
#ifndef _PYTHIA_TABLE_H_
#define _PYTHIA_TABLE_H_ 
#include <stdint.h>

/*
The counter is initially 0, 
and the buckets are set as uninitialized,
except the first one, 
    - which points to a node of key 0, 
    - whose next pointer is set to NULL.
*/
typedef struct _marked* marked_ptr_t;
typedef uint64_t pythia_key_t;


struct _marked{
    uint8_t tombstone;
    struct node_type* node;
} __attribute__((packed));

struct node_type{
    pythia_key_t key;
    marked_ptr_t next;
};



/*
They are set by list find to point at the nodes around the searched key,
and are subsequently used by the same thread to refer to these nodes in
other functions.
----------------------------------------------------------------- */

/*
The fetch-and-inc operation can be implemented in a lock-free manner
via a simple repeated loop of CAS operations, which as we show,
given the low access rates, has a negligible performance overhead.
*/




/*-----------------------------------------------------------------
The role of initialize_bucket is to direct the pointer in the array cell of the index bucket.
The value assigned is the address of a new dummy node containing the dummy key bucket. 

First, the dummy node is created and inserted to an existing bucket, parent.

Then, the cell is assigned the node’s address.
    If the parent bucket is not initialized, 
    the function is called recursively with parent.
In order to control the recursion,
we maintain the invariant that parent < bucket, where “<” is the regular order among keys.

It is also wise to choose parent to be as close as possible to bucket in the list,
but still preceding it. 
This value is achieved by calling the GET_PARENT macro that unsets bucket’s most significant turned-on bit.
If the exact dummy key already exists in the list,
it may be the case that some other process tried to initialize the same bucket,
but for some reason has not completed the second step.
In this case, list insert will fail, but the private variable cur will point to the node holding the dummy key.
The newly created dummy node can be freed and the value of cur used.
*/
void pythia_bucket_init(uint64_t bucket_id);
int pythia_insert_ycsb(const char *key);
int pythia_insert(pythia_key_t key);
int pythia_delete(pythia_key_t key);
int pythia_find(pythia_key_t key);
void pythia_destroy();
void pythia_init();

#endif