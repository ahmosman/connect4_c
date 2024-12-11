// compile: g++ -Wall -std=c++11 ./connect4Client.cpp -o ../outputs/connect4Client.o && ../outputs/connect4Client.o 127.0.0.1 1101

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define PORT 12345
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
    bool your_turn;
    char message[BUFFER_SIZE];
    int move;
    bool game_over;
    char board[ROWS][COLUMNS];
} game_message_t;

char getch()
{
    char buf = 0;
    struct termios old = {0};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return buf;
}

void show_board(char board[ROWS][COLUMNS])
{
    for (int i = 0; i < ROWS; i++)
    {
        printf("|  ");
        for (int j = 0; j < COLUMNS; j++)
        {
            switch (board[i][j])
            {
            case ' ':
                printf("   |  ");
                break;
            case '*':
                printf("\x1B[31mO\033[0m  |  ");
                break;
            case '@':
                printf("\x1B[33mO\033[0m  |  ");
                break;
            default:
                printf("%c  |  ", board[i][j]);
                break;
            }
        }
        printf("\n\n");
    }
    printf("\n");
}

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

    int coordinates[2] = {0, 0};
    char move = 0, prechar = ' ';
    int player = 1;

    while (1)
    {
        game_message_t game_msg;
        int n = read(sockfd, &game_msg, sizeof(game_msg));
        if (n <= 0)
        {
            handle_error("read");
        }

        system("clear");
        printf("Received: %s\n", game_msg.message);
        show_board(game_msg.board);

        if (game_msg.game_over)
        {
            printf("Game over.\n");
            break;
        }

        if (game_msg.your_turn)
        {
            printf("X marks your position\nUse A/D to move, S to place a piece\nA - left\nD - right\nS - place\n");

            move = getch();
            switch (move)
            {
            case 'q':
                close(sockfd);
                return 0;
            case 'a':
                if (coordinates[1] > 0)
                {
                    coordinates[1]--;
                }
                break;
            case 'd':
                if (coordinates[1] < COLUMNS - 1)
                {
                    coordinates[1]++;
                }
                break;
            case 's':
                game_msg.move = coordinates[1];
                game_msg.your_turn = false;
                write(sockfd, &game_msg, sizeof(game_msg));
                break;
            }
        }
        else
        {
            printf("Waiting for opponent's move...\n");
        }
    }

    close(sockfd);
    return 0;
}