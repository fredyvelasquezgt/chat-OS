#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include "new.pb-c.h"
#include <time.h>
#define MAX_MSG_SIZE 1024

/* -------------------------------------------------------------------------- */
/* Chat realizado en C con multithreading y uso de protobuf                   */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* Para correr el server realizar los siguientes comandos                     */
/* Primero instalar protobuf-c https://github.com/protobuf-c/                 */
/* sudo make Makefile compile								                  */
/* ./server3 port							                                  */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* Para correr el client realizar los siguientes comandos                     */
/* Primero instalar protobuf-c https://github.com/protobuf-c/                 */
/* sudo make Makefile compile								                  */
/* ./client3 serverIP port					                                  */
/* -------------------------------------------------------------------------- */
// With help of https://github.com/nikhilroxtomar/Chatroom-in-C

//Max Clients in Chat
#define MAX_CLIENTS 100
#define BUFFER_SZ 2048 * 24

//Client Count
static _Atomic unsigned int clients_count = 0;
static int client_id = 10;

//Trigger inactivity timer
int trigger_inactivity_timer = 10000; /* 10ms */

volatile sig_atomic_t flag = 0;
//Set flag exit and close everyone
void catch_ctrl_c_and_exit(int sig)
{
	flag = 1;
}

//Client Structure
typedef struct
{
	struct sockaddr_in address;
	int sockfd;
	int client_id;
	clock_t last_connection;
	int status_changed_last_connection;
	char status[32];
	char name[32];
} client_str;

//List of clients in server
client_str *clients[MAX_CLIENTS];

//Mutex to handle client interactions
pthread_mutex_t clients_chat_mutex = PTHREAD_MUTEX_INITIALIZER;

//Utils functions
void str_overwrite_stdout()
{
	printf("\r%s", "> ");
	fflush(stdout);
}

void str_trim_lf(char *arr, int length)
{
	int i;
	for (i = 0; i < length; i++)
	{ // trim \n
		if (arr[i] == '\n')
		{
			arr[i] = ' ';
		}
	}
}

//Add Client to server
void add_client_to_server(client_str *cl)
{
	strcpy(cl->status, "activo");
	cl->last_connection = clock();
	cl->status_changed_last_connection = 0;
	pthread_mutex_lock(&clients_chat_mutex);
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (!clients[i])
		{
			clients[i] = cl;

			break;
		}
	}

	pthread_mutex_unlock(&clients_chat_mutex);
}

//Remove Client from Server
void remove_client_from_server(int client_id)
{
	pthread_mutex_lock(&clients_chat_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (clients[i]->client_id == client_id)
			{
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_chat_mutex);
}

//Broadcast message to all clients except for sender
void broadcast_message(char *msg_string, client_str *client_sender)
{
	pthread_mutex_lock(&clients_chat_mutex);
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (clients[i]->client_id != client_sender->client_id)
			{

				//Send message using protobuf
				Chat__ServerResponse srv_res = CHAT__SERVER_RESPONSE__INIT;
				void *buf; // Buffer to store serialized data
				unsigned len;
				srv_res.option = 4;
				Chat__MessageCommunication msg = CHAT__MESSAGE_COMMUNICATION__INIT;
				msg.message = msg_string;
				msg.recipient = "everyone";
				msg.sender = client_sender->name;
				srv_res.messagecommunication = &msg;
				len = chat__server_response__get_packed_size(&srv_res);
				buf = malloc(len);
				chat__server_response__pack(&srv_res, buf);
				if (send(clients[i]->sockfd, buf, len, 0) < 0)
				{
					send_server_failure_response("Error enviando broadcast message.\n", client_sender, 0);
					break;
				}
				free(buf);
			}
		}
	}
	pthread_mutex_unlock(&clients_chat_mutex);
	// send_success_server_response("Message sent succesfully", client_sender,0);
}

//Send success server response with option
void send_success_server_response(char *succces_message, client_str *client_sender, int option)
{
	pthread_mutex_lock(&clients_chat_mutex);

	//Send message using protobuf
	Chat__ServerResponse srv_res = CHAT__SERVER_RESPONSE__INIT;
	void *buf;	  // Buffer to store serialized data
	unsigned len; // Length of serialized data

	srv_res.code = 200;
	srv_res.servermessage = succces_message;
	if (option != 0)
	{
		srv_res.option = option;
	}
	len = chat__server_response__get_packed_size(&srv_res);
	buf = malloc(len);
	chat__server_response__pack(&srv_res, buf);
	send(client_sender->sockfd, buf, len, 0);
	pthread_mutex_unlock(&clients_chat_mutex);
}

//Send failure server response with option
void send_server_failure_response(char *failure_message, client_str *client_sender, int option)
{
	pthread_mutex_lock(&clients_chat_mutex);

	//Send message using protobuf
	Chat__ServerResponse srv_res = CHAT__SERVER_RESPONSE__INIT;
	void *buf;	  // Buffer to store serialized data
	unsigned len; // Length of serialized data

	srv_res.code = 500;
	srv_res.servermessage = failure_message;
	if (option != 0)
	{
		srv_res.option = option;
	}
	len = chat__server_response__get_packed_size(&srv_res);
	buf = malloc(len);
	chat__server_response__pack(&srv_res, buf);
	send(client_sender->sockfd, buf, len, 0);
	pthread_mutex_unlock(&clients_chat_mutex);
}

// Get Users List with its information
void get_users_list(client_str *client)
{

	pthread_mutex_lock(&clients_chat_mutex);
	printf("Enviando lista de usuarios a %s\n", client->name);
	//Send message using protobuf
	Chat__ServerResponse srv_res = CHAT__SERVER_RESPONSE__INIT;
	Chat__ConnectedUsersResponse users = CHAT__CONNECTED_USERS_RESPONSE__INIT;
	Chat__UserInfo **connectedClients;
	int j = 0;
	connectedClients = malloc(sizeof(Chat__UserInfo *) * (clients_count));
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{

			connectedClients[j] = malloc(sizeof(Chat__UserInfo));
			chat__user_info__init(connectedClients[j]);
			char ip[BUFFER_SZ];
			sprintf(ip, "%d.%d.%d.%d",
					clients[i]->address.sin_addr.s_addr & 0xff,
					(clients[i]->address.sin_addr.s_addr & 0xff00) >> 8,
					(clients[i]->address.sin_addr.s_addr & 0xff0000) >> 16,
					(clients[i]->address.sin_addr.s_addr & 0xff000000) >> 24);
			connectedClients[j]->ip = ip;
			connectedClients[j]->status = clients[i]->status;
			connectedClients[j]->username = clients[i]->name;
			j = j + 1;
		}
	}

	void *buf; // Buffer to store serialized data
	unsigned len;
	srv_res.option = 2;
	users.n_connectedusers = clients_count;
	users.connectedusers = connectedClients;
	srv_res.connectedusers = &users;
	srv_res.code = 200;
	len = chat__server_response__get_packed_size(&srv_res);
	buf = malloc(len);
	chat__server_response__pack(&srv_res, buf);
	send(client->sockfd, buf, len, 0);
	pthread_mutex_unlock(&clients_chat_mutex);
	free(buf);
}

// Get information from a user
void get_user_information_request(client_str *client, char *username)
{

	pthread_mutex_lock(&clients_chat_mutex);
	if (strcmp("everyone", username) == 0)
	{
		pthread_mutex_unlock(&clients_chat_mutex);
		send_server_failure_response('Para consultar todos los usuarios debes hacerlo en la opción correcta.\n', client, 5);
		return;
	}
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (strcmp(clients[i]->name, username) == 0)
			{

				//Send message using protobuf
				Chat__ServerResponse srv_res = CHAT__SERVER_RESPONSE__INIT;
				void *buf; // Buffer to store serialized data
				unsigned len;
				srv_res.option = 5;

				Chat__UserInfo user_info = CHAT__USER_INFO__INIT;
				char ip[BUFFER_SZ];
				sprintf(ip, "%d.%d.%d.%d",
						clients[i]->address.sin_addr.s_addr & 0xff,
						(clients[i]->address.sin_addr.s_addr & 0xff00) >> 8,
						(clients[i]->address.sin_addr.s_addr & 0xff0000) >> 16,
						(clients[i]->address.sin_addr.s_addr & 0xff000000) >> 24);

				// Set user info
				user_info.status = clients[i]->status;
				user_info.username = clients[i]->name;
				user_info.ip = ip;

				srv_res.userinforesponse = &user_info;
				srv_res.code = 200;
				len = chat__server_response__get_packed_size(&srv_res);
				buf = malloc(len);
				chat__server_response__pack(&srv_res, buf);
				send(client->sockfd, buf, len, 0);
				printf("Enviando request de usuarios a %s de %s\n", client->name, username);
				pthread_mutex_unlock(&clients_chat_mutex);
				free(buf);
				return;
			}
		}
	}
	pthread_mutex_unlock(&clients_chat_mutex);

	send_server_failure_response("No existe ningun usuario con ese nombre conectado al chat.", client, 5);
}

//Change user status
void change_user_status(client_str *client, char *status, char *username, int inactive)
{

	pthread_mutex_lock(&clients_chat_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (strcmp(clients[i]->name, username) == 0)
			{

				strcpy(clients[i]->status, status);

				pthread_mutex_unlock(&clients_chat_mutex);
				if (inactive == 0)
				{
					send_success_server_response("Status cambiado éxitosamente.\n", client, 3);
				}
				else
				{
					send_success_server_response("Tu status ha cambiado a inactivo por tu inactividad.\n", client, 3);
				}

				pthread_mutex_lock(&clients_chat_mutex);

				//send message to everyone that someone changed status
				char buff_out2[BUFFER_SZ];
				sprintf(buff_out2, "%s ha cambiado su status a  %s\n", username, status);
				printf("Chat General %s ha cambiado su status a %s\n", username, status);

				pthread_mutex_unlock(&clients_chat_mutex);
				broadcast_message(buff_out2, client);
				pthread_mutex_lock(&clients_chat_mutex);
				pthread_mutex_unlock(&clients_chat_mutex);
				return;
			}
		}
	}

	pthread_mutex_unlock(&clients_chat_mutex);
	send_server_failure_response("Error. Intenando cambiar el status de un usuario que no existe", client, 3);
}

//Send private message
void send_private_message(char *msg_string, client_str *client_sender, char *receiver_name)
{
	pthread_mutex_lock(&clients_chat_mutex);
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (strcmp(clients[i]->name, receiver_name) == 0)
			{
				//Send message using protobuf
				Chat__ServerResponse srv_res = CHAT__SERVER_RESPONSE__INIT;
				void *buf; // Buffer to store serialized data
				unsigned len;
				srv_res.option = 4;
				Chat__MessageCommunication msg = CHAT__MESSAGE_COMMUNICATION__INIT;
				msg.message = msg_string;
				msg.recipient = receiver_name;
				msg.sender = client_sender->name;
				srv_res.messagecommunication = &msg;
				len = chat__server_response__get_packed_size(&srv_res);
				buf = malloc(len);
				chat__server_response__pack(&srv_res, buf);

				if (send(clients[i]->sockfd, buf, len, 0) < 0)
				{
					pthread_mutex_unlock(&clients_chat_mutex);
					send_server_failure_response("Error enviando broadcast message. Usuario ya no se encuentra conectado", client_sender, 0);
					pthread_mutex_unlock(&clients_chat_mutex);
					return;
				}
				else
				{
					printf("Chat Privado %s hacia %s -> %s\n", client_sender->name, receiver_name, msg_string);
					pthread_mutex_unlock(&clients_chat_mutex);
					return;
				}

				free(buf);
			}
		}
	}
	pthread_mutex_unlock(&clients_chat_mutex);
	send_server_failure_response("Error. Intentando enviar un mensaje a un usuario que no existe.", client_sender, 0);
}

//Check if name is available in clientes
int check_is_name_available_in_clients(char *name, int client_id)
{
	pthread_mutex_lock(&clients_chat_mutex);
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (clients[i]->client_id != client_id)
			{
				if (strcmp(clients[i]->name, name) == 0)
				{
					pthread_mutex_unlock(&clients_chat_mutex);
					return 0;
				}
			}
		}
	}
	pthread_mutex_unlock(&clients_chat_mutex);
	return 1;
}

//Close clients when server is closed
void close_server()
{
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			close(clients[i]->sockfd);
		}
	}
}
//Check if IP is avaialable in clients
int check_is_ip_available_in_clients(int client_id, struct sockaddr_in addr)
{
	pthread_mutex_lock(&clients_chat_mutex);

	char ip1[BUFFER_SZ];
	sprintf(ip1, "%d.%d.%d.%d",
			addr.sin_addr.s_addr & 0xff,
			(addr.sin_addr.s_addr & 0xff00) >> 8,
			(addr.sin_addr.s_addr & 0xff0000) >> 16,
			(addr.sin_addr.s_addr & 0xff000000) >> 24);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			char ip2[BUFFER_SZ];
			sprintf(ip2, "%d.%d.%d.%d",
					clients[i]->address.sin_addr.s_addr & 0xff,
					(clients[i]->address.sin_addr.s_addr & 0xff00) >> 8,
					(clients[i]->address.sin_addr.s_addr & 0xff0000) >> 16,
					(clients[i]->address.sin_addr.s_addr & 0xff000000) >> 24);
			if (clients[i]->client_id != client_id && strcmp(ip1, ip2) == 0)
			{

				pthread_mutex_unlock(&clients_chat_mutex);
				return 0;
			}
			bzero(ip2, BUFFER_SZ);
		}
	}
	bzero(ip1, BUFFER_SZ);
	pthread_mutex_unlock(&clients_chat_mutex);
	return 1;
}

//Handle client inactivity, changing status if it doesnt send anything for a period of time
void *handle_client_inactivity(void *arg)
{
	client_str *cli = (client_str *)arg;
	int msec = 0;
	while (1)
	{
		clock_t difference = clock() - cli->last_connection;
		msec = difference * 1000 / CLOCKS_PER_SEC;
		if (msec > trigger_inactivity_timer && cli->status_changed_last_connection == 0 && strcmp(cli->status, "inactivo") != 0)
		{
			cli->status_changed_last_connection = 1;
			change_user_status(cli, "inactivo", cli->name, 1);
		};
	}
}

// Handle all communication with client
void *handle_client(void *arg)
{

	char buff_out[BUFFER_SZ];
	char name[32];
	int leave_flag = 0;
	clients_count++;
	client_str *cli = (client_str *)arg;
	if (recv(cli->sockfd, buff_out, BUFFER_SZ, 0) <= 0)
	{
		send_server_failure_response("Error al enviar mensaje en el cliente.\n", cli, 1);
		leave_flag = 1;
	}

	Chat__ClientPetition *cli_ptn_register;
	Chat__UserRegistration *user;
	cli_ptn_register = chat__client_petition__unpack(NULL, strlen(buff_out), buff_out);
	int optionRegister = (cli_ptn_register->option);
	user = cli_ptn_register->registration;
	if (optionRegister != 1)
	{
		send_server_failure_response("Error. Se esperaba un registro de usuario con nombre.\n", cli, 1);
		leave_flag = 1;
	}
	if (leave_flag == 0 && (strlen(user->username) < 2 || strlen(user->username) >= 32 - 1))
	{
		send_server_failure_response("Error. El nombre debe estar entre 2 y 32 caracteres..\n", cli, 1);
		leave_flag = 1;
	}

	if (leave_flag == 0 && strcmp(user->username, "everyone") == 0)
	{
		send_server_failure_response("El nombre everyone se encuentra reservado. Utiliza otro nombre.\n", cli, 1);
		leave_flag = 1;
	}

	if (leave_flag == 0 && check_is_name_available_in_clients(user->username, cli->client_id) == 0)
	{
		leave_flag = 1;
		send_server_failure_response("El nombre de usuario ya existe. Por favor utilice un nombre diferente.\n", cli, 1);
	}
	if (leave_flag == 0 && check_is_ip_available_in_clients(cli->client_id, cli->address) == 0)
	{
		leave_flag = 1;
		send_server_failure_response("La IP enviada ya se encuentra conectada a nuestro servidor. Para conectarse debe cerrar sesion primero.\n", cli, 1);
	}
	//If name and ip are valid continue to chat
	if (leave_flag == 0)
	{
		strcpy(cli->name, user->username);
		sprintf(buff_out, "%s se ha unido al chat.\n", cli->name);
		printf("%s", buff_out);
		send_success_server_response("***** BIENVENIDO AL CHAT DE C ***** \n", cli, 1);
		broadcast_message(buff_out, cli);
	}

	bzero(buff_out, BUFFER_SZ);

	//Continue listening in this thread for any client interaction
	while (1)
	{
		if (leave_flag)
		{
			break;
		}

		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0)
		{

			if (strlen(buff_out) > 0)
			{
				//Update client last connection
				cli->last_connection = clock();
				cli->status_changed_last_connection = 0;

				// Read packed message from standard-input.
				// Unpack the message using protobuf-c.
				Chat__ClientPetition *cli_ptn;
				Chat__MessageCommunication *msg;
				Chat__UserRequest *user_request;
				cli_ptn = chat__client_petition__unpack(NULL, strlen(buff_out), buff_out);
				int option = (cli_ptn->option);

				//Given option you can do different thing in chat.
				switch (option)
				{
				//Repeat code of register a client
				case 1:
					printf("Esta opcion no debería estar contemplada en esta parte.");
					break;
					if (leave_flag == 0 && (strlen(cli_ptn->registration->username) < 2 || strlen(cli_ptn->registration->username) >= 32 - 1))
					{
						send_server_failure_response("Error. El nombre debe estar entre 2 y 32 caracteres..\n", cli, 1);
						leave_flag = 1;
					}

					if (leave_flag == 0 && strcmp(cli_ptn->registration->username, "everyone") == 0)
					{
						send_server_failure_response("El nombre everyone se encuentra reservado. Utiliza otro nombre.\n", cli, 1);
						leave_flag = 1;
					}

					if (leave_flag == 0 && check_is_name_available_in_clients(cli_ptn->registration->username, cli->client_id) == 0)
					{
						leave_flag = 1;
						send_server_failure_response("El nombre de usuario ya existe. Por favor utilice un nombre diferente.\n", cli, 1);
					}
					if (leave_flag == 0 && check_is_ip_available_in_clients(cli->client_id, cli->address) == 0)
					{
						leave_flag = 1;
						send_server_failure_response("La IP enviada ya se encuentra conectada a nuestro servidor. Para conectarse debe cerrar sesion primero.\n", cli, 1);
					}
					//If name and ip are valid continue to chat
					if (leave_flag == 0)
					{
						strcpy(cli->name, cli_ptn->registration->username);
						sprintf(buff_out, "%s se ha unido al chat.\n", cli->name);
						printf("%s", buff_out);
						send_success_server_response("***** BIENVENIDO AL CHAT DE C ***** \n", cli, 1);
						broadcast_message(buff_out, cli);
					}
				case 2:
					get_users_list(cli);
					break;
				case 3:
					change_user_status(cli, cli_ptn->change->status, cli_ptn->change->username, 0);
					break;
				case 4:
					//Send a message
					//if recipient is everyone send it to general chat
					msg = cli_ptn->messagecommunication;
					if (msg == NULL)
					{
						fprintf(stderr, "Error. Mensaje recibido fue enviado con error por el cliente.\n");
						break;
					}

					// printf("\n");
					if (strcmp(msg->recipient, "everyone") == 0)
					{

						char buff_out2[BUFFER_SZ];
						sprintf(buff_out2, "%s\n", msg->message);
						printf("Chat General %s -> %s\n", msg->sender, msg->message);
						broadcast_message(buff_out2, cli);
					}
					else
					{
						char buff_out2[BUFFER_SZ];
						sprintf(buff_out2, "%s\n", msg->message);
						send_private_message(buff_out2, cli, msg->recipient);
					}
					// Free the unpacked message
					chat__message_communication__free_unpacked(msg, NULL);
					break;
				case 5:
					get_user_information_request(cli, cli_ptn->users->user);
					break;
				default:
					break;
				}
			}
		}
		else if (receive == 0 || strcmp(buff_out, "exit") == 0)
		{
			sprintf(buff_out, "%s ha dejado el chat.\n", cli->name);
			printf("%s", buff_out);
			broadcast_message(buff_out, cli);
			leave_flag = 1;
		}
		else
		{
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}

	///Delete client from server and close connection
	close(cli->sockfd);
	remove_client_from_server(cli->client_id);
	free(cli);
	clients_count--;
	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Error. Debe enviarse unicamente el <port> al correr. Se ha enviado %s \n", argv[0]);
		return EXIT_FAILURE;
	}

	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;
	pthread_t tid2;

	//When server is closed
	// signal(SIGINT, catch_ctrl_c_and_exit);

	//Setting of socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY; //Receive from any address
	serv_addr.sin_port = htons(port);

	//Ignore pipe signals
	signal(SIGPIPE, SIG_IGN);

	//Set Sockets
	if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
	{
		perror("Error al setear el socket.");
		return EXIT_FAILURE;
	}

	//Bind
	if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("Error al hacer el bind del sockets.");
		return EXIT_FAILURE;
	}

	//Listen
	if (listen(listenfd, 10) < 0)
	{
		perror("Error al escuchar en el socket.");
		return EXIT_FAILURE;
	}

	//Handle Chat
	printf("=== CHAT SERVER ===\n");
	while (1)
	{
		//Listen for connections
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

		//Check if max clients is reached
		if ((clients_count + 1) == MAX_CLIENTS)
		{
			printf("La cantidad maxima de usuarios esta en el chat. Lo sentimos. ");
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

		//Client Settings
		client_str *cli = (client_str *)malloc(sizeof(client_str));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->client_id = client_id++;

		//Add client to clients list
		add_client_to_server(cli);
		//Create threads to handle_client and handle_client_inactivity
		pthread_create(&tid, NULL, &handle_client, (void *)cli);
		pthread_create(&tid2, NULL, &handle_client_inactivity, (void *)cli);

		//Reduce CPU usage
		sleep(1);
	}
	catch_ctrl_c_and_exit(2);
	if (flag)
	{
		close_server();
	}
	return EXIT_SUCCESS;
}