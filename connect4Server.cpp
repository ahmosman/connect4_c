// compile: g++ -Wall -std=c++11 ./connect4Server.cpp -o ../outputs/connect4Server.o && ../outputs/connect4Server.o

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
#include <iostream>
#include <unordered_map>
#include <typeinfo>

#define PORT 12345
#define MAX_CLIENTS 10
#define BUFFER_SIZE 256
#define ROWS 9
#define COLUMNS 8

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
    int current_player;
    char message[BUFFER_SIZE];
    int move;
    bool game_over;
    char board[ROWS][COLUMNS];
} game_message_t;

typedef struct
{
    char board[ROWS][COLUMNS];
    int current_player;
    bool game_over;
} game_state_t;

client_t clients[MAX_CLIENTS];
game_state_t games[MAX_CLIENTS / 2];
std::unordered_map<int, int> game_map; // Mapping game numbers to indices in the games array

void initialize_game(game_state_t *game)
{
    memset(game->board, ' ', sizeof(game->board));
    game->current_player = 1;
    game->game_over = false;
}

bool check_win(char board[ROWS][COLUMNS], int player)
{
    char symbol = (player == 1) ? '*' : '@';
    // Horizontal, vertical, and diagonal checks
    for (int i = 0; i < ROWS; i++)
    {
        for (int j = 0; j < COLUMNS; j++)
        {
            if (board[i][j] == symbol)
            {
                // Horizontal
                if (j <= COLUMNS - 4 && board[i][j + 1] == symbol && board[i][j + 2] == symbol && board[i][j + 3] == symbol)
                    return true;
                // Vertical
                if (i <= ROWS - 4 && board[i + 1][j] == symbol && board[i + 2][j] == symbol && board[i + 3][j] == symbol)
                    return true;
                // Diagonal (down-right)
                if (i <= ROWS - 4 && j <= COLUMNS - 4 && board[i + 1][j + 1] == symbol && board[i + 2][j + 2] == symbol && board[i + 3][j + 3] == symbol)
                    return true;
                // Diagonal (up-right)
                if (i >= 3 && j <= COLUMNS - 4 && board[i - 1][j + 1] == symbol && board[i - 2][j + 2] == symbol && board[i - 3][j + 3] == symbol)
                    return true;
            }
        }
    }
    return false;
}

bool make_move(game_state_t *game, int column, int player)
{
    if (column < 0 || column >= COLUMNS)
        return false;

    for (int i = ROWS - 1; i >= 0; i--)
    {
        if (game->board[i][column] == ' ')
        {
            game->board[i][column] = (player == 1) ? '*' : '@';
            return true;
        }
    }
    return false;
}

void handle_client_message(int client_index)
{
    game_message_t game_msg;
    int n = read(clients[client_index].fd, &game_msg, sizeof(game_msg));
    if (n <= 0)
    {
        // Client disconnected
        int pair_fd = clients[client_index].pair_fd;
        if (pair_fd != -1)
        {
            strncpy(game_msg.message, "Another player disconnected", BUFFER_SIZE);
            game_msg.game_over = true;
            write(pair_fd, &game_msg, sizeof(game_msg));
        }

        close(clients[client_index].fd);
        clients[client_index].active = false;
        clients[client_index].pair_fd = -1;
        return;
    }

    int game_number = clients[client_index].game_number;
    if (game_map.find(game_number) == game_map.end())
    {
        fprintf(stderr, "Invalid game number: %d\n", game_number);
        return;
    }


    int game_index = game_map[game_number];

    game_state_t *game = &games[game_index];

    if (game->game_over)
    {
        strncpy(game_msg.message, "Game over.", BUFFER_SIZE);
        game_msg.game_over = true;
    }
    else if (make_move(game, game_msg.move, game->current_player))
    {
        if (check_win(game->board, game->current_player))
        {
            game->game_over = true;
            sprintf(game_msg.message, "Player %d wins!", game->current_player);
            game->current_player = 0;
            game_msg.game_over = true;
        }
        else
        {
            game->current_player = (game->current_player == 1) ? 2 : 1;
            strncpy(game_msg.message, "Move accepted.", BUFFER_SIZE);
            game_msg.game_over = false;
        }
    }
    else
    {
        strncpy(game_msg.message, "Invalid move.", BUFFER_SIZE);
        game_msg.game_over = false;
    }

    game_msg.current_player = game->current_player;
    memcpy(game_msg.board, game->board, sizeof(game->board));

    write(clients[client_index].fd, &game_msg, sizeof(game_msg));
    write(clients[client_index].pair_fd, &game_msg, sizeof(game_msg));

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

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        handle_error("setsockopt");
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

            printf("New connection from %s:%d, game number: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), game_number);

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
                printf("Pair found.\n");
                clients[client_index].pair_fd = clients[pair_index].fd;
                clients[pair_index].pair_fd = clients[client_index].fd;
                clients[pair_index].turn = true; // First pair starts
                clients[client_index].turn = false;

                if (game_map.find(game_number) == game_map.end())
                {
                    for (int i = 0; i < MAX_CLIENTS / 2; i++)
                    {
                        if (games[i].current_player == 0) // Find empty game slot
                        {
                            printf("Game slot found: %d\n", i);
                            game_map[game_number] = i;
                            break;
                        }
                    }
                }

                // Send a message to the client who starts the game
                game_message_t game_msg;
                strncpy(game_msg.message, "You start the game.", BUFFER_SIZE);
                int game_index = game_map[game_number];

                initialize_game(&games[game_index]);
                memcpy(game_msg.board, games[game_index].board, sizeof(games[game_index].board));
                game_msg.game_over = games[game_index].game_over;
                game_msg.current_player = games[game_index].current_player;

                int player_number_1 = 1;
                int player_number_2 = 2;

                write(clients[pair_index].fd, &player_number_1, sizeof(player_number_1));
                write(clients[client_index].fd, &player_number_2, sizeof(player_number_2));
                write(clients[pair_index].fd, &game_msg, sizeof(game_msg));
                write(clients[client_index].fd, &game_msg, sizeof(game_msg));
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