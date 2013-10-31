#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* format(int time) {
  char * pointer = malloc(11 * sizeof(char));
  int left;
  int days = time / 86400;
  left = (time - days * 86400);
  int hours = left / 3600;
  int seconds = (left - hours * 3600) / 60;
  sprintf(pointer, "%dd %dh %ds", days, hours, seconds);
  return pointer;
}

void search() {
  FILE * stackFile;
  char str[512];
  stackFile = fopen("/proc/stat", "r");
  int line = 0;
  while (fgets(str, 512, stackFile)) {
    line = line + 1;
    int user, x, kernel, idle;
    if (sscanf(str, "cpu %d %d %d %d", &user, &x, &kernel, &idle)) {
      char * userTime = format(user);
      char * kernelTime = format(kernel);
      char * idleTime = format(idle);
      printf("User-time: %s\tKernel-time: %s\tIdle-time: %s\n", userTime, kernelTime, idleTime);
      free(userTime);
      free(kernelTime);
      free(idleTime);
      fclose(stackFile);
      return;
    }
   
  }
  fclose(stackFile);
}

int main(int argc, char **args) {
  search();
  
  exit(0);
}
