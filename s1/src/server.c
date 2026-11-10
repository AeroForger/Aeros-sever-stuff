#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h> 
#define MAX 80 
#define PORT 8080 
#define SA struct sockaddr 

void chat(int id){
    char buff[MAX];
    int n = 0;

    for(;;){
        bzero(buff, MAX); //idk what bzero does ig buffer zero

        n = read(id, buff, sizeof(buff) - 1);
        if (n < 0){
            printf("error occured");
            exit(0);
        }
        else if (n == 0){
            return;
        }
        buff[n] = '\0';

        printf("From client: %s\t To client : ", buff);

        bzero(buff, MAX);
        n = 0;
        
        while (n < sizeof(buff) - 1 &&
            (buff[n++] = getchar()) != '\n')
            ;
        buff[n] = '\0';

        write(id, buff, n); 
  

        if (strncmp("exit", buff, 4) == 0) { 
            printf("Server Exit...\n"); 
            break; 
        } 
        
    }   
}

int main(void){
    int sockfd, connfd, len;
// sockfd = listening socket
// connfd = socket for the accepted client connection
// len = size of client address structure
    struct sockaddr_in servaddr, cli; 

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully created..\n"); 
    bzero(&servaddr, sizeof(servaddr)); 

    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(PORT); 

    // we bind our socket to the server addr and check if it worked
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
        printf("socket bind failed...\n"); 
        perror("bind");
        exit(0); 
    } 
    else
        printf("Socket successfully binded..\n"); 

        if ((listen(sockfd, 5)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server listening..\n"); 

    len = sizeof(cli); //idk this

    connfd = accept(sockfd, (SA*)&cli, &len); 
    if (connfd < 0) { 
        printf("server accept failed...\n"); 
        exit(0); 
    } 
    else
        printf("server accept the client...\n"); 

    chat(connfd);

    close(connfd);
    close(sockfd); // we close the socket
}