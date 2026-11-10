#include <stdio.h>        // printf(), perror()
#include <netdb.h>        // networking helpers; may not actually be needed yet
#include <netinet/in.h>   // sockaddr_in, INADDR_ANY
#include <stdlib.h>       // exit()
#include <string.h>       // string/memory functions
#include <sys/socket.h>   // socket(), bind(), listen(), accept(), send/recv
#include <sys/types.h>    // system types
#include <unistd.h>       // read(), write(), close()
#include <pthread.h>      // pthread_create(), mutexes, etc.
#include <stdbool.h>

#define MAX 80
#define PORT 8090
#define SA struct sockaddr
#define MAX_USERS 10
pthread_mutex_t lock;
int clients[MAX_USERS];
void removeClient(int id){
    int i = 0;
    pthread_mutex_lock(&lock);   
    while (i < MAX_USERS){
        if (clients[i] == id ){
            clients[i] = -1;
            close(id);
            break;
        }
        i++;
    }
    pthread_mutex_unlock(&lock);
    return;  
}
void sendMSG(int id, char msg[],int msgBytes ){
    int i = 0;
    pthread_mutex_lock(&lock);
    while (i < MAX_USERS){
        if (clients[i] == id || clients[i] == -1){
            i++;
            continue;
        }
        write(clients[i], msg, msgBytes);
        i++;
    }
    pthread_mutex_unlock(&lock);
}

void* chat(void* arg){
    int id = *(int*)arg;
    char buff[MAX];
    int n = 0;
    free(arg);

    for(;;){
        bzero(buff, MAX);
        n = read(id, buff, sizeof(buff) - 1);
        if (n < 0){
            perror("buff");
            removeClient(id);
            return NULL;
        }
        else if (n==0){
            removeClient(id);
            return NULL;
        }
        buff[n] = '\0';

        printf("From Client %d received %s\n", id, buff);
        sendMSG(id, buff, n);

    }

    return NULL;
}



int main(void){

    for (int i = 0; i < MAX_USERS; i++) {
        clients[i] = -1;
    }
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;
    pthread_t thread;
    pthread_mutex_init(&lock, NULL);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }


    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
        printf("socket bind failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully binded..\n"); 
  
    if ((listen(sockfd, 10)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server listening..\n"); 
    len = sizeof(cli); 
    for(;;){
        connfd = accept(sockfd, (SA*)&cli, &len); 
        if (connfd < 0) { 
            printf("server accept failed...\n"); 
            exit(0); 
        } 
        else
            printf("server accept the client...\n"); 
        int i = 0;
        bool space_availabe = false;
        pthread_mutex_lock(&lock);   
        while(i < MAX_USERS){
            if (clients[i] == -1){
                space_availabe = true;
                clients[i] = connfd;
                break;
            }
            i++;
        }
        pthread_mutex_unlock(&lock);

        if (!space_availabe){
            printf("server is full");
            close(connfd);
        }
        else{
            int *client_fd = malloc(sizeof(int));
            *client_fd = connfd;
            pthread_create(&thread, NULL, chat, client_fd);
            pthread_detach(thread);

        }
    }
}