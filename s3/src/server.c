#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "accounts.h"

#define MAX_FRAME 1024
#define MAX_NAME 50
#define MAX_MESSAGE (MAX_FRAME - MAX_NAME - 2)
#define PORT 8100
#define SA struct sockaddr
#define MAX_USERS 10

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int clients[MAX_USERS];

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
    char line[MAX_FRAME + 1];

    if (length > MAX_FRAME || memchr(data, '\n', length) != NULL) {
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

static int open_user_db(sqlite3 **db) {
    static const char *paths[] = {
        "database/users.db",
        "s3/database/users.db",
        "../database/users.db"
    };
    size_t i;
    const char *path = "database/users.db";

    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        if (access(paths[i], F_OK) == 0) {
            path = paths[i];
            break;
        }
    }

    return sqlite3_open(path, db);
}

static int authenticate_user(int sockfd, struct User *user) {
    char operation[16];
    char name[sizeof(user->name)];
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    int rc;
    int success = 0;

    if (receive_line(sockfd, operation, sizeof(operation)) != 0 ||
        receive_line(sockfd, name, sizeof(name)) != 0 ||
        name[0] == '\0') {
        (void)send_line(sockfd, "ERR Invalid authentication request.",
                         sizeof("ERR Invalid authentication request.") - 1);
        return 0;
    }

    if (strcmp(operation, "SIGNUP") != 0 && strcmp(operation, "LOGIN") != 0) {
        (void)send_line(sockfd, "ERR Unknown authentication operation.",
                         sizeof("ERR Unknown authentication operation.") - 1);
        return 0;
    }

    rc = open_user_db(&db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Could not open user database: %s\n",
                db != NULL ? sqlite3_errmsg(db) : "SQLite allocation failure");
        (void)send_line(sockfd, "ERR Account service unavailable.",
                         sizeof("ERR Account service unavailable.") - 1);
        sqlite3_close(db);
        return 0;
    }

    rc = sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS Users("
        "Id INTEGER PRIMARY KEY, Name TEXT, Connfid INT);",
        NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Could not create Users table: %s\n", sqlite3_errmsg(db));
        (void)send_line(sockfd, "ERR Account service unavailable.",
                         sizeof("ERR Account service unavailable.") - 1);
        sqlite3_close(db);
        return 0;
    }

    if (strcmp(operation, "SIGNUP") == 0) {
        rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Could not start signup transaction: %s\n",
                    sqlite3_errmsg(db));
            (void)send_line(sockfd, "ERR Account service unavailable.",
                             sizeof("ERR Account service unavailable.") - 1);
            sqlite3_close(db);
            return 0;
        }

        rc = sqlite3_prepare_v2(db,
            "SELECT Id FROM Users WHERE Name = ?;",
            -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(stmt);
        }
        if (rc == SQLITE_ROW) {
            (void)sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            (void)send_line(sockfd, "ERR That name is already registered.",
                             sizeof("ERR That name is already registered.") - 1);
        }
        else if (rc == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            stmt = NULL;
            rc = sqlite3_prepare_v2(db,
                "INSERT INTO Users (Name) VALUES (?);",
                -1, &stmt, NULL);
            if (rc == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
                rc = sqlite3_step(stmt);
            }
            if (rc == SQLITE_DONE) {
                user->id = (int)sqlite3_last_insert_rowid(db);
                rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
                if (rc == SQLITE_OK) {
                    success = 1;
                }
                else {
                    fprintf(stderr, "Could not commit account creation: %s\n",
                            sqlite3_errmsg(db));
                    (void)sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
                    (void)send_line(sockfd, "ERR Could not create account.",
                                     sizeof("ERR Could not create account.") - 1);
                }
            }
            else {
                (void)sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
                fprintf(stderr, "Could not create account: %s\n", sqlite3_errmsg(db));
                (void)send_line(sockfd, "ERR Could not create account.",
                                 sizeof("ERR Could not create account.") - 1);
            }
        }
        else {
            (void)sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            fprintf(stderr, "Could not check account: %s\n", sqlite3_errmsg(db));
            (void)send_line(sockfd, "ERR Account service unavailable.",
                             sizeof("ERR Account service unavailable.") - 1);
        }
    }
    else {
        rc = sqlite3_prepare_v2(db,
            "SELECT Id FROM Users WHERE Name = ?;",
            -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(stmt);
        }
        if (rc == SQLITE_ROW) {
            user->id = sqlite3_column_int(stmt, 0);
            success = 1;
        }
        else if (rc == SQLITE_DONE) {
            (void)send_line(sockfd, "ERR No account found. Please sign up first.",
                             sizeof("ERR No account found. Please sign up first.") - 1);
        }
        else {
            fprintf(stderr, "Could not find account: %s\n", sqlite3_errmsg(db));
            (void)send_line(sockfd, "ERR Account service unavailable.",
                             sizeof("ERR Account service unavailable.") - 1);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (success) {
        memcpy(user->name, name, strlen(name) + 1);
    }
    return success;
}

static int register_client(int sockfd) {
    size_t i;
    int registered = 0;

    pthread_mutex_lock(&lock);
    for (i = 0; i < MAX_USERS; i++) {
        if (clients[i] == -1) {
            clients[i] = sockfd;
            registered = 1;
            break;
        }
    }
    pthread_mutex_unlock(&lock);
    return registered;
}

static void remove_client(int sockfd) {
    size_t i;

    pthread_mutex_lock(&lock);
    for (i = 0; i < MAX_USERS; i++) {
        if (clients[i] == sockfd) {
            clients[i] = -1;
            break;
        }
    }
    pthread_mutex_unlock(&lock);
    close(sockfd);
}

static void broadcast(int excluded_client, const char *message) {
    size_t i;

    pthread_mutex_lock(&lock);
    for (i = 0; i < MAX_USERS; i++) {
        if (clients[i] != -1 && clients[i] != excluded_client &&
            send_line(clients[i], message, strlen(message)) != 0) {
            fprintf(stderr, "Could not forward message to client %d\n", clients[i]);
        }
    }
    pthread_mutex_unlock(&lock);
}

static void broadcast_event(int excluded_client, const char *event,
                            const char *username) {
    char message[sizeof(((struct User *)0)->name) + 32];
    int length = snprintf(message, sizeof(message), "%s %s", event, username);

    if (length < 0 || (size_t)length >= sizeof(message)) {
        fprintf(stderr, "Could not format %s event for %s\n", event, username);
        return;
    }
    broadcast(excluded_client, message);
}

static void *handle_client(void *arg) {
    int sockfd = *(int *)arg;
    char buffer[MAX_FRAME + 1];
    char message[MAX_FRAME + 1];
    struct User user = {0};
    int length;

    free(arg);
    if (!authenticate_user(sockfd, &user)) {
        close(sockfd);
        return NULL;
    }
    if (!register_client(sockfd)) {
        (void)send_line(sockfd, "ERR Server is full.",
                         sizeof("ERR Server is full.") - 1);
        close(sockfd);
        return NULL;
    }
    if (send_line(sockfd, "OK Authentication successful.",
                   sizeof("OK Authentication successful.") - 1) != 0) {
        remove_client(sockfd);
        return NULL;
    }

    printf("User %s (id %d) connected.\n", user.name, user.id);
    broadcast_event(sockfd, "JOIN", user.name);
    for (;;) {
        if (receive_line(sockfd, buffer, sizeof(buffer)) != 0) {
            break;
        }

        if (strcmp(buffer, "/quit") == 0) {
            break;
        }

        if (strlen(buffer) > MAX_MESSAGE) {
            (void)send_line(sockfd, "ERR Message is too long.",
                             sizeof("ERR Message is too long.") - 1);
            continue;
        }
        printf("From %s: %s\n", user.name, buffer);
        length = snprintf(message, sizeof(message), "%s: %s", user.name, buffer);
        if (length < 0 || (size_t)length >= sizeof(message)) {
            fprintf(stderr, "Message from %s exceeds the supported size.\n", user.name);
            continue;
        }
        broadcast(sockfd, message);
    }

    remove_client(sockfd);
    broadcast_event(-1, "LEAVE", user.name);
    printf("User %s disconnected.\n", user.name);
    return NULL;
}

int main(void) {
    int sockfd;
    int connfd;
    int opt = 1;
    socklen_t len;
    struct sockaddr_in servaddr;
    struct sockaddr_in cli;

    for (size_t i = 0; i < MAX_USERS; i++) {
        clients[i] = -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return 1;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(sockfd);
        return 1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (SA *)&servaddr, sizeof(servaddr)) != 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }
    if (listen(sockfd, 10) != 0) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    puts("Server listening...");
    for (;;) {
        pthread_t thread;
        int *client_fd;

        len = sizeof(cli);
        connfd = accept(sockfd, (SA *)&cli, &len);
        if (connfd < 0) {
            perror("accept");
            continue;
        }

        client_fd = malloc(sizeof(*client_fd));
        if (client_fd == NULL) {
            perror("malloc");
            close(connfd);
            continue;
        }
        *client_fd = connfd;
        if (pthread_create(&thread, NULL, handle_client, client_fd) != 0) {
            perror("pthread_create");
            free(client_fd);
            close(connfd);
            continue;
        }
        pthread_detach(thread);
    }
}
