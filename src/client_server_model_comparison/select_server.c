#include <sys/types.h>
#include <stdlib.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <errno.h>

#define ASYNC 0x0040

int main(int argc,char* argv[]){
   fd_set readset, tempset;
   int max_sd, flags;
   int srvsock, peersock, result;
   unsigned int len;
   char buffer[10240];
   struct sockaddr_in svrAdd;
   int portNo;
   struct timeval start,end;
   int client_socket[10000], max_clients = 10000,sd;
   int opt = 1;
   int num_socks = 0;
   int num_connections;
   int connection_i = 0;

   if(argc == 2){
      num_connections = atoi(argv[1]);
      portNo = 5555;
   }
   else if (argc == 3)
   {
      num_connections = atoi(argv[1]);
      portNo = atoi(argv[2]);
   }
   else{
      printf("\nSyntax : ./select_server num_connections <port>");
      exit(1);
   }

   gettimeofday(&start,NULL);
   //create socket
   srvsock = socket(AF_INET, SOCK_STREAM, 0);
   
   if(srvsock < 0)
   {
      printf("\nCannot open socket");
      exit(1);
   }
   
   bzero((char*) &svrAdd, sizeof(svrAdd));

   if (setsockopt(srvsock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
   {
      perror("setsockopt");
      exit(EXIT_FAILURE);
   }
   
   svrAdd.sin_family = AF_INET;
   svrAdd.sin_addr.s_addr = INADDR_ANY;
   svrAdd.sin_port = htons(portNo);
   
    //bind socket
   if(bind(srvsock, (struct sockaddr *)&svrAdd, sizeof(svrAdd)) < 0)
   {
      printf("\nCannot bind");
      exit(1);
   }

   int i;
   for (i = 0; i < max_clients; i++)
   {
      client_socket[i] = 0;
   }
   
   listen(srvsock, SOMAXCONN);
   max_sd = srvsock;

   while(1) {
      gettimeofday(&end,NULL);
      //Print time taken till this point
      printf("Time taken is: %f milliseconds",(end.tv_sec - start.tv_sec)*1000.0+(end.tv_usec-start.tv_usec)/1000.0);
      FD_ZERO(&readset);
      FD_SET(srvsock, &readset);
      for (i = 0; i < max_clients; i++)
      {
         //socket descriptor
         sd = client_socket[i];

         //if valid socket descriptor then add to read list
         if (sd > 0)
            FD_SET(sd, &readset);

         //highest file descriptor number, need it for the select function
         if (sd > max_sd)
            max_sd = sd;
      }
      printf("\nWaiting on select...");
      result = select(max_sd + 1, &readset, NULL, NULL, NULL);
      printf("\nSelect returned!");
      if (result == 0) {
         printf("\nselect() timed out!");
         break;
      }
      else if (result < 0 && errno != EINTR) {
         printf("Error in select():%s",strerror(errno));
      }
      else if (result > 0) {
         if (FD_ISSET(srvsock, &readset) && num_socks<=1000) {
            len = sizeof(svrAdd);
            peersock = accept(srvsock, (struct sockaddr *)&svrAdd, &len);
            printf("\nNew connection accepted!");
            connection_i++;
            if (peersock < 0) {
               printf("\nError in accept():%d",strerror(errno));
            }
            else {
              for (i = 0; i < max_clients; i++)
              {
            //if position is empty
               if (client_socket[i] == 0)
               {
                  fcntl(peersock, F_SETFL, ASYNC);
                  client_socket[i] = peersock;
                  num_socks++;
                  printf("Adding to list of sockets as %d\n", i);
                  break;
               }
            }
         }
         continue;
      }

      for (i=0; i<max_clients; i++) {
         sd = client_socket[i];
         if (FD_ISSET(sd, &readset)) {
            result = read(sd, buffer, 10240);
            if(result==0){
             close(sd);
             num_socks--;
             client_socket[i] = 0;
          }
          else if(result>0){
            printf("\nData received!");
         }
         else if(result<0 && result!=EINTR){
            printf("\nError in recv()");
         }
         break; 
            }      // end if (FD_ISSET(j, &tempset))
         }      // end for (;...)
      }      // end else if (result > 0)
   }
   gettimeofday(&end,NULL);
   printf("Time taken is: %f microseconds",(end.tv_sec - start.tv_sec)*1000.0+(end.tv_usec-start.tv_usec)/1000.0);
}
