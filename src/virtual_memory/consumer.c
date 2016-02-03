#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
  char result[512];

  int n = 0;
  int fd = open("/dev/scullBuffer0",O_RDONLY);
  n = read(fd, result, 512);

  printf("Consumer #%d read %d bytes from the scullBuffer\n", atoi(argv[1]), n);
  if( n != 0) {
    printf("Consumer #%d read \"%s\"\n", atoi(argv[1]), result);
  }
  close(fd);
  printf("Consumer #%d closed the scullBuffer\n", atoi(argv[1]));
  return 0;
}

