#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <aio.h>
#include <ctime>

#define BUF_SIZE 40960 
#define TRUE 1
#define FALSE 0
#define MAX_SOCKETS 1000
#define FION_BIO 0x5421

using namespace std;

int main (int argc, char *argv[]) {

  int max_clients;
  int server_port;
  if(argc == 2){
    max_clients = atoi(argv[1]);
    server_port = 5555;
  }
  else if(argc == 3){
    max_clients = atoi(argv[1]);
    server_port = atoi(argv[2]);
  }
  else{
    cout << "Invalid syntax. ./2_a max_clients <server_port>" << endl;
    exit(1);
  }


  int rc, on = 1;
  int server_fd = -1, client_fd = -1;
  int end_server = FALSE;
  char buffer[BUF_SIZE];
  struct sockaddr_in   addr;
  struct aiocb aiocb[10000];
  int nfds = 0;

  //Create socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket() failed");
    exit(1);
  }
  //Set socket options
  rc = setsockopt(server_fd, SOL_SOCKET,  SO_REUSEADDR, (char *)&on, sizeof(on));
  if (rc < 0) {
    perror("setsockopt() failed");
    close(server_fd);
    exit(1);
  }


  memset(&addr, 0 , sizeof(addr));
  
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(server_port);

  //Bind socket
  rc = bind(server_fd,
    (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0) {
    perror("bind() failed");
    close(server_fd);
    exit(1);
  }

  //listen for client requests
  rc = listen(server_fd, 32);
  if (rc < 0) {
    perror("listen() failed");
    close(server_fd);
    exit(1);
  }

  memset(aiocb, 0 , sizeof(aiocb));
  int active_sockets = 0;
  struct timeval tmp;
  long int start = 0;

  while(nfds < max_clients || end_server == FALSE) {

    if(nfds == 0) {
      gettimeofday(&tmp, NULL);
      start = tmp.tv_sec * 1000 + tmp.tv_usec / 1000;
    }

    if(end_server == FALSE) {
      int finished_reads = 0;
      for (int i = 0; i < nfds; i++) {
        if(aiocb[i].aio_fildes == -1) {
          ++finished_reads;
          continue;
        }
        //Do asynchronous read
        int err = aio_error(&aiocb[i]);
        if(err == EINPROGRESS) {
          continue;
        } else if(err == 0) {
          int ret = aio_return(&aiocb[i]);
          if(ret == 0) {
            close(aiocb[i].aio_fildes);
            --active_sockets;
            aiocb[i].aio_fildes = -1;
            cout << "Closed connection : " << i << ", Active sockets: " << active_sockets << endl;
          } else {
            aiocb[i].aio_offset = aiocb[i].aio_offset+BUF_SIZE;
            if (aio_read(&aiocb[i]) == -1) {
              cout << "Error at aio_read(): " << strerror(errno) << endl;
              exit(1);
            }
          }
        } else {
          cout << "Error at aio_error() : " << strerror (err) << endl;
          exit(1);
        }
      }

      if(finished_reads >= max_clients) {
        end_server = TRUE;
      }
    }

    if(nfds < max_clients && active_sockets < MAX_SOCKETS) {
      client_fd = accept(server_fd, NULL, NULL);
      if (client_fd < 0) {
        continue;
      }
      cout << "Accepted connection : " << nfds << ", Active sockets: " << active_sockets << endl;
      struct aiocb temp;
      memset(&temp, 0, sizeof(struct aiocb));
      memset(buffer, 0xaa, BUF_SIZE);
      temp.aio_fildes = client_fd;
      temp.aio_buf = buffer;
      temp.aio_nbytes = BUF_SIZE;
      temp.aio_lio_opcode = LIO_WRITE;
      aiocb[nfds] = temp;
      if (aio_read(&aiocb[nfds]) == -1) {
        cout << "Error at aio_read(): " << strerror(errno) << endl;
        exit(1);
      }
      nfds++;
      active_sockets++;
    }

  }

  gettimeofday(&tmp, NULL);
  long int end = tmp.tv_sec * 1000 + tmp.tv_usec / 1000;
  cout << "Time for the server to handle all the incoming requests : " << end-start << " milli seconds " << endl;
  cout << "Number of requests handled per second : " << float(max_clients*1000)/float(end-start) << endl;

  return 0;
}
