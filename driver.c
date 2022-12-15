#include "pythia_table.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

void *workload(void *vargp) {
  int tid = *((int *)vargp);
  int i;
  printf("Thread ID: %d, starting at %d to %d\n", tid, 10000000 * (tid),
         10000000 * ((tid) + 1));

  for (i = 10000000 * (tid); i < (10000000 * ((tid) + 1)); i++) {
    pythia_insert(i,"HAHA");
    if (i % 1000000 == 0)
      printf("Thread%d key %d\n", tid, i);
  }
  return NULL;
}

int main(int argc, char **argv) {
  static int i;
  int ids[] = {0, 1, 2, 3, 4, 5, 6, 7};
  int lost_keys = 0;
  if (argc > 1) {
    int threads = atoi(argv[1]);
    pthread_t *tid = (pthread_t *)malloc(sizeof(pthread_t) * threads);
    pythia_init();

    for (i = 0; i < threads; i++) {
      pthread_create(&tid[i], NULL, workload, &ids[i]);
    }
    for (i = 0; i < threads; i++)
      pthread_join(tid[i], NULL);
    for (i = 0; i < 10000000 * threads; i++)
      if (!pythia_find(i)) {
        printf("Lost %d\n", i);
        lost_keys++;
      }
  } else {
    pythia_init();
    for (i = 0; i < 20000000; i++) {
      // pythia_insert(i);
      // pythia_insert(201);
      // pythia_insert(301);
      // pythia_insert(501);
      // pythia_insert(601);
      // pythia_insert(701);
      // pythia_insert(801);
      // pythia_insert(901);
      // pythia_insert(1);
      if (i % 1000000 == 0)
        printf("%d\n", i);
    }
    printf("-----------------------\n");
  }
  printf("Lost %d keys\n", lost_keys);

  pythia_destroy();
  return 0;
}