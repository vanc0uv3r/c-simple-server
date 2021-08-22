#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <arpa/inet.h>

int counter = 0;

const char invalid_buf[] = "Invalid command! Try help to see command list\n";
const char inc_buf[] = "The counter has incremented. Now it's = ";
const char dec_buf[] = "The counter has decremented. Now it's = ";
const char help_buf[] = "inc – increments counter\ndec – decrements counter\n"
                  "help – show command list\n";
const char too_many_msg[] = "Cannot connect to server! Too many players already"
                            "\n";
const char no_enough_msg[] = "Sorry, but we are waiting for all players\n";
const char new_client_msg[] = "New client has connected\n";
const char game_on_msg[] = "Sorry, you cannot enter. Game is on\n";
const char invalid_port_msg[] = "Invalid port or user number! Max user number "
                                "is 1000 and port can be more then 1024 and les"
                                "s then 65000\n";
const char invalid_argc_msg[] = "Invalid number of arguments!\n"
                            "Usage: max_players port\n";
const char start_game_msg[] = "Alright. Everyone is here. Now we can start the "
                              "game! Let's Go!\n";

enum sizes{
    client_buffer_size = 1024,
    read_buffer_size = 1024,
    int_bites = 13
};

typedef struct player
{
    int fd;
    int buffer_size;
    int empty;
    char *buffer;
} player;

typedef struct server
{
    int max_players;
    int now_players;
    int start;
    int reached_max;
} server;

int str_to_int(char *s)
{
    int i, num = 0, minus = 1;
    if (strlen(s) > 12)
        return -1;
    for (i = 0; i < strlen(s); ++i)
    {
        if (i == 0 && s[i] == '-')
            minus = -1;
        else
        {
            if ((s[i] > '9' || s[i] < '0'))
                return -1;
            num = num * 10 + s[i] - '0';
        }
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

void exit_with_perror(char *buffer)
{
    perror(buffer);
    exit(1);
}

void exit_with_print(const char *buffer)
{
    printf("%s\n", buffer);
    exit(1);
}

int check_argv(int port, int max_players)
{
    return (max_players == -1 || port == -1) || (max_players > 1000
    || port > 65000 || max_players < 1 || port < 1024);
}

int check_argc(int argc)
{
    if (argc != 3)
    {
        printf("Invalid number of arguments!\n"
               "Usage: ./prog <players_number> <port_to_bind>\n");
        return 0;
    }
    return 1;
}

int deploy_server_socket(int port, int max_players)
{
    int sock, opt = 1;
    struct sockaddr_in server_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        exit_with_perror("Socket problem");
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        exit_with_perror("Binding problem");
    if (listen(sock, max_players) == -1)
        exit_with_perror("Listen socket problem");
    return sock;
}

player *init_clients(int players)
{
    int i;
    player *tmp = malloc(sizeof(*tmp) * players);
    for (i = 0; i < players; ++i)
    {
        tmp[i].fd = -1;
        tmp[i].buffer = calloc(client_buffer_size, sizeof(char));
        tmp[i].empty = client_buffer_size;
        tmp[i].buffer_size = client_buffer_size;
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
            if (clients[i].buffer == NULL)
                clients[i].buffer = calloc(client_buffer_size, sizeof(char));
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
//    printf("empty = %d\n", client->empty);
}

void delete_client(player *client)
{
    client->fd = -1;
    client->empty = client_buffer_size;
    client->buffer_size = client_buffer_size;
    if (client->buffer != NULL)
        free(client->buffer);
    client->buffer = NULL;
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
    char *tmp = malloc((to + 1) * sizeof(*tmp));
    strncpy(tmp, client->buffer, to + 1);
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
    if (client->buffer_size != client_buffer_size)
    {
        free(client->buffer);
        client->buffer_size = client_buffer_size;
        client->buffer = calloc(client_buffer_size, sizeof(char));
    }
    client->empty = client_buffer_size;
}

void send_all_clients(player *clients, int max_p, const char *buff, int b_size)
{
    int i;
    for (i = 0; i < max_p; ++i) {
        if (clients[i].fd != -1)
            write(clients[i].fd, buff, b_size);
    }
}

void execute_command(char *cmd, player *clients, int max_players, int sender)
{
    char *templ_buf;
    int templ_size;
    if (strcmp(cmd, "inc") == 0)
    {
        counter++;
        templ_buf = malloc(sizeof(char) * (14 + sizeof(inc_buf)));
        templ_size = snprintf(templ_buf, 14 + sizeof(inc_buf), "%s%d\n",
                              inc_buf, counter);
        send_all_clients(clients, max_players, templ_buf, templ_size);
        free(templ_buf);
    }
    else if (strcmp(cmd, "dec") == 0)
    {
        counter--;
        templ_buf = malloc(sizeof(char) * (int_bites + sizeof(dec_buf)));
        templ_size = snprintf(templ_buf, int_bites + sizeof(dec_buf), "%s%d\n",
                              dec_buf, counter);
        send_all_clients(clients, max_players, templ_buf, templ_size);
        free(templ_buf);
    }
    else if (strcmp(cmd, "help") == 0)
        write(clients[sender].fd, help_buf, sizeof(help_buf));
    else if (strcmp(cmd, "\n") != 0)
        write(clients[sender].fd, invalid_buf, sizeof(invalid_buf));
}

int read_client_data(player *client)
{
    int rc;
    char buff[read_buffer_size];
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

int can_play(int now_players, int start)
{
    return now_players > 0 || start == 0;
}

char *trim(char *str)
{
    char *end;
    while(*str == ' ') str++
    ;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while(end > str && *end == ' ') end--
    ;
    end[1] = '\0';
    return str;
}

void find_command(player *clients, int max_players, int sender, int start)
{
    int n_pos = get_buff_enter(clients[sender].buffer);
    char *cmd;
    while (n_pos != -1)
    {
        cmd = cut_command(&clients[sender], n_pos);
        if (!start)
            write(clients[sender].fd, no_enough_msg, sizeof(no_enough_msg));
        else
            execute_command(trim(cmd), clients, max_players, sender);
        free(cmd);
        n_pos = get_buff_enter(clients[sender].buffer);
    }
    if (strlen(clients[sender].buffer) == 0)
        reset_client_buffer(&clients[sender]);
}

void handle_client(player *clients, server *serv, fd_set *readfds)
{
    int i;
    for (i = 0; i < serv->max_players; i++)
    {
        if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, readfds))
        {
            if (read_client_data(&clients[i]) == 0)
                serv->now_players--;
            else
                find_command(clients, serv->max_players, i, serv->start);
        }
    }
}

void start_game(player *clients, server *serv)
{
    send_all_clients(clients, serv->max_players, start_game_msg,
                     sizeof(start_game_msg));
    serv->reached_max = 1;
    serv->start = 1;
}

void start_server(int sock, server *serv, player *clients)
{
    int res, max_d, fd;
    unsigned int addrlen;
    struct sockaddr_in client_addr;
    fd_set readfds;
    while (can_play(serv->now_players, serv->start))
    {
        max_d = sock;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        set_fd_readfds(clients, serv->max_players, &readfds, &max_d);
        res = select(max_d + 1, &readfds, NULL, NULL, NULL);
        if (res < 1)
            exit_with_perror("Select problem ");
        if (FD_ISSET(sock, &readfds))
        {
            addrlen = sizeof(client_addr);
            fd = accept(sock, (struct sockaddr *)&client_addr, &addrlen);
            if (serv->now_players == serv->max_players)
            {
                write(fd, too_many_msg, sizeof(too_many_msg));
                close_connection(fd);
            }
            else if (!serv->reached_max)
            {
                serv->now_players++;
                printf("New connect from ip %s %d\n",
                       inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
                send_all_clients(clients, serv->max_players, new_client_msg,
                                 sizeof(new_client_msg));
                add_new_client(clients, fd, serv->max_players);
                if (serv->now_players == serv->max_players)
                    start_game(clients, serv);
            }
            else if (serv->start)
            {
                write(fd, game_on_msg, sizeof(game_on_msg));
                close_connection(fd);
            }
        }
        handle_client(clients, serv, &readfds);
    }
}

server *init_server(int max_players)
{
    server *tmp = malloc(sizeof(*tmp));
    tmp->start = 0;
    tmp->max_players = max_players;
    tmp->now_players = 0;
    tmp->reached_max = 0;
    return tmp;
}

int main(int argc, char *argv[])
{
    int sock, port, max_players;
    player *clients;
    server *serv;
    if (!check_argc(argc))
        exit_with_print(invalid_argc_msg);
    max_players = str_to_int(argv[1]);
    port = str_to_int(argv[2]);
    if (check_argv(port, max_players))
        exit_with_print(invalid_port_msg);
    sock = deploy_server_socket(port, max_players);
    serv = init_server(max_players);
    printf("Starting server...\n");
    clients = init_clients(max_players);
    start_server(sock, serv, clients);
    free(clients);
    free(serv);
    close(sock);
    printf("No more clients. Exiting...\n");
    return 0;
}
