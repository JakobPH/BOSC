/******************************************************************************
   main.c

   Implementation of a simple FIFO buffer as a linked list defined in list.h.

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "list.h"

void * test_list_add(void * param) {
  List * list = param;
  int i;
  for (i = 0; i < 100; i++) {
    char str[3];
    sprintf(str, "s%d", i);
    list_add(list, node_new_str(str));
  }
  pthread_exit(0);
}

void * test_list_remove(void * param) {
  List * list = param;
  int i;
  for (i = 0; i < 100; i++) {
    list_remove(list);
  }
  pthread_exit(0);
}

void test(int thread_number) {
  pthread_t threads[thread_number];
  pthread_attr_t attr;  
  List * list = list_new();
  Node * node;
  int i, max;

  for (i = 0; i < thread_number; i++) {
    pthread_attr_init(&attr);
    pthread_create(&threads[i], &attr, test_list_add, list);
  }

  for (i = 0; i < thread_number; i++) {
    pthread_join(threads[i], NULL);
  }

  // Run through the list to check the integrity
  node = list->first;
  i = -1; // Take the first null element into account
  while (node != NULL) {
    i++;
    node = node->next;
  }
    
  if (i != list->len) {
    printf("Failure on adding %d numbers with %d threads\n", thread_number, i);
    return;
  }
  max = list->len;

  for (i = 0; i < thread_number; i++) {
    pthread_attr_init(&attr);
    pthread_create(&threads[i], &attr, test_list_remove, list);
  }

  for (i = 0; i < thread_number; i++) {
    pthread_join(threads[i], NULL);
  }

  // Test the length
  if (list->len == 0) {
    printf("Success when testing list with %d elemets.\n", max);
  } else {
    printf("Failure when testing list. Expected length 0, got length %d\n", list->len);
  }
}

int main(int argc, char* argv[])
{
  if (argc != 2) {
    printf("Usage: [Number of threads]");
    return 1;
  }
  
  test(atoi(argv[1]));

  return 0;

}
