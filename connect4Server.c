// compile: gcc -Wall ./connect4Server.c -o ./outputs/connect4Server.o && ./outputs/connect4Server.o

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#define PORT 12345
#define MAX_CLIENTS 10
#define BUFFER_SIZE 256

#define handle_error(msg)   \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

typedef struct
{
    int fd;
    int pair_fd;
    bool active;
    bool turn;
    int game_number;
} client_t;

typedef struct
{
    bool your_turn;
    char message[BUFFER_SIZE];
} game_message_t;

client_t clients[MAX_CLIENTS];

void handle_client_message(int client_index)
{
    game_message_t game_msg;
    int n = read(clients[client_index].fd, &game_msg, sizeof(game_msg));
    if (n <= 0)
    {
        close(clients[client_index].fd);
        clients[client_index].active = false;
        clients[client_index].pair_fd = -1;
        return;
    }

    // Prepare the message to send to the paired client
    game_msg.your_turn = true;

    // Send the message to the paired client
    write(clients[client_index].pair_fd, &game_msg, sizeof(game_msg));

    // Switch turns
    clients[client_index].turn = false;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd == clients[client_index].pair_fd)
        {
            clients[i].turn = true;
            break;
        }
    }
}

int main()
{
    int server_fd, new_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    struct pollfd poll_fds[MAX_CLIENTS + 1];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        handle_error("socket");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        handle_error("bind");
    }

    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        handle_error("listen");
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].fd = -1;
        clients[i].pair_fd = -1;
        clients[i].active = false;
        clients[i].turn = false;
        clients[i].game_number = -1;
    }

    poll_fds[0].fd = server_fd;
    poll_fds[0].events = POLLIN;

    while (1)
    {
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            poll_fds[i + 1].fd = clients[i].fd;
            poll_fds[i + 1].events = POLLIN;
        }

        int poll_count = poll(poll_fds, MAX_CLIENTS + 1, -1);
        if (poll_count < 0)
        {
            handle_error("poll");
        }

        if (poll_fds[0].revents & POLLIN)
        {
            new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
            if (new_fd < 0)
            {
                perror("accept");
                continue;
            }

            int game_number;
            read(new_fd, &game_number, sizeof(game_number));

            int client_index = -1;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (!clients[i].active)
                {
                    clients[i].fd = new_fd;
                    clients[i].active = true;
                    clients[i].game_number = game_number;
                    client_index = i;
                    break;
                }
            }

            if (client_index == -1)
            {
                close(new_fd);
                continue;
            }

            int pair_index = -1;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].active && clients[i].fd != new_fd && clients[i].pair_fd == -1 && clients[i].game_number == game_number)
                {
                    pair_index = i;
                    break;
                }
            }

            if (pair_index != -1)
            {
                clients[client_index].pair_fd = clients[pair_index].fd;
                clients[pair_index].pair_fd = clients[client_index].fd;
                clients[pair_index].turn = true; // First pair starts
                clients[client_index].turn = false;

                // Send a message to the client who starts the game
                game_message_t game_msg = {.your_turn = true, .message = "You start the game."};
                write(clients[pair_index].fd, &game_msg, sizeof(game_msg));
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].active && clients[i].turn && (poll_fds[i + 1].revents & POLLIN))
            {
                handle_client_message(i);
            }
        }
    }

    close(server_fd);
    return 0;
}