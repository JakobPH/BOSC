#include<stdio.h>
#include<stdlib.h>
#include <sys/time.h>
#include <pthread.h>

typedef struct state {
  int *resource;
  int *available;
  int **max;
  int **allocation;
  int **need;
} State;

// Global variables
int m, n;
State *s = NULL;

// Mutex for access to state.
pthread_mutex_t state_mutex;

/* Random sleep function */
void Sleep(float wait_time_ms)
{
  // add randomness
  wait_time_ms = ((float)rand())*wait_time_ms / (float)RAND_MAX;
  usleep((int) (wait_time_ms * 1e3f)); // convert from ms to us
}

/* Allocate resources in request for process i, only if it 
   results in a safe state and return 1, else return 0 */
int resource_request(int i, int *request)
{
  pthread_mutex_lock(&state_mutex);
  int a, l, r = 1, sum = 0;
  for (l = 0; l < m; l++) {
    if (s->max[i][l] - s->allocation[i][l] < request[l]) {
      r = 0;
      break;
    }
    sum += request[l];
  }
  a = s->available[i];
  pthread_mutex_unlock(&state_mutex);

  if (r || sum > a) {
    // Allocate the resources
    pthread_mutex_lock(&state_mutex);
    for (l = 0; l < m; l++) {
      s->allocation[i][l] = s->allocation[i][l] + request[l];
    }
    s->available[i] = sum;
    pthread_mutex_unlock(&state_mutex);
    return 0; 
  } else return 1;
}

/* Release the resources in request for process i */
void resource_release(int i, int *request)
{
  pthread_mutex_lock(&state_mutex);
  int l, v, sum = 0;
  for (l = 0; l < m; l++) {
    v = s->allocation[i][l] - request[l];
    if (v < 0) v = 0;
    s->allocation[i][l] = v;
    sum += v;
  }
  // Update available
  s->available[i] = sum;
  pthread_mutex_unlock(&state_mutex);
}

/* Generate a request vector */
void generate_request(int i, int *request)
{
  int j, sum = 0;
  while (!sum) {
    for (j = 0;j < n; j++) {
      request[j] = s->need[i][j] * ((double)rand())/ (double)RAND_MAX;
      sum += request[j];
    }
  }
  printf("Process %d: Requesting resources.\n",i);
}

/* Generate a release vector */
void generate_release(int i, int *request)
{
  int j, sum = 0;
  while (!sum) {
    for (j = 0;j < n; j++) {
      request[j] = s->allocation[i][j] * ((double)rand())/ (double)RAND_MAX;
      sum += request[j];
    }
  }
  printf("Process %d: Releasing resources.\n",i);
}

/* Threads starts here */
void *process_thread(void *param)
{
  /* Process number */
  int i = (int) (long) param, j;
  /* Allocate request vector */
  int *request = malloc(n*sizeof(int));
  while (1) {
    /* Generate request */
    generate_request(i, request);
    while (!resource_request(i, request)) {
      /* Wait */
      Sleep(100);
    }
    /* Generate release */
    generate_release(i, request);
    /* Release resources */
    resource_release(i, request);
    /* Wait */
    Sleep(1000);
  }
  free(request);
}

void * allocate(int ** arr, int i, int rows) {
  int n;
  for (n = 0; n < i; n++) {
    arr[n] = (int *) malloc(sizeof(int) * rows);
  }
}

void * freearr(int ** arr) {
  int i;
  for (i = 0; i < m; i++) free(arr[i]);
  free(arr);
}

int issafe(void) {
  int i, j, available, sum;
  pthread_mutex_lock(&state_mutex);
  for (i = 0; i < m; i++) {
    available = 0;
    sum = 0;
    for (j = 0; j < n; j++) {
      if (s->max[i][j] - s->allocation[i][j] != s->need[i][j]) {
	printf("Relationship between max, allocation and need unsafe");
        sum += s->allocation[i][j];
	return 0;
      }
      available = s->resource[i] - sum;
    }
    if (available != s->available[i]) {
      printf("A: %d  %d\n", available, s->available[i]);
      printf("The available vector is not safe.\n");
      return 0;
    }
  }
   
  pthread_mutex_unlock(&state_mutex);
  return 1;
}

int main(int argc, char* argv[])
{
  /* Get size of current state as input */
  int i, j;
  printf("Number of processes: ");
  scanf("%d", &m); // customers
  printf("Number of resources: ");
  scanf("%d", &n); 
  printf("Hi %d %d\n\n", n, m);

  /* Allocate memory for state */
  s = malloc(sizeof(State));
  s->resource   = malloc(sizeof(int) * m);
  s->available  = malloc(sizeof(int) * m);
  s->max        = malloc(sizeof(int *) * m);
  s->allocation = malloc(sizeof(int *) * m);
  s->need       = malloc(sizeof(int *) * m);
  allocate(s->max, m, n);
  allocate(s->allocation, m, n);
  allocate(s->need, m, n);

  // Init mutex
  pthread_mutex_init(&state_mutex, NULL);

  /* Get current state as input */
  printf("Resource vector: ");
  for(i = 0; i < n; i++)
    scanf("%d", &s->resource[i]);
  printf("Enter max matrix: ");
  for(i = 0;i < m; i++)
    for(j = 0;j < n; j++)
      scanf("%d", &s->max[i][j]);
  printf("Enter allocation matrix: ");
  for(i = 0; i < m; i++)
    for(j = 0; j < n; j++) {
      scanf("%d", &s->allocation[i][j]);
    }
  printf("\n");

  /* Calcuate the need matrix */
  for(i = 0; i < m; i++)
    for(j = 0; j < n; j++)
      s->need[i][j] = s->max[i][j]-s->allocation[i][j];

  /* Calcuate the availability vector */
  for(j = 0; j < n; j++) {
    int sum = 0;
    for(i = 0; i < m; i++)
      sum += s->allocation[i][j];
    s->available[j] = s->resource[j] - sum;
  }

  /* Output need matrix and availability vector */
  printf("Need matrix:\n");
  for(i = 0; i < n; i++)
    printf("R%d ", i+1);
  printf("\n");
  for(i = 0; i < m; i++) {
    for(j = 0; j < n; j++)
      printf("%d  ",s->need[i][j]);
    printf("\n");
  }
  printf("Availability vector:\n");
  for(i = 0; i < n; i++)
    printf("R%d ", i+1);
  printf("\n");
  for(j = 0; j < n; j++)
    printf("%d  ",s->available[j]);
  printf("\n");

  /* If initial state is unsafe then terminate with error */
  if (!issafe()) {
    printf("Initial state unsafe!. Exiting.\n");
    exit(1);
  }

  /* Seed the random number generator */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand(tv.tv_usec);
  
  /* Create m threads */
  pthread_t *tid = malloc(m*sizeof(pthread_t));
  for (i = 0; i < m; i++)
    pthread_create(&tid[i], NULL, process_thread, (void *) (long) i);
  
  /* Wait for threads to finish */
  pthread_exit(0);
  free(tid);

  /* Free state memory */
  freearr(s->allocation);
  freearr(s->max);
  freearr(s->need);
  free(s->resource);
  free(s->available);
  free(s);
  
}
