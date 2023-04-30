#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <condition_variable>
#include <map>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "user.h"
#include "payload.pb.h"
#include <time.h>

#define BUFFER_SIZE 8192
#define DEFAULT_SENDER "Server"
#define HTTP_OK 200
#define HTTP_INTERNAL_ERROR 500
#define MAX_INACTIVE_TIME 30 //sec

using namespace std;

const char* default_ip = "127.0.0.1";
map<string, User *> users = {};

void activity_check() {
    time_t server_time;
    time(&server_time);
    
    map<string, User *>::iterator itr;
    for(itr = users.begin(); itr != users.end(); ++itr) {
        string str_id = itr->first;
        User *u = itr->second;

        if(difftime(server_time, u->last_activity) > MAX_INACTIVE_TIME && u->status != BUSY ) {
            u->set_status("INACTIVO");
            printf("Setting to INACTIVE status user %s expiration time reached\n", u->username.c_str());
        }

        // if(difftime(server_time, u->last_activity) < MAX_INACTIVE_TIME ) {
        //     u->set_status("ACTIVO");
        //     printf("Checked: %s ACTIVE status user\n", u->username.c_str());

        // }
        if(difftime(server_time, u->last_activity) < MAX_INACTIVE_TIME && u->status != BUSY) {
            u->set_status("ACTIVO");
            printf("Updating to ACTIVE status user: %s activity detected\n", u->username.c_str());

        }
    }
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void send_response(int socket, string sender, string message, Payload_PayloadFlag flag, int code, char *buffer)
{
    string binary;
    Payload *response = new Payload();
    response->set_sender(sender);
    response->set_message(message);
    response->set_code(code);
    response->set_flag(flag);

    response->SerializeToString(&binary);

    strcpy(buffer, binary.c_str());
    send(socket, buffer, binary.size() + 1, 0);
    printf("SENDING RESPONSE TO %s...\n", response->sender().c_str());
}

void send_error(int socket, string message)
{
    string binary;
    Payload *error_payload = new Payload();
    error_payload->set_sender("server");
    error_payload->set_message(message);
    error_payload->set_code(500);
    error_payload->SerializeToString(&binary);

    char buffer[binary.size() + 1];
    strcpy(buffer, binary.c_str());
    int sent = send(socket, buffer, sizeof buffer, 0);
    if (sent == 0)
    {
        fprintf(stderr, "ERROR sending response to current client\n");
    }
}

void *handle_client_connected(void *params)
{
    char buffer[BUFFER_SIZE];
    User current_user;
    User *new_user = (User *)params;
    int own_socket = new_user->socket;
    User theUser = *new_user;

    // Received payload
    Payload payload;

    while (1)
    {
        int received = recv(own_socket, buffer, BUFFER_SIZE, 0);
        if (received > 0)
        {
            if (*buffer == '#')
            {
                printf("SERVER - Lost connection with the client %s...\n", current_user.username.c_str());
            }
            else
            {
                // Handle Proto Options
                payload.ParseFromString(buffer);

                // USER REGISTER
                if (payload.flag() == Payload_PayloadFlag_register_)
                {
                    printf("New server registration request with username: %s\n",payload.sender().c_str());

                    if (users.count(payload.sender()) == 0)
                    {
                        string response_message = "User registered successfully";
                        send_response(own_socket, DEFAULT_SENDER, response_message, payload.flag(), HTTP_OK, buffer);
                        printf("User %s entered to chat\n", new_user->username.c_str());

                        // Guardar informacion de nuevo cliente
                        time_t curr_time;
                        current_user.username = payload.sender();
                        current_user.socket = own_socket;
                        current_user.status = ACTIVE;
                        current_user.update_last_activity_time();

                        strcpy(current_user.ip_address, new_user->ip_address);
                        users[current_user.username] = &current_user;
                    }
                    else
                    {
                        send_error(own_socket, "Username has already been taken");
                    }
                }

                // CONNECTED USERS
                else if (payload.flag() == Payload_PayloadFlag_user_list)
                {
                    printf("Listing all users for user: %s\n", current_user.username.c_str());

                    current_user.update_last_activity_time();

                    Payload *response = new Payload();
                    string response_message = "\tUSERNAME\tIP\tESTADO\n";
                    map<string, User *>::iterator itr;

                    for (itr = users.begin(); itr != users.end(); ++itr)
                    {
                        User *u = itr->second;
                        response_message = response_message + u->to_string() + "\n";
                    }
                    send_response(own_socket, DEFAULT_SENDER, response_message, payload.flag(), HTTP_OK, buffer);
                
                    activity_check();

                }

                // USER INFO
                else if (payload.flag() == Payload_PayloadFlag_user_info)
                {

                    if (users.count(payload.extra()) > 0)
                    {
                        current_user.update_last_activity_time();
                        printf("Retrieving info of user '%s', action performed by user: %s\n", payload.extra().c_str(), current_user.username.c_str());
                        string response_message = "\tUSERNAME\tIP\tESTADO\n";
                        User *user_retrieved = users[payload.extra()];

                        response_message = response_message + user_retrieved->to_string() + "\n";
                        send_response(own_socket, DEFAULT_SENDER, response_message, payload.flag(), HTTP_OK, buffer);
                    }
                    else
                    {
                        printf("User does not exists on server");
                        send_error(own_socket, "User does not exists on server");
                    }

                    activity_check();

                }

                // CHANGE USER STATUS
                else if (payload.flag() == Payload_PayloadFlag_update_status)
                {
                    printf("Status change request for user: %s (%s)\n", current_user.username.c_str(), payload.extra().c_str());
                    current_user.update_last_activity_time();
                    current_user.set_status(payload.extra()); 

                    string response_message = "Status updated to " + current_user.get_status();
                    send_response(own_socket, DEFAULT_SENDER, response_message, payload.flag(), HTTP_OK, buffer);
                    
                    activity_check();

                }

                // BROADCAST
                else if (payload.flag() == Payload_PayloadFlag_general_chat)
                {
                    printf("Broadcast message received from user: %s\n", current_user.username.c_str());
                    current_user.update_last_activity_time();
                    string response_message = "Broadcast message was sent successfully";
                    send_response(own_socket, DEFAULT_SENDER, response_message, payload.flag(), HTTP_OK, buffer);

                    // Real broadcast message
                    Payload *broadcast = new Payload();
                    broadcast->set_sender(DEFAULT_SENDER);
                    broadcast->set_message(payload.sender() + " (general): " + payload.message() + "\n");
                    broadcast->set_code(HTTP_OK);

                    string binary;
                    broadcast->SerializeToString(&binary);
                    strcpy(buffer, binary.c_str());

                    map<string, User *>::iterator itr;
                    for (itr = users.begin(); itr != users.end(); ++itr)
                    {
                        User *u = itr->second;
                        // We manage the current user broadcast because we already know the message...
                        if (u->username != new_user->username) {
                            send(u->socket, buffer, binary.size() + 1, 0);
                        }
                    }
                    activity_check();

                }

                // PRIVATE MESSAGE
                else if (payload.flag() == Payload_PayloadFlag_private_chat)
                {
                    current_user.update_last_activity_time();

                    if (users.count(payload.extra()) > 0)
                    {
                        printf("Private message received from user: %s, to: %s\n", current_user.username.c_str(), payload.extra().c_str());
                        string response_message = "Private message was sent successfully to: " + payload.extra();
                        send_response(own_socket, DEFAULT_SENDER, response_message, payload.flag(), HTTP_OK, buffer);

                        User *recipient = users[payload.extra()];
                        // Real private message
                        Payload *private_msg = new Payload();
                        private_msg->set_sender(current_user.username);
                        private_msg->set_message(payload.sender() + " (private): " + payload.message() + "\n");
                        private_msg->set_code(HTTP_OK);
                        private_msg->set_flag(payload.flag());

                        string binary;
                        private_msg->SerializeToString(&binary);
                        strcpy(buffer, binary.c_str());
                        send(recipient->socket, buffer, binary.size() + 1, 0);
                    }
                    else
                    {
                        printf("User for PM does not exists on server");
                        send_error(own_socket, "User for PM does not exists on server");
                    }
                    activity_check();

                }

                // OPTION ERROR
                else
                {
                    send_error(own_socket, "Server couldn't match selected option...");
                }
            }
        }

    }

    // Disconnnect user from socket
    printf("Disconnecting user %s from server...", new_user->username.c_str());
    users.erase(new_user->username);
    close(own_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }


    int port = atoi(argv[1]);

    sockaddr_in server_addr;
    sockaddr_in client_addr;
    socklen_t client_size;
    int socket_fd = 0, conn_fd = 0;

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(default_ip);
    server_addr.sin_port = htons(port);
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0)
    {
        fprintf(stderr, "Error creating socket...\n");
        return EXIT_FAILURE;
    }

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "Error on binding IP...\n");
        return EXIT_FAILURE;
    }

    if (listen(socket_fd, 5) < 0)
    {
        fprintf(stderr, "Socket listening failed...\n");
        return EXIT_FAILURE;
    }

    printf("SERVER RUNNING ON PORT %d\n", port);

    while (1)
    {
        client_size = sizeof client_addr;
        conn_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &client_size);

        // TODO: Manage a limited number of clients connected...
        User new_user;
        new_user.socket = conn_fd;
        inet_ntop(AF_INET, &(client_addr.sin_addr), new_user.ip_address, INET_ADDRSTRLEN);

        // Threads
        pthread_t thread_id;
        pthread_attr_t attrs;

        pthread_attr_init(&attrs);
        pthread_create(&thread_id, &attrs, handle_client_connected, (void *)&new_user);

        // Reduce CPU usage
        sleep(1);
    }

    close(socket_fd);
    return EXIT_SUCCESS;
}
