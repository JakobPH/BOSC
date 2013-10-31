#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct _instr {
  int start, end;
  double sum;
} instr;

void * sum(void * instrPointer) {
  int i;
  instr * instr = instrPointer;
  for (i = instr->start; i <= instr->end; i++) {
    instr->sum += sqrt(i);
  }   
  pthread_exit(0);
}

long long sumsqrt(int threads, int n) {
  int i;
  struct timeval startTime, stopTime;

  // Calculate n / threads to split the calculations later (rounded up)
  int part = 1 + ((n - 1) / threads);

  // Set start time
  gettimeofday(&startTime, NULL);

  // Create the threads
  pthread_t ids[threads];
  instr instrs[threads];
  for (i = threads; i > 0; i--) {
    int start = n - part * i + 1;
    int end   = n - part * (i - 1);
    instr instr = {(start < 0 ? 0 : start), end, 0};
    instrs[i - 1] = instr;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&ids[i - 1], &attr, sum, &instrs[i - 1]);
  }

  // Join the threads
  double sum = 0;
  for (i = threads; i > 0; i--) {
    pthread_join(ids[i - 1], NULL);
    sum += instrs[i - 1].sum;
  }
  
  // Set stop time
  gettimeofday(&stopTime, NULL);
  return ((stopTime.tv_sec - startTime.tv_sec) * 1000000LL + 
          stopTime.tv_usec - startTime.tv_usec) / 1000;
}

int main(int argc, int * args[]) {
  // Retrieve the number of processors for optimal threading
  int sysThreads = sysconf(_SC_NPROCESSORS_ONLN);

  int i;
  long long sequential = sumsqrt(1, 10000000);
  printf("Thread : %d \t Speedup: %f \t Efficiency: %f\n", 1, 1.0, 1.0); 
  for (i = 2; i <= 14; i++) { 
    int p = pow(2, i);
    long t = sumsqrt(p, 10000000);
    double s = sequential / (double) t;
    double e = s / p;
    printf("Threads: %d \t Speedup: %f \t Efficiency: %f\n", p, s, e);
  }

}
