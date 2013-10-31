#include "list/list.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int max_buffer_size = 0;
int max_products = 0;
int consumed = 0;
int produced = 0;
List * buffer;
pthread_mutex_t buffer_access, products_consumed, products_produced;
sem_t empty, full;

/* Random sleep function */
void Sleep(float wait_time_ms) {
  wait_time_ms = ((float)rand())*wait_time_ms / (float)RAND_MAX;
  usleep((int) (wait_time_ms * 1e3f)); // convert from ms to us
}

int should_consume(void) {
  int r = 0;
  pthread_mutex_lock(&products_consumed);
  if (consumed < max_products) {
    consumed++;
    r = consumed;
  }
  pthread_mutex_unlock(&products_consumed);
  return r;
}

int should_produce(void) {
  int r = 0;
  pthread_mutex_lock(&products_produced);
  if (produced < max_products) {
    produced++;
    r = produced;
  }
  pthread_mutex_unlock(&products_produced);
  return r;
}

void * consume(void * param) {
  Node * node;
  int * id = (int *) param;
  
  // Loop
  while (should_consume()) {
    Sleep(200);
    sem_wait(&full);
    node = list_remove(buffer);
    sem_post(&empty);
    printf("Consumer %3d consumed %6s. Items in buffer: %3d (out of %3d).\n", *id, (char *) node->elm, buffer->len, max_buffer_size);
  }
  pthread_exit(0);
}

void * produce(void * param) {
  Node * node;
  int * id = (int *) param;
  int name;

  while((name = should_produce())) {
    Sleep(200);
    sem_wait(&empty);
    char str[7];
    sprintf(str, "Elm %d", name);
    node = node_new_str(str);
    list_add(buffer, node);
    sem_post(&full);
    printf("Producer %3d produced %6s. Items in buffer: %3d (out of %3d).\n", *id, str, buffer->len, max_buffer_size);
  }
  pthread_exit(0);
}

pthread_t start_thread(int cons, int * id) {
  pthread_t t;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  if (cons) {
    pthread_create(&t, &attr, consume, (void *) id);
  } else {
    pthread_create(&t, &attr, produce, (void *) id);
  }
  return t;
}

int main(int argc, char ** argv) {
  if (argc != 5) {
    printf("Usage: [Number of producers] [Number of consumers] [Size of shared buffer] [Number of products to produce]");
    exit(1);
  }

  // seed the random number generator
  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand(tv.tv_usec);

  // Init variables
  const int producers  = atoi(argv[1]);
   pthread_t ps[producers];
  const int consumers  = atoi(argv[2]);
  pthread_t cs[consumers];
  max_products   = atoi(argv[4]);
  int i;
  max_buffer_size = atoi(argv[3]);

  if (!(producers && consumers)) {
    printf("Error: Must have more than 0 producers and consumers.\n");
    exit(2);
  }

  buffer = list_new();
  printf("Initialising %d producers and %d consumers with a buffer size of %d\n", producers, consumers, max_buffer_size);

  pthread_mutex_init(&buffer_access, NULL);
  pthread_mutex_init(&products_produced, NULL);
  pthread_mutex_init(&products_consumed, NULL);
  
  sem_init(&empty, 0, max_buffer_size);
  sem_init(&full, 0, 0);

  // Start producers and consumers
  for (i = 0; i < producers; i++) ps[i] = start_thread(0, &i);
  for (i = 0; i < consumers; i++) cs[i] = start_thread(1, &i);
    
  // Join producers and consumers
  for (i = 0; i < producers; i++) pthread_join(ps[i], NULL);
  for (i = 0; i < consumers; i++) pthread_join(cs[i], NULL);

}
