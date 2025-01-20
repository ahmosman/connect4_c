# Connect4 Game

This project implements a simple Connect4 game using TCP sockets and poll in C++. The game consists of a server and multiple clients. Clients can connect to the server, join a game, and take turns to play.

## Compilation and Execution

### Server

To compile and run the server:

```sh
g++ -Wall -std=c++11 ./connect4Server.cpp -o ./outputs/connect4Server.o
./outputs/connect4Server.o
```

### Client

To compile and run the client:

```sh
g++ -Wall -std=c++11 ./connect4Client.cpp -o ./outputs/connect4Client.o
./outputs/connect4Client.o <server_ip> <game_number>
```

Replace `<server_ip>` with the IP address of the server and `<game_number>` with the game number you want to join.

## Dependencies

- GCC (GNU Compiler Collection)
- Standard C++ libraries

## License

This project is licensed under the MIT License.