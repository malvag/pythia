#include "pythia_table.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#define WSIZE 10000000

void *workload(void *vargp) {
  uint64_t tid = *((uint64_t *)vargp);
  uint64_t i;
  printf("Thread ID: %ld, starting at %ld to %ld\n", tid, WSIZE * (tid),
         WSIZE * ((tid) + 1));

  for (i = WSIZE * (tid); i < (WSIZE * ((tid) + 1)); i++) {
    char *key = (char *)malloc(27);
    sprintf(key, "malvag%lu", i);

    pythia_insert(key, "data");
    if (i % (WSIZE / 10) == 0)
      printf("Thread%ld key %ld\n", tid, i);
  }
  return NULL;
}

int main(int argc, char **argv) {
  static uint64_t i;
  uint64_t ids[] = {0, 1, 2, 3, 4, 5, 6, 7};
  int lost_keys = 0;
  if (argc > 1) {
    uint64_t threads = atoi(argv[1]);
    pthread_t *tid = (pthread_t *)malloc(sizeof(pthread_t) * threads);
    pythia_init();

    for (i = 0; i < threads; i++) {
      pthread_create(&tid[i], NULL, workload, &ids[i]);
    }
    for (i = 0; i < threads; i++)
      pthread_join(tid[i], NULL);

    for (i = 0; i < 10000000 * threads; i++) {
      char *key = (char *)malloc(27);
      sprintf(key, "malvag%lu", i);
      int value = pythia_find(key);

      if (!value) {
        // printf("Lost %lu\n", i);
        lost_keys++;
      }
    }
  } else {
    pythia_init();
    for (i = 0; i < (WSIZE * 1); i++) {
      char *key = (char *)malloc(27);
      sprintf(key, "malvag%lu", i);

      pythia_insert(key, "data");
      if (i % (WSIZE / 10) == 0)
        printf("Thread%d key %ld\n", 0, i);
    }
    printf("-----------------------\n");
    for (i = 0; i < 10000000; i++) {
      char *key = (char *)malloc(27);
      sprintf(key, "malvag%lu", i);
      int value = pythia_find(key);

      if (!value) {
        // printf("Lost %lu\n", i);
        lost_keys++;
      }
    }
  }

  printf("Lost %d keys\n", lost_keys);

  pythia_destroy();
  return 0;
}