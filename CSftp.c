#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"

#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#include <stdarg.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BACKLOG 10
#define BUFFER_SIZE 1024



// Here is an example of how to use the above function. It also shows
// one how to get the arguments passed on the command line.

// Server side C/C++ program to demonstrate Socket programming

void ftp_server(void *connected_client_socket);

int send_str(int peer, const char *fmt, ...);

int send_path(int peer, char *file, uint32_t offset);

int send_file(int peer, FILE *f);

int port_number;

int server_socket_fd;
int client_socket_fd;

struct sockaddr_in server;
struct sockaddr_in client;

socklen_t len = sizeof(struct sockaddr_in);

int main(int argc, char **argv) {
    // Check the command line arguments
    if (argc != 2) {
        usage(argv[0]);
        return -1;
    }

    port_number = atoi(argv[1]);

    // Socket Creation:
    // sockfd: socket descriptor, an integer (like a file-handle)
    // domain: integer, communication domain e.g.,
    //         AF_INET (IPv4 protocol) ,
    //         AF_INET6 (IPv6 protocol)
    // type: communication type
    //       SOCK_STREAM: TCP(reliable, connection oriented)
    //       SOCK_DGRAM: UDP(unreliable, connectionless)
    // protocol: Protocol value for Internet Protocol(IP), which is 0.
    //       This is the same number which appears on protocol field in the IP header
    //       of a packet.(man protocols for more details)
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd == 0) {
        perror("Socket connection failed.");
        exit(EXIT_FAILURE);
    }

    puts("Socket connected successfully.");

    // Initialize fields:
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port_number);


    socklen_t server_address_length = sizeof(server);

    // Bind:
    // After creation of the socket, bind function binds the socket to
    // the address and port number specified in addr(custom data structure).
    // In the example code, we bind the server to the localhost,
    // hence we use INADDR_ANY to specify the IP address.
    if (bind(server_socket_fd, (struct sockaddr *) &server, server_address_length) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    puts("Bind successfully.");

    // Listen:
    // It puts the server socket in a passive mode, where it waits for
    // the client to approach the server to make a connection. The backlog,
    // defines the maximum length to which the queue of pending connections
    // for sockfd may grow. If a connection request arrives when the queue
    // is full, the client may receive an error with an indication of ECONNREFUSED.
    if (listen(server_socket_fd, BACKLOG) < 0) {
        perror("Listen failed.");
        exit(EXIT_FAILURE);
    }

    puts("Waiting for a client.");

    while (1) {
        client_socket_fd = accept(server_socket_fd, (struct sockaddr *) &client, &len);
        puts("Connection accepted.");

        int *connected_client_socket;
        connected_client_socket = malloc(1);
        *connected_client_socket = client_socket_fd;

        ftp_server(connected_client_socket);

        puts("Connection closed.");
    }

    close(server_socket_fd);
    return EXIT_SUCCESS;

    if (client_socket_fd < 0) {
        perror("Accept failed.");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void ftp_server(void *connected_client_socket) {

    ssize_t read_buffer_size;

    int connected_socket = *(int *) connected_client_socket;

    struct sockaddr_in server_address;
    int server_address_length = sizeof(server_address);
    getsockname(connected_socket, (struct sockaddr *) &server_address, &server_address_length);

    char *server_message;
    char *client_message[2048];
    char *user_command[4];
    char *parameter[1024];

    char cwd[1024];

    int already_log_in = 0;
    int ascii_type = 0;
    int stream_mode = 0;
    int file_type = 0;
    int passive_mode = 0;

    int passive_port, passive_socket_fd;
    struct sockaddr_in passive_server;

    int data_client = -1;
    struct sockaddr_in data_client_address;
    int data_client_len = sizeof(data_client_address);

    server_message = "220 - Service ready for new user. Client connected to server successfully! \n";
    write(connected_socket, server_message, strlen(server_message));

    server_message = "220 - Specify your username. This server only takes the username \"cs317\".\n";
    write(connected_socket, server_message, strlen(server_message));

    while ((read_buffer_size = recv(connected_socket, client_message, 2000, 0)) > 0) {
        sscanf(client_message, "%s", user_command);

        puts("user command:");
        puts(user_command);

        // If not already logged in
        if (already_log_in == 0) {

            // the only accepted command is "USER"
            if (!strcasecmp(user_command, "USER")) {

                // Compare user typed name with "cs317"
                sscanf(client_message, "%s%s", user_command, parameter);

                if (!strcasecmp(parameter, "cs317")) {
                    already_log_in = 1;
                    send_str(connected_socket, "230 User logged in, proceed - Login successful.\n");
                }
                // Does not support any other username
                else {
                    send_str(connected_socket,
                             "504 Command not implemented for that parameter - This server only takes the username \"cs317\".\n");
                }
            } else {
                send_str(connected_socket, "530 Not logged in - Please log in before any other action.\n");
            }
        }
        // If already logged in
        else {
            // USER
            if (!strcasecmp(user_command, "USER")) {

                // Compare user typed name with "cs317"
                sscanf(client_message, "%s%s", user_command, parameter);

                if (!strcasecmp(parameter, "cs317")) {
                    send_str(connected_socket, "200 Command okay - Already logged in with \"cs317\", no need to log in again!\n");
                }
                else {
                    send_str(connected_socket, "200 Command okay - Can not change from \"cs317\" to other user.\n");
                }
            }

            // QUIT
            else if (!strcasecmp(user_command, "QUIT")){
                send_str(connected_socket, "221 Service closing control connection - User quit. Closing connection.\n");
                fflush(stdout);
                close(connected_socket);
                // close(pasv_sock);
                break;
            }

            // CWD - (4.1.1) For security reasons you are not accept any CWD command that starts with ./
            // or ../ or contains ../ in it. (hint: see the chdir system call.)
            else if (!strcasecmp(user_command, "CWD")){

            }

            // CDUP - (4.1.1) For security reasons do not allow a CDUP command to set the working directory
            // to be the parent directory of where your ftp server is started from. (hint: use the getcwd
            // system call to get the initial working directory so that you know where things are started
            // from and then if a CDUP command is received while in that diredtory return an appropriate error
            // code to the client.)
            else if (!strcasecmp(user_command, "CDUP")){
                getcwd(cwd, sizeof(cwd));
                puts(cwd);
            }

            // TYPE - (4.1.1) you are only to support the Image and ASCII type (3.1.1, 3.1.1.3)
            else if (!strcasecmp(user_command, "TYPE")){

                sscanf(client_message, "%s%s", user_command, parameter);

                if (!strcasecmp(parameter, "A")) {

                    if (ascii_type == 0) {
                        ascii_type = 1;
                        send_str(connected_socket, "200 Command okay - Setting TYPE to ASCII.\n");
                    } else {
                        send_str(connected_socket, "200 Command okay - Already in ASCII type.\n");
                    }
                }
                else if (!strcasecmp(parameter, "I")) {

                    if (ascii_type == 1) {
                        ascii_type = 0;
                        send_str(connected_socket, "200 Command okay - Setting TYPE to Image.\n");
                    } else {
                        send_str(connected_socket, "200 Command okay - Already in Image type.\n");
                    }
                }
                // Does not support any other username
                else {
                    send_str(connected_socket, "504 Command not implemented for that parameter - This server only supports TYPE A and TYPE I.\n");
                }
            }

            // MODE - you are only to support Stream mode (3.4.1)
            else if (!strcasecmp(user_command, "MODE")){

                sscanf(client_message, "%s%s", user_command, parameter);

                if (!strcasecmp(parameter, "S")) {
                    if (stream_mode == 0) {
                        stream_mode = 1;
                        send_str(connected_socket, "200 Command okay - Entering Stream mode.\n");
                    }
                    else {
                        send_str(connected_socket, "200 Command okay - Already in Stream mode.\n");
                    }
                }

                else {
                    send_str(connected_socket, "504 Command not implemented for that parameter - This server only supports MODE S.\n");
                }

            }

            // STRU - you are only to support File structure type (3.1.2, 3.1.2.1)
            else if (!strcasecmp(user_command, "STRU")){

                sscanf(client_message, "%s%s", user_command, parameter);

                if (!strcasecmp(parameter, "F")) {
                    if (file_type == 0) {
                        file_type = 1;
                        send_str(connected_socket, "200 Command okay - Data Structure set to File Structure.\n");
                    }
                    else {
                        send_str(connected_socket, "200 Command okay - Data Structure is already set to File Structure.\n");
                    }
                }
                else {
                    send_str(connected_socket, "504 Command not implemented for that parameter - This server only supports STRU F.\n");
                }
            }

            // RETR - (4.1.3)
            else if (!strcasecmp(user_command, "RETR")){

                if (passive_mode == 1) {

                    if (passive_port > 1024 && passive_port <= 65535 && passive_socket_fd >= 0) {
                        ascii_type = 0;

                        send_str(connected_socket, "150 File status okay - Opening binary mode data connection.\n");
                        listen(passive_socket_fd, BACKLOG);
                        data_client = accept(passive_socket_fd, (struct sockaddr *) &data_client_address, &data_client_len);

                        sscanf(client_message, "%s%s", user_command, parameter);

                        int st = send_path(data_client, parameter, 0);

                        if (st >= 0) {
                            send_str(connected_socket, "226 Closing data connection - Transfer complete.\n");
                        }

                        else {
                            send_str(connected_socket, "451 Requested action aborted: local error in processing - File not found.\n");
                        }

                        close(data_client);

                        close(passive_socket_fd);
                        passive_mode = 0;

                    }
                    else {
                        send_str(connected_socket, "500 - No passive server created.\n");
                    }
                }
                else {
                    send_str(connected_socket, "425 - Use PASV first.\n");
                }
            }

            // PASV - (4.1.1)
            else if (!strcasecmp(user_command, "PASV")){

                if (passive_mode == 0) {
                    // Loop until a passive socket is successfully created

                    do {
                        // Pick a random port number
                        passive_port = (rand() % 64512 + 1024);

                        // Create new socket
                        passive_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

                        passive_server.sin_family = AF_INET;
                        passive_server.sin_addr.s_addr = INADDR_ANY;
                        passive_server.sin_port = htons(passive_port);

                    } while (bind(passive_socket_fd,(struct sockaddr *)&passive_server , sizeof(passive_server)) < 0);

                    if (passive_socket_fd < 0) {
                        send_str(connected_socket, "500 - Error entering Passive mode.\n");
                    }
                    else {
                        listen(passive_socket_fd, 1);
                        passive_mode = 1;

                        uint32_t address = passive_server.sin_addr.s_addr;

                        send_str(connected_socket, "227 - Entering passive mode (%d,%d,%d,%d,%d,%d)\n",
                                 address&0xff, (address>>8)&0xff, (address>>16)&0xff, (address>>24)&0xff,
                                 passive_port>>8, passive_port & 0xff);
                    }
                }
                else {
                    send_str(connected_socket, "227 - Already in passive mode. Port number: %d\n", passive_port);
                }
            }

            // NLST - (4.1.3) to produce a directory listing, you are only required to support the version
            // of NLST with no command parameters. Respond with a 501 if the server gets an NLST with a parameter.
            else if (!strcasecmp(user_command, "NLST")){

                if (passive_mode == 1) {

                    if (passive_port > 1024 && passive_port <= 65535 && passive_socket_fd >= 0) {
                        ascii_type = 1;
                        send_str(connected_socket, "150 - Here comes the directory listing.\n");

                        listen(passive_socket_fd, BACKLOG);
                        data_client = accept(passive_socket_fd, (struct sockaddr *)& data_client_address, &data_client_len);

                        getcwd(cwd, sizeof(cwd));
                        listFiles(data_client, cwd);
                        send_str(connected_socket, "226 - Transfer complete.\n");

                        close(data_client);

                        close(passive_socket_fd);
                        passive_mode = 0;
                    }
                    else {
                        send_str(connected_socket, "500 - Error entering passive mode.\n");
                    }
                }
                else {
                    send_str(connected_socket, "425 - Use PASV first.\n");
                }
            }
            // All other command not accepted
            else{
                send_str(connected_socket, "502 Command not implemented - This server only supports: USER, QUIT, CWD, CDUP, TYPE, MODE, STRU, RETR, PASV, NLST.\n");
            }
        }

    }

    if (read_buffer_size == -1) {
        perror("recv failed");
    }
}

int send_str(int peer, const char *fmt, ...) {
    va_list args;
    char msgbuf[1024];
    va_start(args, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
    va_end(args);
    return write(peer, msgbuf, strlen(msgbuf));
}

int send_path(int peer, char *file, uint32_t offset) {
    FILE *f = fopen(file, "rb");
    if (f) {
        fseek(f, offset, SEEK_SET);
        int st = send_file(peer, f);
        if (st < 0) {
            return -2;
        }
    } else {
        return -1;
    }
    int ret = fclose(f);
    return ret == 0 ? 0 : -3;
}

int send_file(int peer, FILE *f) {
    char filebuf[1025];
    int n, ret = 0;
    while ((n = fread(filebuf, 1, 1024, f)) > 0) {
        int st = send(peer, filebuf, n, 0);

        if (st < 0) {
            ret = -1;
            break;
        } else {
            filebuf[n] = 0;
        }
    }
    return ret;
}