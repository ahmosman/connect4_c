// compile: gcc -Wall ./connect4Client.c -o ./outputs/connect4Client.o && ./outputs/connect4Client.o 127.0.0.1 1101

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 12345
#define BUFFER_SIZE 256

#define handle_error(msg)   \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

typedef struct
{
    bool your_turn;
    char message[BUFFER_SIZE];
} game_message_t;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <server_ip> <game_number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int game_number = atoi(argv[2]);
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        handle_error("socket");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
    {
        handle_error("inet_pton");
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        handle_error("connect");
    }

    write(sockfd, &game_number, sizeof(game_number));

    while (1)
    {
        game_message_t game_msg;
        int n = read(sockfd, &game_msg, sizeof(game_msg));
        if (n <= 0)
        {
            handle_error("read");
        }

        printf("Received: %s\n", game_msg.message);

        if (game_msg.your_turn)
        {
            printf("Enter message: ");
            fgets(buffer, BUFFER_SIZE, stdin);
            strncpy(game_msg.message, buffer, BUFFER_SIZE);
            game_msg.your_turn = false;
            write(sockfd, &game_msg, sizeof(game_msg));
        }
        else
        {
            // Clear any input provided by the user when it's not their turn
            while (fgets(buffer, BUFFER_SIZE, stdin) != NULL && buffer[0] != '\n')
            {
                printf("It's not your turn. Please wait.\n");
            }
        }
    }

    close(sockfd);
    return 0;
}