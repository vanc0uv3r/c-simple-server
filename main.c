#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <arpa/inet.h>

#define CLIENT_BUFFER_SIZE 8
#define READ_BUFFER_SIZE 8

int counter = 0;

const char invalid_buf[] = "Invalid command! Try help to see command list\n";
const char inc_buf[] = "The counter has incremented. Now it's = ";
const char dec_buf[] = "The counter has decremented. Now it's = ";
const char help_buf[] = "inc – increments counter\ndec – decrements counter\n"
                  "help – show command list\n";
const char too_many[] = "Cannot connect to server! Too many players already\n";
const char no_enough[] = "Sorry, but we are waiting for all players\n";
const char new_client[] = "New client has connected\n";
const char start_game[] = "Alright. Everyone is here. Now we can start the game."
                          " Let's Go!\n";

typedef struct player
{
    int fd;
    int buffer_size;
    int empty;
    char *buffer;
} player;

int str_to_int(char *s)
{
    int i, num = 0, minus = 1;
    for (i = 0; i < strlen(s); ++i)
    {
        if (i == 0 && s[i] == '-')
            minus = -1;
        else if ((s[i] > '9' || s[i] < '0'))
            return -1;
        num = num * 10 + s[i] - '0';
    }
    return num * minus;
}

void close_connection(int fd)
{
    shutdown(fd, 2);
    close(fd);
}

void set_fd_readfds(player *clients, int players, fd_set *readfds, int *max_d)
{
    int i;
    for (i = 0; i < players; ++i)
    {
        if (clients[i].fd != -1)
        {
            FD_SET(clients[i].fd, readfds);
            if (clients[i].fd > *max_d)
                *max_d = clients[i].fd;
        }
    }
}

int return_with_error(char *buffer)
{
    perror(buffer);
    exit(1);
}

int check_argv(int port, int max_players)
{
    if (max_players == -1 || port == -1)
    {
        printf("Players number or port is not integer!\n");
        return 0;
    }
    return 1;
}

int check_argc(int argc)
{
    if (argc != 3)
    {
        printf("Invalid number of arguments!\n"
               "Usage: ./prog players_number port_to_bind\n");
        return 0;
    }
    return 1;
}

int deploy_server_socket(int port, int max_players)
{
    int sock;
    struct sockaddr_in server_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return_with_error("Socket problem");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        return_with_error("Binding problem");
    if (listen(sock, max_players) == -1)
        return_with_error("Listen socket problem");
    return sock;
}

player *init_clients(int players)
{
    int i;
    player *tmp = malloc(sizeof(*tmp) * players);
    for (i = 0; i < players; ++i)
    {
        tmp[i].fd = -1;
        tmp[i].buffer = calloc(CLIENT_BUFFER_SIZE, sizeof(char));
        tmp[i].empty = CLIENT_BUFFER_SIZE;
        tmp[i].buffer_size = CLIENT_BUFFER_SIZE;
    }
    return tmp;
}

void add_new_client(player *clients, int fd, int max_players)
{
    int i;
    for (i = 0; i < max_players; ++i)
    {
        if (clients[i].fd == -1)
        {
            clients[i].fd = fd;
            break;
        }
    }
}

int need_realloc(int empty, int buff_size)
{
    return buff_size > empty;
}

void add_client_data(player *client, char *buff, int buff_size)
{
    if (need_realloc(client->empty, buff_size))
    {
        client->empty += client->buffer_size;
        client->buffer_size *= 2;
        char *tmp = malloc(client->buffer_size * sizeof(char));
        strncpy(tmp, client->buffer, strlen(client->buffer) + 1);
        free(client->buffer);
        client->buffer = tmp;
    }
    strncpy(client->buffer + strlen(client->buffer), buff, buff_size);
    client->empty -= buff_size;
    printf("empty = %d\n", client->empty);
}

void delete_client(player *client)
{
    client->fd = -1;
    client->empty = CLIENT_BUFFER_SIZE;
    client->buffer_size = CLIENT_BUFFER_SIZE;
    free(client->buffer);
}

int get_buff_enter(char *buff)
{
    int i;
    for (i = 0; i < strlen(buff); ++i)
    {
        if (buff[i] == '\n')
            return i;
    }
    return -1;
}

char *cut_command(player *client, int to)
{
    int i;
    char *tmp = malloc(to * sizeof(*tmp));
    strncpy(tmp, client->buffer, to);
    tmp[to - 1] = '\0';
    for (i = 0; i < strlen(client->buffer); ++i)
    {
        if (i + to + 1 > strlen(client->buffer) - 1)
            client->buffer[i] = '\0';
        else
            client->buffer[i] = client->buffer[i + to + 1];
    }
    return tmp;
}

void reset_client_buffer(player *client)
{
    if (client->buffer_size != CLIENT_BUFFER_SIZE)
    {
        free(client->buffer);
        client->buffer_size = CLIENT_BUFFER_SIZE;
        client->buffer = calloc(CLIENT_BUFFER_SIZE, sizeof(char));
    }
    client->empty = CLIENT_BUFFER_SIZE;
}

void send_all_clients(player *clients, int max_players, const char *buff, int b_size)
{
    int i;
    for (i = 0; i < max_players; ++i) {
        if (clients[i].fd != -1)
            write(clients[i].fd, buff, b_size);
    }
}

void execute_command(char *command, player *clients, int max_players, int sender)
{
    char *templ_buf;
    int templ_size;
    if (strcmp(command, "inc") == 0)
    {
        counter++;
        templ_buf = malloc(sizeof(char) * (14 + sizeof(inc_buf)));
        templ_size = snprintf(templ_buf, 14 + sizeof(inc_buf), "%s%d\n",
                              inc_buf, counter);
        send_all_clients(clients, max_players, templ_buf, templ_size);
        free(templ_buf);
    }
    else if (strcmp(command, "dec") == 0)
    {
        counter--;
        templ_buf = malloc(sizeof(char) * (14 + sizeof(dec_buf)));
        templ_size = snprintf(templ_buf, 14 + sizeof(dec_buf), "%s%d\n",
                              dec_buf, counter);
        send_all_clients(clients, max_players, templ_buf, templ_size);
        free(templ_buf);
    }
    else if (strcmp(command, "help") == 0)
        write(clients[sender].fd, help_buf, sizeof(help_buf));
    else
        write(clients[sender].fd, invalid_buf, sizeof(invalid_buf));
}

int handle_client(player *client)
{
    int rc;
    char buff[READ_BUFFER_SIZE];
    rc = read(client->fd, buff, sizeof(buff) - 1);
    if (rc == -1)
    {
        perror("Problems with read");
        exit(1);
    }
    if (rc == 0)
    {
        printf("Client has been disconnected\n");
        close_connection(client->fd);
        delete_client(client);
        return 0;
    }
    buff[rc] = '\0';
    add_client_data(client, buff, rc + 1);
    return 1;
}

void find_command(player *clients, int max_players, int now_players, int sender)
{
    int n_pos = get_buff_enter(clients[sender].buffer);
    char *cmd;
    while (n_pos != -1)
    {
        cmd = cut_command(&clients[sender], n_pos);
        if (max_players != now_players)
            write(clients[sender].fd, no_enough, sizeof(no_enough));
        else
        {
            execute_command(cmd, clients, max_players, sender);
            free(cmd);
        }
        n_pos = get_buff_enter(clients[sender].buffer);
    }
    if (strlen(clients[sender].buffer) == 0)
        reset_client_buffer(&clients[sender]);
}

int main(int argc, char *argv[])
{
    int sock;
    int port;
    int max_players;
    int max_d;
    int fd;
    int i;
    int now_players = 0;
    unsigned int addrlen;
    struct sockaddr_in client_addr;
    fd_set readfds;
    player *clients = NULL;
    if (!check_argc(argc))
        return 1;
    max_players = str_to_int(argv[1]);
    port = str_to_int(argv[2]);
    if (!check_argv(port, max_players))
        return 1;
    sock = deploy_server_socket(port, max_players);
    printf("Starting server...\n");
    clients = init_clients(max_players);
    while (1)
    {
        max_d = sock;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        set_fd_readfds(clients, max_players, &readfds, &max_d);
        int res = select(max_d + 1, &readfds, NULL, NULL, NULL);
        if (res < 1)
        {
            perror("Select problem ");
            return 1;
        }
        if (FD_ISSET(sock, &readfds))
        {
            addrlen = sizeof(client_addr);
            fd = accept(sock, (struct sockaddr *)&client_addr, &addrlen);
            if (now_players == max_players)
            {
                write(fd, too_many, sizeof(too_many));
                close_connection(fd);
            }
            else
            {
                now_players++;
                printf("New connect from ip %s %d\n",
                       inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
                send_all_clients(clients, max_players, new_client, sizeof(new_client));
                add_new_client(clients, fd, max_players);
                if (now_players == max_players)
                    send_all_clients(clients, max_players, start_game, sizeof(start_game));
            }
        }
        for (i = 0; i < max_players; i++)
        {
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &readfds))
            {
                if (handle_client(&clients[i]) == 0)
                    now_players--;
                else
                    find_command(clients, max_players, now_players, i);
            }
        }
    }
    return 0;
}
