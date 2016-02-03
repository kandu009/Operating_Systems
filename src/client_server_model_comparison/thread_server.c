#include <stdlib.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>

void *task1(int);

int main(int argc, char* argv[])
{
    int pId, portNo, listenFd;
    socklen_t len; 
    struct sockaddr_in svrAdd, clntAdd;
    struct timeval start,end;

    if(argc == 1){
    	portNo = 5555;
    }
    else if (argc == 2)
    {
    	portNo = atoi(argv[1]);
    }
    else{
    	printf("\nInvalid syntax. ./thread_server <portNo>");	
        exit(1);
    }
    
    //Create socket to listen
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    
    if(listenFd < 0)
    {
        printf("\nCannot create socket");
        exit(1);
    }
    
    //Set server address parameters
    bzero((char*) &svrAdd, sizeof(svrAdd));
    
    svrAdd.sin_family = AF_INET;
    svrAdd.sin_addr.s_addr = INADDR_ANY;
    svrAdd.sin_port = htons(portNo);
    
    //bind socket
    if(bind(listenFd, (struct sockaddr *)&svrAdd, sizeof(svrAdd)) < 0)
    {
        printf("\nBind error");
        exit(1);
    }
    
    //Listen for a maximum of SOMAXCONN connections in queue
    listen(listenFd,SOMAXCONN);
    
    len = sizeof(clntAdd);

    int connFd;

    gettimeofday(&start,NULL);

    //Accept connections in loop
    while (connFd = accept(listenFd, (struct sockaddr *)&clntAdd, &len))
    {

       printf("\nListening...");
       if (connFd < 0)
       {
        printf("\nError creating socket");
        exit(1);
    }
    else
    {
        printf("\nAccepted connection!");
    }

    pthread_t newThread;

    //Pass socket to thread
    pthread_create(&newThread, NULL, task1, connFd); 

}
gettimeofday(&end,NULL);

printf("Time taken is: %d",end.tv_sec - start.tv_sec);
}


//thread handler task
void *task1 (int arg)
{
    char buff[10240];
    int result;
    int connFd = arg;
    while(1)
    {   
        result=recv(connFd, buff, 10240 , 0);
        if(result==0) {
         break;
     }
     else if(result>0){
         printf("\nReceived data!");
     }

     else if(result <0){
        printf("\nError reading data!");   
    }
}
printf("Closing connection!");
close(connFd);
pthread_exit(NULL);
}

