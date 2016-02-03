#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
  int n = 0;
  int fd = open("/dev/scullBuffer0",O_WRONLY);
  char hello[] = "Testing - Write random bytes into scullBuffer";


  int i = atoi(argv[2]);
  sleep(i);
  n = write(fd, hello, 512);

  printf("Producer #%d deposited %d bytes to the scullBuffer\n", atoi(argv[1]), n);
  close(fd);
  printf("Producer #%d has closed the scullBuffer\n", atoi(argv[1]));
  return 0;
}
