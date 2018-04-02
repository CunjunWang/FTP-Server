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

#include <time.h>

#define BACKLOG 10

// Here is an example of how to use the above function. It also shows
// one how to get the arguments passed on the command line.

void ftpServer(int *connected_client_socket);

int sendMessage(int socket_fd, const char *format, ...);

int sendPath(int socket_fd, char *path, uint32_t offset);

int fileExchange(int peer, FILE *f);

unsigned long getIPAddress(int connected_socket_fd);

int port_number;

int server_socket_fd;
int client_socket_fd;

struct sockaddr_in server;
struct sockaddr_in client;

socklen_t len = sizeof(struct sockaddr_in);

int main(int argc, char **argv) {
    // Check the command line arguments
    port_number = atoi(argv[1]);
    if ((argc != 2) || (port_number < 1024) || (port_number > 65535)) {
        usage(argv[0]);
        return -1;
    }

    // the server socket
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

    // allow reuse the same port number
    int optval = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

    if (bind(server_socket_fd, (struct sockaddr *) &server, server_address_length) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    puts("Bind successfully.");

    if (listen(server_socket_fd, BACKLOG) < 0) {
        perror("Listen failed.");
        exit(EXIT_FAILURE);
    }

    puts("Waiting for a client.");

    while (1) {
        // the client connecting this server
        client_socket_fd = accept(server_socket_fd, (struct sockaddr *) &client, &len);

        if (client_socket_fd < 0) {
            perror("Accept failed.");
            exit(EXIT_FAILURE);
        }

        puts("Connection accepted.");

        int *connected_client_socket;
        connected_client_socket = (int *) malloc(sizeof(int));
        *connected_client_socket = client_socket_fd;

        ftpServer(connected_client_socket);

        free(connected_client_socket);
        puts("Connection closed.");
        puts("Waiting for a client.");
    }
}

void ftpServer(int *connected_client_socket) {

    int connected_socket = *connected_client_socket;

    // track the status
    int already_log_in = 0;
    int ascii_type = 0;
    int stream_mode = 0;
    int file_type = 0;
    int passive_mode = 0;

    struct sockaddr_in server_address;
    socklen_t server_address_length = sizeof(server_address);
    getsockname(connected_socket, (struct sockaddr *) &server_address, &server_address_length);

    char *server_message;
    char *client_message[1024];
    char *user_command[4];
    char *parameter[16];

    char cwd[512];

    int passive_port;
    int passive_socket_fd;
    struct sockaddr_in passive_server;
    int passive_address_len = sizeof(passive_server);

    int data_client;
    struct sockaddr_in data_client_socket;
    int data_client_len = sizeof(data_client_socket);

    server_message = "220 - Service ready for new user. Client connected to server successfully!\n";
    write(connected_socket, server_message, strlen(server_message));

    server_message = "220 - Specify your username. This server only takes the username \"cs317\".\n";
    write(connected_socket, server_message, strlen(server_message));

    ssize_t read_buffer_size;
    while ((read_buffer_size = recv(connected_socket, client_message, 2000, 0)) > 0) {

        sscanf(client_message, "%s", user_command);

        char copy[1024];
        strcpy(copy, client_message);

        int counter = 0;

        char *pch;
        pch = strtok(copy, " ");
        while (pch != NULL) {
            pch = strtok(NULL, " ");
            counter++;
        }
        counter--;

        // QUIT
        if (!strcasecmp(user_command, "QUIT")) {
            printf("counter in quit is: %d", counter);
            if (counter != 0) {
                counter = 0;
                server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                sendMessage(connected_socket, server_message);
            } else {
                counter = 0;
                server_message = "221 Service closing control connection - User quit. Closing connection.\n";
                sendMessage(connected_socket, server_message);
                fflush(stdout);
                close(data_client);
                close(passive_socket_fd);
                close(connected_socket);
                break;
            }
        }

        // If not already logged in
        if (already_log_in == 0) {

            // the only accepted command is "USER"
            if (!strcasecmp(user_command, "USER")) {
                if (counter != 1) {
                    counter = 0;
                    server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                    sendMessage(connected_socket, server_message);
                } else {
                    counter = 0;
                    // Compare user typed name with "cs317"
                    sscanf(client_message, "%s%s", user_command, parameter);

                    if (!strcasecmp(parameter, "cs317")) {
                        already_log_in = 1;
                        server_message = "230 User logged in, proceed - Login successful.\n";
                        sendMessage(connected_socket, server_message);
                    } else { // Does not support any other username
                        server_message = "504 Command not implemented for that parameter - This server only takes the username \"cs317\".\n";
                        sendMessage(connected_socket, server_message);
                    }
                }
            } else if (!strcasecmp(user_command, "QUIT")) {
                counter = 0;
                continue;
            } else {
                counter = 0;
                server_message = "530 Not logged in - Please log in before any other action.\n";
                sendMessage(connected_socket, server_message);
            }
        } else {  // If already logged in
            // USER
            if (!strcasecmp(user_command, "USER")) {
                if (counter != 1) {
                    counter = 0;
                    server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                    sendMessage(connected_socket, server_message);
                } else {
                    counter = 0;

                    // Compare user typed name with "cs317"
                    sscanf(client_message, "%s%s", user_command, parameter);

                    if (!strcasecmp(parameter, "cs317")) {
                        server_message = "200 Command okay - Already logged in with \"cs317\", no need to log in again!\n";
                        sendMessage(connected_socket, server_message);
                    } else {
                        server_message = "200 Command okay - Can not change from \"cs317\" to other user.\n";
                        sendMessage(connected_socket, server_message);
                    }
                }
            } else if (!strcasecmp(user_command, "CWD")) {
                // CWD - (4.1.1) For security reasons you are not accept any CWD command that starts with ./
                // or ../ or contains ../ in it. (hint: see the chdir system call.)

                if (counter != 1) {
                    counter = 0;
                    server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                    sendMessage(connected_socket, server_message);
                } else {
                    counter = 0;

                    sscanf(client_message, "%s%s", user_command, parameter);

                    char start_path[128];
                    getcwd(start_path, sizeof(start_path));
                    getcwd(cwd, sizeof(cwd));

                    if ((strcmp(cwd, start_path) == 0) && strncmp("..", parameter, 2) == 0) {
                        server_message = "550 No access - failed to change directory. Can not go up further at this point!\n";
                        sendMessage(connected_socket, server_message);
                        continue;
                    }

                    if (strstr(parameter, "../") != NULL || strncmp("./", parameter, 2) == 0) {
                        server_message = "550 No access - failed to change directory. Can not go up further at this point!\n";
                        sendMessage(connected_socket, server_message);
                        continue;
                    }

                    if (chdir(parameter) == -1) {
                        server_message = "550 No access - failed to change directory.\n";
                        sendMessage(connected_socket, server_message);
                    } else {
                        server_message = "250 - Directory successfully changed.\n";
                        sendMessage(connected_socket, server_message);
                    }
                }
            } else if (!strcasecmp(user_command, "CDUP")) {
                // CDUP - (4.1.1) For security reasons do not allow a CDUP command to set the working directory
                // to be the parent directory of where your ftp server is started from. (hint: use the getcwd
                // system call to get the initial working director y so that you know where things are started
                // from and then if a CDUP command is received while in that diredtory return an appropriate error
                // code to the client.)
                if (counter != 0) {
                    counter = 0;
                    server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                    sendMessage(connected_socket, server_message);
                } else {
                    counter = 0;

                    char start_path[128];
                    getcwd(start_path, sizeof(start_path));
                    getcwd(cwd, sizeof(cwd));

                    if (strcmp(cwd, start_path) == 0) {
                        server_message = "550 - failed to change directory. Can not go up further at this point!\n";
                        sendMessage(connected_socket, server_message);
                    } else {
                        if (chdir("../") == -1) {
                            server_message = "550 - failed to change directory.\n";
                            sendMessage(connected_socket, server_message);
                        } else {
                            server_message = "200 - Command okay.\n";
                            sendMessage(connected_socket, server_message);
                        }
                    }
                }
            } else if (!strcasecmp(user_command, "wd")) {
                // test command to show current working directory
                getcwd(cwd, sizeof(cwd));
                puts(cwd);

            } else if (!strcasecmp(user_command, "TYPE")) {
                // TYPE - (4.1.1) you are only to support the Image and ASCII type (3.1.1, 3.1.1.3)

                if (counter != 1) {
                    counter = 0;
                    server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                    sendMessage(connected_socket, server_message);
                } else {
                    counter = 0;

                    sscanf(client_message, "%s%s", user_command, parameter);

                    if (!strcasecmp(parameter, "A")) {
                        if (ascii_type == 0) {
                            ascii_type = 1;
                            server_message = "200 Command okay - Setting TYPE to ASCII.\n";
                            sendMessage(connected_socket, server_message);
                        } else {
                            server_message = "200 Command okay - Already in ASCII type.\n";
                            sendMessage(connected_socket, server_message);
                        }
                    } else if (!strcasecmp(parameter, "I")) {

                        if (ascii_type == 1) {
                            ascii_type = 0;
                            server_message = "200 Command okay - Setting TYPE to Image.\n";
                            sendMessage(connected_socket, server_message);
                        } else {
                            server_message = "200 Command okay - Already in Image type.\n";
                            sendMessage(connected_socket, server_message);
                        }
                    } else {
                        // Does not support any other username
                        server_message = "504 Command not implemented for that parameter - This server only supports TYPE A and TYPE I.\n";
                        sendMessage(connected_socket, server_message);
                    }
                }
            } else if (!strcasecmp(user_command, "MODE")) {
                // MODE - you are only to support Stream mode (3.4.1)
                if (counter != 1) {
                    counter = 0;
                    server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                    sendMessage(connected_socket, server_message);
                } else {
                    counter = 0;
                    sscanf(client_message, "%s%s", user_command, parameter);

                    if (!strcasecmp(parameter, "S")) {
                        if (stream_mode == 0) {
                            stream_mode = 1;
                            server_message = "200 Command okay - Entering Stream mode.\n";
                            sendMessage(connected_socket, server_message);
                        } else {
                            server_message = "200 Command okay - Already in Stream mode.\n";
                            sendMessage(connected_socket, server_message);
                        }
                    } else {
                        server_message = "504 Command not implemented for that parameter - This server only supports MODE S.\n";
                        sendMessage(connected_socket, server_message);
                    }
                }
            } else if (!strcasecmp(user_command, "PASV")) { // PASV - (4.1.1)
                if (counter != 0) {
                    counter = 0;
                    server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                    sendMessage(connected_socket, server_message);
                } else {
                    counter = 0;
                    if (passive_mode == 0) {
                        // Loop until a passive socket is successfully created
                        do {
                            // Pick a random port number
                            srand(time(NULL));
                            int r = rand();
                            passive_port = (r % 64512 + 1024);

                            // Create new socket
                            passive_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

                            passive_server.sin_family = AF_INET;
                            passive_server.sin_addr.s_addr = INADDR_ANY;
                            passive_server.sin_port = htons(passive_port);
                        } while (bind(passive_socket_fd, (struct sockaddr *) &passive_server, sizeof(passive_server)) <
                                 0);

                        if (passive_socket_fd < 0) {
                            server_message = "500 - Error entering Passive mode.\n";
                            sendMessage(connected_socket, server_message);
                        } else {
                            listen(passive_socket_fd, 1);
                            passive_mode = 1;

                            int address = getIPAddress(connected_socket);
                            sendMessage(connected_socket, "passive address: %d\n", address);
                            sendMessage(connected_socket, "227 - Entering passive mode (%d,%d,%d,%d,%d,%d)\n",
                                        address & 0xff, (address >> 8) & 0xff,
                                        (address >> 16) & 0xff, (address >> 24) & 0xff,
                                        (passive_port >> 8) & 0xff, passive_port & 0xff);
                        }
                    } else {
                        sendMessage(connected_socket, "227 - Already in passive mode. Port number: %d\n", passive_port);
                    }
                }
            } else if (!strcasecmp(user_command, "STRU")) {
                // STRU - you are only to support File structure type (3.1.2, 3.1.2.1)
                if (counter != 0) {
                    counter = 0;
                    server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                    sendMessage(connected_socket, server_message);
                } else {
                    counter = 0;
                    sscanf(client_message, "%s%s", user_command, parameter);

                    if (!strcasecmp(parameter, "F")) {
                        if (file_type == 0) {
                            file_type = 1;
                            server_message = "200 Command okay - Data Structure set to File Structure.\n";
                            sendMessage(connected_socket, server_message);
                        } else {
                            server_message = "200 Command okay - Data Structure is already set to File Structure.\n";
                            sendMessage(connected_socket, server_message);
                        }
                    } else {
                        server_message = "504 Command not implemented for that parameter - This server only supports STRU F.\n";
                        sendMessage(connected_socket, server_message);
                    }
                }
            } else if (!strcasecmp(user_command, "RETR")) {
                // RETR - (4.1.3)
                if (counter != 0) {
                    counter = 0;
                    server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                    sendMessage(connected_socket, server_message);
                } else {
                    counter = 0;
                    if (passive_mode == 1) {

                        if (passive_port > 1024 && passive_port <= 65535 && passive_socket_fd >= 0) {
                            ascii_type = 0;
                            server_message = "150 File status okay - Opening binary mode data connection.\n";
                            sendMessage(connected_socket, server_message);
                            listen(passive_socket_fd, BACKLOG);
                            data_client = accept(passive_socket_fd, (struct sockaddr *) &data_client_socket,
                                                 &data_client_len);

                            sscanf(client_message, "%s%s", user_command, parameter);

                            int st = sendPath(data_client, parameter, 0);

                            if (st >= 0) {
                                server_message = "226 Closing data connection - Transfer complete.\n";
                                sendMessage(connected_socket, server_message);
                            } else {
                                server_message = "451 Requested action aborted: local error in processing - File not found.\n";
                                sendMessage(connected_socket, server_message);
                            }

                            close(data_client);

                            close(passive_socket_fd);
                            passive_mode = 0;

                        } else {
                            server_message = "500 - No passive server created.\n";
                            sendMessage(connected_socket, server_message);
                        }
                    } else {
                        server_message = "425 - Use PASV first.\n";
                        sendMessage(connected_socket, server_message);
                    }
                }
            } else if (!strcasecmp(user_command, "NLST")) {
                // NLST - (4.1.3) to produce a directory listing, you are only required to support the version
                // of NLST with no command parameters. Respond with a 501 if the server gets an NLST with a parameter.

                if (counter != 0) {
                    counter = 0;
                    server_message = "501 Syntax error in parameters or arguments - Wrong number of argument!\n";
                    sendMessage(connected_socket, server_message);
                } else {
                    counter = 0;
                    if (passive_mode == 1) {

                        if (passive_port > 1024 && passive_port <= 65535 && passive_socket_fd >= 0) {
                            server_message = "150 - Here comes the directory listing.\n";
                            sendMessage(connected_socket, server_message);

                            listen(passive_socket_fd, BACKLOG);
                            data_client = accept(passive_socket_fd, (struct sockaddr *) &data_client_socket,
                                                 &data_client_len);

                            getcwd(cwd, sizeof(cwd));
                            listFiles(data_client, cwd);

                            server_message = "226 - Transfer complete.\n";
                            sendMessage(connected_socket, server_message);

                            close(data_client);
                            close(passive_socket_fd);
                            passive_mode = 0;
                        } else {
                            server_message = "500 - Error entering passive mode.\n";
                            sendMessage(connected_socket, server_message);
                        }
                    } else {
                        server_message = "425 - Use PASV first.\n";
                        sendMessage(connected_socket, server_message);
                    }
                }
            } else {
                // All other command not accepted
                server_message = "502 Command not implemented - This server only supports: USER, QUIT, CWD, CDUP, TYPE, MODE, STRU, RETR, PASV, NLST.\n";
                sendMessage(connected_socket, server_message);
            }
        }
    }

    if (read_buffer_size == -1) {
        perror("recv failed");
    }
}

unsigned long getIPAddress(int connected_socket_fd) {
    unsigned long IP_long;
    struct sockaddr_in connected_socket;
    socklen_t length = sizeof(connected_socket);
    if (getsockname(connected_socket_fd, (struct sockaddr *) &connected_socket, &length) < 0) {
        perror("error!");
        sendMessage(connected_socket_fd, "225 - Data connection failed.\n");
        return -1;
    } else {
        IP_long = connected_socket.sin_addr.s_addr;
        return IP_long;
    }
}

int sendMessage(int socket_fd, const char *format, ...) {
    va_list args;
    char message[1024];
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    return (int) write(socket_fd, message, strlen(message));
}

int sendPath(int socket_fd, char *path, uint32_t offset) {
    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, offset, SEEK_SET);
        int send_d = fileExchange(socket_fd, f);
        if (send_d < 0) {
            return -2;
        }
    } else {
        return -1;
    }
    int to_return = fclose(f);
    if (to_return == 0) {
        return 0;
    } else {
        return -3;
    }
}

int fileExchange(int peer, FILE *f) {
    char file_buffer[1025];
    ssize_t n, ret = 0;
    while ((n = fread(file_buffer, 1, 1024, f)) > 0) {
        ssize_t st = send(peer, file_buffer, n, 0);
        if (st < 0) {
            ret = -1;
            break;
        } else {
            file_buffer[n] = 0;
        }
    }
    return (int) ret;
}
