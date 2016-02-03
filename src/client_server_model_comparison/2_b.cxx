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
#include <ctime>

#define BUF_SIZE 12288 
#define TRUE 1
#define FALSE 0
#define MAX_SOCKETS 1000
#define ASYNC 0x0040

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
  struct sockaddr_in addr;

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
  rc = bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0) {
    perror("bind() failed");
    close(server_fd);
    exit(1);
  }

  rc = listen(server_fd, 32);
  if (rc < 0) {
    perror("listen() failed");
    close(server_fd);
    exit(1);
  }

  int active_sockets = 0;
  int client_fds[10000];
  int nfds = 0;
  struct timeval temp;
  long int start = 0;

  while(nfds < max_clients || end_server == FALSE) {

    if(nfds == 0) {
      gettimeofday(&temp, NULL);
      start = temp.tv_sec * 1000 + temp.tv_usec / 1000;
    }

    if(end_server == FALSE) {
      int fin = 0;
      for (int i = 0; i < nfds; ++i) {
        if(client_fds[i] == -1) {
          ++fin;
          continue;
        }
        memset(buffer, 0xaa, BUF_SIZE);
        //Do asynchronous read
        int ret = read(client_fds[i], buffer, BUF_SIZE);
        if(ret < 0) {
          if(errno == EINTR) {
            continue;
          } else {
            cout << "Error at read(): " << strerror(errno) << endl;
            exit(1);
          }
        } else if(ret == 0) {
          close(client_fds[i]);
          --active_sockets;
          client_fds[i] = -1;
          cout << "Closed connection : " << i << endl;
        }
      }
      if(fin >= max_clients) {
        end_server = TRUE;
      }
    }

    if(nfds < max_clients && active_sockets < MAX_SOCKETS) {
      client_fd = accept(server_fd, NULL, NULL);
      if (client_fd < 0) {
        continue;
      }
      //Set socket to asynchronous read
      fcntl(client_fd, F_SETFL, ASYNC);
      cout << "Accepted connection : " << nfds << endl;
      client_fds[nfds] = client_fd;
      nfds++;
      active_sockets++;
    }

    if(end_server == TRUE) {
      break;
    }

  }

  gettimeofday(&temp, NULL);
  long int end = temp.tv_sec * 1000 + temp.tv_usec / 1000;

  cout << "Time for the server to handle all the incoming requests : " << end-start << " milli seconds " << endl;
  cout << "Number of requests handled per second : " << float(max_clients*1000)/float(end-start) << endl;

  return 0;
}
