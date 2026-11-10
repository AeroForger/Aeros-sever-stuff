// client2.c

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#define MAX_LINE 1024
#define MAX_NAME 50
#define MAX_MESSAGE (MAX_LINE - MAX_NAME - 2)
#define PORT 8110
#define SA struct sockaddr

static int send_all(int sockfd, const char *data, size_t length) {
    size_t sent = 0;

    while (sent < length) {
        ssize_t count = send(sockfd, data + sent, length - sent, MSG_NOSIGNAL);
        if (count <= 0) {
            return -1;
        }
        sent += (size_t)count;
    }
    return 0;
}

static int send_line(int sockfd, const char *data, size_t length) {
    char line[MAX_LINE + 1];

    if (length > MAX_LINE || memchr(data, '\n', length) != NULL) {
        return -1;
    }
    memcpy(line, data, length);
    line[length] = '\n';
    return send_all(sockfd, line, length + 1);
}

static int receive_line(int sockfd, char *buffer, size_t capacity) {
    size_t used = 0;

    for (;;) {
        char byte;
        ssize_t count = recv(sockfd, &byte, 1, 0);
        if (count <= 0) {
            return -1;
        }
        if (byte == '\n') {
            buffer[used] = '\0';
            return 0;
        }
        if (byte == '\r') {
            continue;
        }
        if (used + 1 >= capacity) {
            return -1;
        }
        buffer[used++] = byte;
    }
}

static int prompt_name(char *name, size_t capacity) {
    size_t length;

    printf("Please enter your name: ");
    if (fgets(name, (int)capacity, stdin) == NULL) {
        return -1;
    }

    length = strcspn(name, "\n");
    if (name[length] == '\n') {
        name[length] = '\0';
    }
    else if (!feof(stdin)) {
        int ch = getchar();
        if (ch != '\n' && ch != EOF) {
            while (ch != '\n' && ch != EOF) {
                ch = getchar();
            }
            fprintf(stderr, "Name must not exceed %zu characters.\n", capacity - 1);
            return -1;
        }
    }

    if (name[0] == '\0') {
        fprintf(stderr, "Name must not be empty and must fit within %zu characters.\n",
                capacity - 1);
        return -1;
    }
    return 0;
}

static int authenticate(int sockfd, int choice) {
    char name[50];
    char response[128];
    const char *operation = choice == 1 ? "SIGNUP" : "LOGIN";

    if (prompt_name(name, sizeof(name)) != 0) {
        return 0;
    }
    if (send_line(sockfd, operation, strlen(operation)) != 0 ||
        send_line(sockfd, name, strlen(name)) != 0) {
        perror("Could not send authentication request");
        return 0;
    }
    if (receive_line(sockfd, response, sizeof(response)) != 0) {
        fprintf(stderr, "Server closed the connection during authentication.\n");
        return 0;
    }

    puts(response);
    return strncmp(response, "OK ", 3) == 0;
}

static void chat(int sockfd) {
    struct pollfd fds[2] = {
        {.fd = STDIN_FILENO, .events = POLLIN},
        {.fd = sockfd, .events = POLLIN}
    };
    struct termios original;
    struct termios interactive;
    char input[MAX_MESSAGE + 1] = {0};
    char incoming[MAX_LINE + 1];
    size_t used = 0;
    int terminal = isatty(STDIN_FILENO);
    int terminal_changed = 0;
    int skip_lf = 0;
    int overflow = 0;
    int done = 0;

    if (terminal && tcgetattr(STDIN_FILENO, &original) == 0) {
        interactive = original;
        interactive.c_lflag &= (tcflag_t)~(ICANON | ECHO);
        interactive.c_cc[VMIN] = 1;
        interactive.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &interactive) == 0) {
            terminal_changed = 1;
        }
    }

    if (terminal) {
        fputs("> ", stdout);
        fflush(stdout);
    }
    while (!done) {
        int ready = poll(fds, 2, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            if (receive_line(sockfd, incoming, sizeof(incoming)) != 0) {
                if (terminal) {
                    fputc('\n', stdout);
                }
                puts("Disconnected from server.");
                break;
            }
            if (terminal) {
                printf("\r\n%s\n> %.*s", incoming, (int)used, input);
            }
            else {
                printf("%s\n", incoming);
            }
            fflush(stdout);
        }

        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            char chunk[256];
            ssize_t count = read(STDIN_FILENO, chunk, sizeof(chunk));
            if (count <= 0) {
                (void)send_line(sockfd, "/quit", sizeof("/quit") - 1);
                break;
            }

            for (ssize_t i = 0; i < count; i++) {
                unsigned char ch = (unsigned char)chunk[i];
                if (skip_lf) {
                    skip_lf = 0;
                    if (ch == '\n') {
                        continue;
                    }
                }
                if (ch == '\r' || ch == '\n') {
                    if (terminal) {
                        fputc('\n', stdout);
                    }
                    if (overflow) {
                        puts("Message is too long.");
                    }
                    else if (strcmp(input, "/quit") == 0) {
                        if (send_line(sockfd, input, used) != 0) {
                            perror("Could not send quit request");
                        }
                        done = 1;
                        break;
                    }
                    if (used == 0) {
                        if (terminal) {
                            puts("Messages cannot be empty.");
                        }
                    }
                    else if (send_line(sockfd, input, used) != 0) {
                        perror("Could not send message");
                        done = 1;
                        break;
                    }
                    used = 0;
                    overflow = 0;
                    input[0] = '\0';
                    if (ch == '\r') {
                        skip_lf = 1;
                    }
                    if (terminal) {
                        fputs("> ", stdout);
                        fflush(stdout);
                    }
                    continue;
                }
                if (ch == 4 && used == 0) {
                    (void)send_line(sockfd, "/quit", sizeof("/quit") - 1);
                    done = 1;
                    break;
                }
                if ((ch == 8 || ch == 127) && used > 0) {
                    input[--used] = '\0';
                    if (terminal) {
                        fputs("\b \b", stdout);
                        fflush(stdout);
                    }
                    continue;
                }
                if (ch >= 32 && ch != 127) {
                    if (used == MAX_MESSAGE) {
                        overflow = 1;
                        continue;
                    }
                    if (overflow) {
                        continue;
                    }
                    input[used++] = (char)ch;
                    input[used] = '\0';
                    if (terminal) {
                        fputc(ch, stdout);
                        fflush(stdout);
                    }
                }
            }
        }
    }

    if (terminal_changed) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &original);
        fputc('\n', stdout);
    }
}

static int start_client(int choice) {
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) != 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    if (!authenticate(sockfd, choice)) {
        close(sockfd);
        return -1;
    }

    chat(sockfd);
    (void)shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return 0;
}

int main(void) {
    char input[32];
    int choice;

    (void)setvbuf(stdin, NULL, _IONBF, 0);
    for (;;) {
        printf("\n1. Sign Up\n2. Login\n3. Exit\nSelect an option: ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            return 0;
        }
        if (sscanf(input, "%d", &choice) != 1) {
            printf("Invalid input. Please try again.\n");
            continue;
        }

        if (choice == 3) {
            puts("Goodbye!");
            return 0;
        }
        if (choice != 1 && choice != 2) {
            puts("Invalid selection. Please choose 1, 2, or 3.");
            continue;
        }

        if (start_client(choice) == 0) {
            return 0;
        }
    }
}
