#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUF_SIZE 1024

static volatile sig_atomic_t got_sighup = 0;
static int listen_fd = -1;
static int client_fd = -1;

static void handler(int sig) {
    (void)sig;
    got_sighup = 1;
}

static int create_listener(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, 5) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

static void close_client(void) {
    if (client_fd >= 0) {
        close(client_fd);
        client_fd = -1;
    }
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("sigaction");
        return 1;
    }

    sigset_t block_mask, empty_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGHUP);
    sigemptyset(&empty_mask);
    if (sigprocmask(SIG_BLOCK, &block_mask, NULL) < 0) {
        perror("sigprocmask");
        return 1;
    }

    listen_fd = create_listener();
    if (listen_fd < 0) return 1;
    printf("Сервер слушает порт %d. Жду соединений...\n", PORT);
    fflush(stdout);

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        int maxfd = listen_fd;

        if (client_fd >= 0) {
            FD_SET(client_fd, &readfds);
            if (client_fd > maxfd) maxfd = client_fd;
        }

        int ready = pselect(maxfd + 1, &readfds, NULL, NULL, NULL, &empty_mask);

        if (ready < 0) {
            if (errno == EINTR) {
                if (got_sighup) {
                    printf("[СИГНАЛ] Получен SIGHUP\n");
                    fflush(stdout);
                    got_sighup = 0;
                }
                continue;
            }
            perror("pselect");
            break;
        }

        if (FD_ISSET(listen_fd, &readfds)) {
            struct sockaddr_in cli;
            socklen_t cli_len = sizeof(cli);
            int new_fd = accept(listen_fd, (struct sockaddr*)&cli, &cli_len);
            if (new_fd >= 0) {
                if (client_fd < 0) {
                    client_fd = new_fd;
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
                    printf("[СОЕДИНЕНИЕ] Принято от %s:%d\n",
                           ip, ntohs(cli.sin_port));
                } else {
                    printf("[СОЕДИНЕНИЕ] Отклонено (уже есть активное)\n");
                    close(new_fd);
                }
                fflush(stdout);
            } else {
                perror("accept");
            }
        }

        if (client_fd >= 0 && FD_ISSET(client_fd, &readfds)) {
            char buf[BUF_SIZE];
            ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
            if (n > 0) {
                printf("[ДАННЫЕ] Получено %zd байт от клиента\n", n);
                fflush(stdout);
            } else if (n == 0) {
                printf("[СОЕДИНЕНИЕ] Клиент закрыл соединение\n");
                fflush(stdout);
                close_client();
            } else {
                if (errno != EINTR) {
                    perror("recv");
                    close_client();
                }
            }
        }
    }

    close_client();
    close(listen_fd);
    return 0;
}