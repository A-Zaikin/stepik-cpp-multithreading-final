#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>

#define READ_BUFFER_SIZE 32768
#define WRITE_BUFFER_SIZE 32768

char *ip;
char *port;
char *directory;
int socket_fd;

void read_args(int argc, char **argv);
void handle_error(char *error);
void start_server();
void *respond_routine(void *args);
void respond(int accepted_socket_fd);
char *read_request(int accepted_socket_fd);
void send_response(int accepted_socket_fd, char *requested_resource);

/*
./final -h 127.0.0.1 -p 12345 -d /tmp
*/

// compile with -pthread
int main(int argc, char **argv) {
    read_args(argc, argv);
    // ip = "127.0.0.1";
    // port = "12345";
    // directory = "/tmp";

    daemon(1, 0);
    start_server();

    printf("Waiting for requests...\n");

    while(1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int accepted_socket_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (accepted_socket_fd == -1) {
            handle_error("accept");
        }

        pthread_t thread_id;
        int result = pthread_create(&thread_id, NULL, respond_routine, &accepted_socket_fd);
        if (result != 0) {
            printf("pthread_create error #%d\n", result);
            exit(EXIT_FAILURE);
        }
    }

    if (close(socket_fd) == -1) {
        handle_error("close");
    }

    return EXIT_SUCCESS;
}

void start_server()
{
    struct addrinfo addr_hints, *first_addr, *possible_addr;

    // get list of available addresses
    memset(&addr_hints, 0, sizeof(addr_hints));
    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_flags = AI_PASSIVE;
    int gai_result = getaddrinfo(NULL, port, &addr_hints, &first_addr);
    if (gai_result != 0) {
        printf("getaddrinfo: %s\n", gai_strerror(gai_result));
        exit(EXIT_FAILURE);
    }

    // find a socket to which we can bind
    for (possible_addr = first_addr; possible_addr != NULL; possible_addr = possible_addr->ai_next)
    {
        socket_fd = socket(possible_addr->ai_family, possible_addr->ai_socktype, 0);
        if (socket_fd == -1) {
            continue;
        }
        if (bind(socket_fd, possible_addr->ai_addr, possible_addr->ai_addrlen) == 0) {
            break;
        }
        close(socket_fd);
    }

    freeaddrinfo(first_addr);

    if (possible_addr == NULL) {
        printf("No possible addresses.\n");
        pause();
        exit(EXIT_FAILURE);
    }

    // listen for incoming connections
    if (listen(socket_fd, 1024) == -1) {
        handle_error("listen");
    }
}

void *respond_routine(void *args) {
    respond(*((int *)args));
    return NULL;
}

void respond(int accepted_socket_fd) {
    char *uri = read_request(accepted_socket_fd);
    send_response(accepted_socket_fd, uri);
    close(accepted_socket_fd);
    free(uri);
}

char *read_request(int accepted_socket_fd) {
    int num_recieved, fd, bytes_read;
    char *ptr;
    char *read_buffer = malloc(READ_BUFFER_SIZE);

    num_recieved = recv(accepted_socket_fd, read_buffer, READ_BUFFER_SIZE, 0);

    if (num_recieved == -1) {
        handle_error("recv");
    } else if (num_recieved == 0) {
        printf("Client disconnected unexpectedly.\n");
        pthread_exit(NULL);
    }

    //printf("%s\n", read_buffer);

    char *method = strtok(read_buffer, " ");
    if (strcmp(method, "GET") != 0) {
        printf("\"%s\" method is not supported.\n", method);
        exit(EXIT_FAILURE);
    }

    char *uri = strtok(NULL, " ?");
    uri = strdup(uri);
    printf("Successfully recieved GET: \"%s\"\n", uri);

    free(read_buffer);
    return uri;
}

void send_response(int accepted_socket_fd, char *resource_uri) {
    char *http_not_found = "HTTP/1.0 404 Not Found\r\n\r\n";
    char *http_ok = "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n\r\n";
    char full_path[strlen(directory) + strlen(resource_uri) + 1];
    strcpy(full_path, directory);
    strcat(full_path, resource_uri);
    FILE *resource_file = fopen(full_path, "r");
    if (resource_file == NULL) {
        write(accepted_socket_fd, http_not_found, strlen(http_not_found) + 1);
        return;
    }
    char *resource_buffer = malloc(READ_BUFFER_SIZE);
    int resource_size = fread(resource_buffer, sizeof(char), READ_BUFFER_SIZE, resource_file);
    fclose(resource_file);

    resource_buffer[resource_size] = '\0';

    char *response_buffer = malloc(WRITE_BUFFER_SIZE);
    sprintf(response_buffer, http_ok, resource_size);
    strcat(response_buffer, resource_buffer);
    write(accepted_socket_fd, response_buffer, strlen(response_buffer) + 1);

    free(resource_buffer);
    free(response_buffer);
}

void read_args(int argc, char **argv) {
    int arg_char;
    while ((arg_char = getopt(argc, argv, "h:p:d:")) != -1) {
        switch (arg_char) {
            case 'h':
                ip = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'd':
                directory = optarg;
                break;
            default:
                abort();
        }
    }
    //printf("ip = %s, port = %s, directory = %s\n", ip, port, directory);
}

void handle_error(char *error) {
    perror(error);
    exit(EXIT_FAILURE);
}