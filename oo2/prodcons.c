#include "list/list.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int max_buffer_size = 0;
int max_products = 0;
List * buffer;
pthread_mutex_t buffer_access;
sem_t empty, full, products_consumed, products_produced;

/* Random sleep function */
void Sleep(float wait_time_ms) {
  wait_time_ms = ((float)rand())*wait_time_ms / (float)RAND_MAX;
  usleep((int) (wait_time_ms * 1e3f)); // convert from ms to us
}

int canConsume(void) {
  int products;
  sem_getvalue(&products_consumed, &products);
  return products < max_products;
}

int canProduce(void) {
  int products;
  sem_getvalue(&products_produced, &products);
  return products < max_products;
}

void * consume(void * param) {
  Node * node;
  int id = (int) param;

  while(canConsume()) {
    Sleep(200);
    sem_wait(&full);
    node = list_remove(buffer);
    printf("Consumer %3d consumed %6s. Items in buffer: %3d (out of %3d).\n", id, node->elm, buffer->len, max_buffer_size);
    sem_post(&empty);
    sem_post(&products_consumed);
  }
  pthread_exit(0);
}

void * produce(void * param) {
  Node * node;
  int id = (int) param;

  while(canProduce()) {
    Sleep(200);
    sem_wait(&empty);
    char str[6];
    sprintf(str, "Elm %d", buffer->len);
    node = node_new_str(str);
    list_add(buffer, node);
    sem_post(&full);
    printf("Producer %3d produced %6s. Items in buffer: %3d (out of %3d).\n", id, str, buffer->len, max_buffer_size);
    sem_post(&products_produced);
  }
  pthread_exit(0);
}

pthread_t start_thread(int cons, int id) {
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
    return 1;
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
  buffer = list_new();
  printf("Initialising %d producers and %d consumers with a buffer size of %d\n", producers, consumers, max_buffer_size);

  pthread_mutex_init(&buffer_access, NULL);
  sem_init(&empty, 0, max_buffer_size);
  sem_init(&full, 0, 0);

  // Start producers and consumers
  for (i = 0; i < producers; i++) ps[i] = start_thread(0, i);
  for (i = 0; i < consumers; i++) cs[i] = start_thread(1, i);
    
  // Join producers and consumers
  for (i = 0; i < producers; i++) pthread_join(ps[i], NULL);
  for (i = 0; i < consumers; i++) pthread_join(cs[i], NULL);

}
