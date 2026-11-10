#include <arpa/inet.h> 
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> 
#include <sys/socket.h>
#include <unistd.h> 
#include <pthread.h>

#define MAX 80
#define PORT 8090
#define SA struct sockaddr

void chat(int sockfd){
    char buff[MAX];
    int n;

    for (;;){
        bzero(buff, sizeof(buff));
        printf("\n enter the msg: ");
        n = 0;

        while (n < sizeof(buff)-1 && (buff[n++] = getchar()) != '\n')
            ;
        if (n == 1 && buff[0] == '\n'){
            printf("You cant send an empty char");
            continue;
        }
        write(sockfd, buff, n);
    }
}

void *receiveMSG(void *arg){

    int id = *(int*)arg;
    char buff[MAX];
    int n = 0;

    for(;;){
        bzero(buff, MAX);
        n = read(id, buff, sizeof(buff) - 1);
        if (n < 0){
            perror("buff");
            return NULL;
        }
        else if (n==0){
            return NULL;
        }
        buff[n] = '\0';

        printf("\n received %s\n", buff);

    }

    return NULL;

}

int main()
{
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))
        != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    pthread_t receiver;

    pthread_create(&receiver, NULL, receiveMSG, &sockfd);
    chat(sockfd);

    close(sockfd);
}