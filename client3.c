#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include "new.pb-c.h"

		                                          //
// -------------------------------------------------------------------------- //
// -------------------------------------------------------------------------- //
// Chat realizado en C con multithreading y uso de protobuf                   //
// -------------------------------------------------------------------------- //
// -------------------------------------------------------------------------- //
// Para correr el server realizar los siguientes comandos                     //
// Primero instalar protobuf-c https://github.com/protobuf-c/                 //
// sudo make Makefile compile								                  //
// ./server3 port							                                  //
// -------------------------------------------------------------------------- //
// -------------------------------------------------------------------------- //
// Para correr el client realizar los siguientes comandos                     //
// Primero instalar protobuf-c https://github.com/protobuf-c/                 //
// sudo make Makefile compile								                  //
// ./client3 serverIP port					                                  //
// -------------------------------------------------------------------------- //
// With help of https://github.com/nikhilroxtomar/Chatroom-in-C               //
// -------------------------------------------------------------------------- //


// Global variables
#define MAX_MSG_SIZE 1024
#define BUFFER_SZ 2048 * 24
#define LENGTH 2048
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];
struct hostent *host;

//Print ">" into input
void str_overwrite_stdout()
{
    printf("%s", "> ");
    fflush(stdout);
}

//Remove line breaks
void str_trim_lf(char *arr, int length)
{
    int i;
    for (i = 0; i < length; i++)
    { // trim \n
        if (arr[i] == '\n')
        {
            arr[i] = '\0';
            break;
        }
    }
}

//Set flag exit
void catch_ctrl_c_and_exit(int sig)
{
    flag = 1;
}

//Broadcast message sender function
void broadcast_message()
{
    char message[LENGTH] = {};
    char buffer[LENGTH + 32] = {};
    char temp;
    printf("Ingresa tu mensaje o 'exit' para volver al menú principal.\n");
    scanf("%c", &temp);
    str_overwrite_stdout();
    scanf("%[^\n]", &message);

    //Protobuf structures init
    Chat__ClientPetition cli_ptn = CHAT__CLIENT_PETITION__INIT;
    Chat__MessageCommunication msg = CHAT__MESSAGE_COMMUNICATION__INIT; 
    void *buf;                                                          
    unsigned len;      

    //Set information
    msg.message = message;
    msg.recipient = "everyone";
    msg.sender = name;
    cli_ptn.messagecommunication = &msg;
    cli_ptn.option = 4;

    //Pack client petition
    len = chat__client_petition__get_packed_size(&cli_ptn);
    buf = malloc(len);
    chat__client_petition__pack(&cli_ptn, buf);

    //If message = "exit" return to menu
    if (strcmp(message, "exit") == 0)
    {
        return;
    }
    else
    {
        //Othewise send protobuf package to server
        send(sockfd, buf, len, 0);
    }

    //Free the allocated serialized buffer
    free(buf);
    bzero(message, LENGTH);
    bzero(buffer, LENGTH + 32);
}

//Private message to specific user sender function
void private_message()
{
    char message[LENGTH] = {};
    char user_name[LENGTH] = {};
    char buffer[LENGTH + 32] = {};
    char temp;
    scanf("%c", &temp);
    printf("Ingresa el nombre del usuario a quien deseas enviarle el mensaje.\n");
    str_overwrite_stdout();
    scanf("%[^\n]", &user_name);
    scanf("%c", &temp);
    printf("Ingresa tu mensaje o 'exit' para volver al menú principal.\n");
    str_overwrite_stdout();
    scanf("%[^\n]", &message);

    //Protobuf structures init
    Chat__ClientPetition cli_ptn = CHAT__CLIENT_PETITION__INIT;
    Chat__MessageCommunication msg = CHAT__MESSAGE_COMMUNICATION__INIT; // AMessage
    void *buf;                                                          // Buffer to store serialized data
    unsigned len;                 
    //Set information
    msg.message = message;
    msg.recipient = user_name;
    msg.sender = name;
    cli_ptn.messagecommunication = &msg;
    cli_ptn.option = 4;

    //Pack client petition
    len = chat__client_petition__get_packed_size(&cli_ptn);
    buf = malloc(len);
    chat__client_petition__pack(&cli_ptn, buf);

    //If message = "exit" return to menu
    if (strcmp(message, "exit") == 0)
    {
        return;
    }
    else
    {
        //Othewise send protobuf package to server
        send(sockfd, buf, len, 0);
    }

    // Free the allocated serialized buffer
    free(buf); 
    bzero(message, LENGTH);
    bzero(buffer, LENGTH + 32);
}

//Change Status Function
void change_status()
{
    int choice_status;
    char *status;
    char buffer[LENGTH + 32] = {};
    char temp;
    scanf("%c", &temp);
    printf("Ingresa la opción para cambiar de status:\n");
    printf("1. activo\n");
    printf("2. inactivo\n");
    printf("3. ocupado\n");
    str_overwrite_stdout();
    scanf("%d", &choice_status);

    switch (choice_status)
    {
    case 1:
        status = "activo";
        break;
    case 2:
        status = "inactivo";
        break;
    case 3:
        status = "ocupado";
        break;
    default:
        printf("La opción ingresada es icorrecta\n");
        return;
    }

    //Protobuf structures init
    Chat__ClientPetition cli_ptn = CHAT__CLIENT_PETITION__INIT;
    Chat__ChangeStatus new_status = CHAT__CHANGE_STATUS__INIT; // AMessage
    void *buf;                                                          // Buffer to store serialized data
    unsigned len;  

    //Set information
    new_status.status = status;
    new_status.username = name;
    cli_ptn.option = 3;
    cli_ptn.change = &new_status;
    
    //Pack client petition
    len = chat__client_petition__get_packed_size(&cli_ptn);
    buf = malloc(len);
    chat__client_petition__pack(&cli_ptn, buf);

    //Send protobuf package to server
    send(sockfd, buf, len, 0);

    // Free the allocated serialized buffer
    free(buf); 
    bzero(buffer, LENGTH + 32);
}

//Get user list from server function
void user_list()
{
    
    //Protobuf structures init
    Chat__ClientPetition cli_ptn = CHAT__CLIENT_PETITION__INIT;
    Chat__UserRequest user_request = CHAT__USER_REQUEST__INIT; // AMessage
    void *buf;                                                          // Buffer to store serialized data
    unsigned len;                 
    //Set information
    user_request.user = "everyone";
    cli_ptn.users = &user_request;
    cli_ptn.option = 2;

    //Pack client petition
    len = chat__client_petition__get_packed_size(&cli_ptn);
    buf = malloc(len);
    chat__client_petition__pack(&cli_ptn, buf);

    //Send protobuf package to server
    send(sockfd, buf, len, 0);

    // Free the allocated serialized buffer
    free(buf);
}

//Get specific user information from server
void user_information()
{
    char user_name[LENGTH] = {};
    char buffer[LENGTH + 32] = {};
    char temp;
    scanf("%c", &temp);
    printf("Ingresa el nombre del usuario de quien deseas ver la información.\n");
    str_overwrite_stdout();
    scanf("%[^\n]", &user_name);

    //Protobuf structures init
    Chat__ClientPetition cli_ptn = CHAT__CLIENT_PETITION__INIT;
    Chat__UserRequest user_request = CHAT__USER_REQUEST__INIT; // AMessage
    void *buf;                                                          // Buffer to store serialized data
    unsigned len;   
    //Set information             
    user_request.user = user_name;

    cli_ptn.users = &user_request;
    cli_ptn.option = 5;

    len = chat__client_petition__get_packed_size(&cli_ptn);
    buf = malloc(len);
    chat__client_petition__pack(&cli_ptn, buf);
    
    //If message = "exit" return to menu
    if (strcmp(user_name, "exit") == 0)
    {
        return;
    }
    else
    {
        //Othewise send protobuf package to server
        send(sockfd, buf, len, 0);
    }

    // Free the allocated serialized buffer
    free(buf); 
    bzero(user_name, LENGTH);
    bzero(buffer, LENGTH + 32);
}

//Client Menu Handler function
void client_menu_handler()
{

    while (flag == 0)
    {
        int choice;
        //Print Menu
        printf("\nMenu:\n");
        printf("1. Chatear con todos los usuarios (broadcasting).\n");
        printf("2. Enviar mensajes directos, privados, aparte del chat general.\n");
        printf("3. Cambiar de status.\n");
        printf("4. Listar los usuarios conectados al sistema de chat.\n");
        printf("5. Desplegar información de un usuario en particular.\n");
        printf("6. Ayuda.\n");
        printf("7. Salir.\n");
        scanf("%d", &choice);
        
        switch (choice)
        {
        case 1:
            //Send Broadcast Message
            broadcast_message();
            break;
        case 2:
            //Send Private Message
            private_message();
            break;
        case 3:
            //Change user status in the server 
            change_status();
            break;
        case 4:
            //Get connected user lis from server
            user_list();
            break;
        case 5:
            //Get user information from server
            user_information();
            break;
        case 6:
            //Help Message
            printf("Este es un chat generado con C y Protobuf para comunicarte por medio de\nun servidor con tus amigos. A continuación se te mostrará las funciones \nque puedes realizar en el chat. Para salir del chat ingresa la opción 7.\n");
            break;
        case 7:
            //Exit form menu
            printf("¡Gracias por usar el chat!\n");
            catch_ctrl_c_and_exit(2);
            break;
        default:
            //Error message
            printf("Wrong Choice. Enter again\n");
            break;
        }
    }
    catch_ctrl_c_and_exit(2);
}

void recv_msg_handler()
{
    char buff_out[BUFFER_SZ];
    while (1)
    {

        int receive = recv(sockfd, buff_out, BUFFER_SZ, 0);
        if (receive > 0)
        {
            
            //Protobuf structures init
            Chat__ServerResponse *server_res;
            Chat__MessageCommunication *msg;
            Chat__UserInfo *user_info;
            Chat__ConnectedUsersResponse *connected_user_info;

            //Unpack server response
            server_res = chat__server_response__unpack(NULL, strlen(buff_out), buff_out);

            //Get Response Code
            int code = server_res->code;
            //Get response option
            int option = (server_res->option);
            if (option!=0)
            {
                switch (option)
                {
                //User Register Response
                case 1:
                    if (code == 200)
                    {
                        printf("%s\n", server_res->servermessage);   
                    }
                    else
                    {
                        //Print Error Message
                        printf("%s\n", server_res->servermessage);
                        return EXIT_FAILURE;
                    }
                    break;
                //Connected Users Response
                case 2:
                    connected_user_info = server_res->connectedusers;
                    if (code == 200)
                    {
                        printf("Listado de Usuarios: \n");
                        for (int i = 0; i < connected_user_info->n_connectedusers; ++i)
                        {
                            if (connected_user_info->connectedusers[i])
                            {
                                printf("\t%d. Usuario: %s \n\tStatus: %s\n\tIP: %s\n", (i+1),connected_user_info->connectedusers[i]->username, connected_user_info->connectedusers[i]->status, connected_user_info->connectedusers[i]->ip);
                            }
                        }
                        
                    }
                    else if (code == 500)
                    {
                        //Print Error Message
                        printf("%s\n", server_res->servermessage);
                    }
                    break;
                //Change Status Response
                case 3:
                    if (code == 200){
                        //Print Success Message
                        printf("%s\n", server_res->servermessage);
                    }
                    else if (code == 500)
                    {
                        //Print Error Message
                        printf("%s\n", server_res->servermessage);
                    }
                    break;
                //Messages Response
                case 4:
                    msg = server_res->messagecommunication;

                    if (strcmp(msg->recipient, "everyone") == 0)
                    {
                        printf("Chat General enviado: %s\n", msg->message);
                    }
                    else
                    {
                        printf("Chat Privado enviado por %s -> %s\n", msg->sender, msg->message);
                    }
                    break;
                //User Information Response
                case 5:
                    user_info = server_res->userinforesponse;
                     if (code == 200)
                    {
                        printf("Infof de usuario:\n");
                        printf("\tUsuario: %s \n\tStatus: %s\n\tIP: %s\n", user_info->username, user_info->status, user_info->ip);
                    }
                    else if (code == 500)
                    {
                        //Print Error Message
                        printf("%s\n", server_res->servermessage);
                    }
                    break;
                default:
                    break;
                }
            }
            else if (code == 200 && option==0){
                //Print Error Message
                printf("%s\n", server_res->servermessage);
                
            }
            else if (code == 500 && option==0)
            {
                //Print Error Message
                printf("%s\n", server_res->servermessage);
            }
        }
        else if (receive == 0)
        {
            break;
        }
        else
        {
            break;
        }
        
        //Free buffer
        bzero(buff_out, BUFFER_SZ);
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        //Args error message   
        printf("Para ejecutar el programa ingresa ./client3 <serverIP> <port>\nDentro del programa se te solicitara tu nombre de usuario.\n", argv[0]);
        return EXIT_FAILURE;
    }

    host = gethostbyname(argv[1]);
    if (!host)
    {
        fprintf(stderr, "%s: error: Host desconocido: %s\n", argv[0], argv[1]);
        return -4;
    }
    int port = atoi(argv[2]);

    signal(SIGINT, catch_ctrl_c_and_exit);

    //Get user name
    printf("Ingresa tu nombre de usuario: ");
    str_overwrite_stdout();
    fgets(name, 32, stdin);
    str_trim_lf(name, strlen(name));

    if (strlen(name) > 32 || strlen(name) < 2)
    {
        //Error Message
        printf("El nombre del usuario tiene que ser entre 2 y 30 caracteres.\n");
        return EXIT_FAILURE;
    }

    //Socket Init
    struct sockaddr_in server_addr;

    //Socket settings
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);
    //Connect to Server
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1)
    {
        //Connection Error Message
        printf("Error de conexión con el servidor.\n");
        return EXIT_FAILURE;
    }

    //Set client ip
    char *ip_client;
    ip_client = "3.142.76.26";
    

    //Create Protobuf User Registration
    Chat__ClientPetition cli_ptn_register = CHAT__CLIENT_PETITION__INIT;
    Chat__UserRegistration user = CHAT__USER_REGISTRATION__INIT; 
    void *buf;                                                          
    unsigned len;              
    
    //Set user information
    user.username = name;
    user.ip = ip_client;
 
    cli_ptn_register.option = 1;
    cli_ptn_register.registration = &user;
    
    //Pack protobuf petition
    len = chat__client_petition__get_packed_size(&cli_ptn_register);
    buf = malloc(len);
    chat__client_petition__pack(&cli_ptn_register, buf);

    //Send User Registation
    send(sockfd, buf, len, 0);

    char buff_out[BUFFER_SZ];
    //Wait for server response
    // int receive = recv(sockfd, buff_out, BUFFER_SZ, 0);
    // if (receive > 0)
    // {
    //     //Init Protobuf Server Response
    //     Chat__ServerResponse *server_res;
    //     Chat__MessageCommunication *msg;

    //     //Unpack Protobuf Server Response
    //     server_res = chat__server_response__unpack(NULL, strlen(buff_out), buff_out);

    //     //Get Response Code
    //     int code = server_res->code;
    //     //Get Response Option
    //     int option = (server_res->option);
    //     if (code == 200)
    //     {
    //         //Print Success Message
    //         printf("%s\n", server_res->servermessage);   
    //     }
    //     else
    //     {
    //         //Print Error Message
    //         printf("%s\n", server_res->servermessage);
    //         //return EXIT_FAILURE;
    //     }
    // }
    // else
    // {
    //     return EXIT_FAILURE;
    // }

    // bzero(buff_out, BUFFER_SZ);


    //Create client menu handler thread
    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void *)client_menu_handler, NULL) != 0)
    {
        printf("Error al crear el thread.\n");
        return EXIT_FAILURE;
    }

    //Create client recieve message from server handler thread
    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void *)recv_msg_handler, NULL) != 0)
    {
        printf("Error al crear el thread.\n");
        return EXIT_FAILURE;
    }

    while (1)
    {
        if (flag)
        {
            //Goodbye Message
            printf("\n¡Hasta Pronto!\n");
            break;
        }
    }

    //Close socket
    close(sockfd);

    //Exit Program
    return EXIT_SUCCESS;
}