#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include "chat.pb-c.h"

//Definiciones globales
#define BACKLOG 10
#define BUFFER_SIZE 1024

typedef struct
{
    char username[100];
    char ip[50];
    int socketFD;
    int status;
    time_t last_active;
} User;

//Lista de usuarios conectados
#define MAX_USERS 50
User userList[MAX_USERS];
int numUsers = 0;

//Devuelve si un usuario existe
int userExists(char *username)
{
    int i;
    for (i = 0; i < numUsers; i++)
    {
        if (strcmp(userList[i].username, username) == 0)
        {
            return 1; // El usuario ya existe en la lista
        }
    }
    return 0; // El usuario no existe en la lista
}

// Agrega un usuario a la lista
void addUser(char *username, char *ip, int socketFD, int status)
{
    if (numUsers >= MAX_USERS)
    {
        printf("La lista de usuarios está llena. No se puede agregar más usuarios.\n");
        return;
    }
    User newUser;
    strcpy(newUser.username, username);
    strcpy(newUser.ip, ip);
    newUser.socketFD = socketFD;
    newUser.status = status;
    newUser.last_active = time(NULL);
    userList[numUsers] = newUser;
    numUsers++;
}

// Elimina un usuario de la lista
void removeUser(char *username, char *ip, int socketFD, int status)
{
    int i, j;
    for (i = 0; i < numUsers; i++)
    {
        User user = userList[i];
        if (strcmp(user.username, username) == 0 && strcmp(user.ip, ip) == 0 && user.socketFD == socketFD)
        {
            // Encontró el usuario, lo elimina
            for (j = i; j < numUsers - 1; j++)
            {
                userList[j] = userList[j + 1];
            }
            numUsers--;
            printf("Usuario eliminado: %s\n", username);
            return;
        }
    }
    // No encontró al usuario
    printf("No se encontró al usuario: %s\n", username);
}

// revisar si esta activo de lo contraio cambiarlo a inactive
void* check_inactive_users(void *arg) {
    while (1) {
        time_t current_time = time(NULL);
        printf("________________TIME_______________________\n");
        for (int i = 0; i < numUsers; i++) {
            double elapsed_time = difftime(current_time, userList[i].last_active);
            printf("User: %s, Elapsed Time: %.0f\n", userList[i].username, elapsed_time);
            if (elapsed_time >= 60) {
                userList[i].status = 3; // Set the status to 3 if the user is inactive for 30 seconds
            }
        }
        sleep(15); // Check the userList every 15 seconds (or any other desired interval)
    }
}

//Funcion que maneja las respuestas al cliente
void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    int client_in_session = 1;

    // Recibir el registro del cliente
    uint8_t recv_buffer[BUFFER_SIZE];
    ssize_t recv_size = recv(client_socket, recv_buffer, sizeof(recv_buffer), 0);
    if (recv_size < 0)
    {
        perror("Error al recibir el mensaje del cliente");
        exit(1);
    }

    // Deserializar el registro de NewUser
    ChatSistOS__UserOption *usero_registration = chat_sist_os__user_option__unpack(NULL, recv_size, recv_buffer);
    if (usero_registration == NULL)
    {
        fprintf(stderr, "Error al deserializar el mensaje del cliente\n");
        exit(1);
    }

    ChatSistOS__NewUser *chat_registration = usero_registration->createuser;

    printf("\n >> Nuevo usuario conectado!  >> Nombre: %s  >> IP: %s\n", chat_registration->username, chat_registration->ip);
    // printf("Mensaje recibido del cliente %d: %s\n", client_socket, chat_message->content);

    // Informacion del Cliente asociada al thread
    User MyInfo;
    strcpy(MyInfo.username, chat_registration->username);
    strcpy(MyInfo.ip, chat_registration->ip);
    MyInfo.socketFD = client_socket;


    // Answer server
    ChatSistOS__Answer respuesta_servidor_registro = CHAT_SIST_OS__ANSWER__INIT;
    if (userExists(MyInfo.username) == 0)
    {
        // Agregar usuario conectado a la lista de usuarios
        addUser(chat_registration->username, chat_registration->ip, client_socket, 1);

        respuesta_servidor_registro.op = 0;
        respuesta_servidor_registro.response_status_code = 400;
        respuesta_servidor_registro.response_message = "Fuiste Registrado! :D";
        // respuesta_servidor.message = &response;

        // Serializar la respuesta en un buffer
        size_t serialized_size_servidor_registro = chat_sist_os__answer__get_packed_size(&respuesta_servidor_registro);
        uint8_t *buffer_servidor_registro = malloc(serialized_size_servidor_registro);
        chat_sist_os__answer__pack(&respuesta_servidor_registro, buffer_servidor_registro);

        // Enviar el buffer de respuesta a través del socket
        if (send(MyInfo.socketFD, buffer_servidor_registro, serialized_size_servidor_registro, 0) < 0)
        {
            perror("Error al enviar la respuesta");
            exit(1);
        }

        // Liberar los buffers y el mensaje
        free(buffer_servidor_registro);
    }
    else
    {
        // ChatSistOS__Answer respuesta_servidor_registro          = CHAT_SIST_OS__ANSWER__INIT;
        respuesta_servidor_registro.op = 0;
        respuesta_servidor_registro.response_status_code = 200;
        respuesta_servidor_registro.response_message = "Este usuario ya existe! D:";
        // respuesta_servidor.message = &response;

        // Serializar la respuesta en un buffer
        size_t serialized_size_servidor_registro = chat_sist_os__answer__get_packed_size(&respuesta_servidor_registro);
        uint8_t *buffer_servidor_registro = malloc(serialized_size_servidor_registro);
        chat_sist_os__answer__pack(&respuesta_servidor_registro, buffer_servidor_registro);

        // Enviar el buffer de respuesta a través del socket
        if (send(MyInfo.socketFD, buffer_servidor_registro, serialized_size_servidor_registro, 0) < 0)
        {
            perror("Error al enviar la respuesta");
            exit(1);
        }

        // Liberar los buffers y el mensaje
        free(buffer_servidor_registro);
        // removeUser(chat_registration->username,chat_registration->ip,client_socket, 0);
    }

    chat_sist_os__user_option__free_unpacked(usero_registration, NULL);

    //Escuchando useroptions recibidas
    printf("\n\n Entrando en el ciclo de opciones de [%s]\n", MyInfo.username);
    while (1)
    {
        printf("\n");

        uint8_t recv_buffer_opcion[BUFFER_SIZE];
        ssize_t recv_size_opcion = recv(client_socket, recv_buffer_opcion, sizeof(recv_buffer_opcion), 0);
        if (recv_size_opcion < 0)
        {
            perror("Error al recibir el mensaje del cliente");
            exit(1);
        }
        if (recv_size_opcion == 0)
        {
            perror("El cliente se ha desconectado");
            goto salir_del_ciclo;
        }
        // Deserializar la opcion elegida del cliente
        ChatSistOS__UserOption *client_opcion = chat_sist_os__user_option__unpack(NULL, recv_size_opcion, recv_buffer_opcion);
        if (client_opcion == NULL)
        {
            fprintf(stderr, "Error al deserializar el mensaje del cliente\n");
            exit(1);
        }

        int opcion_elegida = client_opcion->op;
        printf("[%s] escogio --> [%d]", MyInfo.username, opcion_elegida);
        switch (opcion_elegida)
        {
        case 1:
            printf("\n\n");
            ChatSistOS__Message *mensaje_recibido = client_opcion->message;

            // Recorrer la lista de usuarios
            for (int i = 0; i < numUsers; i++)
            {
                if (strcmp(userList[i].username, MyInfo.username) == 0)
                {
                    // revisar si el usuario esta inactivo en este caso activarlo como activo
                    if (userList[i].status == 3){
                        userList[i].status = 1;
                    }
                    // Si el usuario se llama Gabriel, omitirlo y continuar con el siguiente
                    userList[i].last_active = time(NULL);
                    continue;
                }

                ChatSistOS__Answer respuesta_servidor = CHAT_SIST_OS__ANSWER__INIT;
                respuesta_servidor.op = 1;
                respuesta_servidor.response_status_code = 400;
                respuesta_servidor.message = mensaje_recibido;

                // Serializar la respuesta en un buffer
                size_t serialized_size_servidor = chat_sist_os__answer__get_packed_size(&respuesta_servidor);
                uint8_t *buffer_servidor = malloc(serialized_size_servidor);
                chat_sist_os__answer__pack(&respuesta_servidor, buffer_servidor);

                // Enviar el buffer de respuesta a través del socket
                if (send(userList[i].socketFD, buffer_servidor, serialized_size_servidor, 0) < 0)
                {
                    perror("Error al enviar la respuesta");
                    exit(1);
                }

                // Liberar los buffers y el mensaje
                free(buffer_servidor);
            }

            break;
        case 2:

            printf("\n\n");
            ChatSistOS__Message *mensaje_recibido_directo = client_opcion->message;

            // Recorrer la lista de usuarios
            int enviar_mensaje = 0;
            int indice_usuario = 0;
            for (int i = 0; i < numUsers; i++)
            {
                if (strcmp(userList[i].username, mensaje_recibido_directo->message_destination) == 0)
                {
                    // revisar si el usuario esta inactivo en este caso activarlo como activo
                    if (userList[i].status == 3){
                        userList[i].status = 1;
                    }
                    userList[i].last_active = time(NULL);
                    enviar_mensaje = 1;
                    indice_usuario = i;
                }
            }

            if (enviar_mensaje == 1)
            {
                // Si el usuario se encuentra
                ChatSistOS__Answer respuesta_servidor = CHAT_SIST_OS__ANSWER__INIT;
                respuesta_servidor.op = 2;
                respuesta_servidor.response_status_code = 400;
                respuesta_servidor.message = mensaje_recibido_directo;

                // Serializar la respuesta en un buffer
                size_t serialized_size_servidor = chat_sist_os__answer__get_packed_size(&respuesta_servidor);
                uint8_t *buffer_servidor = malloc(serialized_size_servidor);
                chat_sist_os__answer__pack(&respuesta_servidor, buffer_servidor);

                // Enviar el buffer de respuesta a través del socket
                if (send(userList[indice_usuario].socketFD, buffer_servidor, serialized_size_servidor, 0) < 0)
                {
                    perror("Error al enviar la respuesta");
                    exit(1);
                }

                // Liberar los buffers y el mensaje
                free(buffer_servidor);
            }
            else
            {

                // Si el usuario no se encuentra
                ChatSistOS__Answer respuesta_servidor = CHAT_SIST_OS__ANSWER__INIT;
                respuesta_servidor.op = 2;
                respuesta_servidor.response_status_code = 200;
                respuesta_servidor.response_message = "USUARIO NO ENCONTRADO";
                respuesta_servidor.message = mensaje_recibido_directo;

                // Serializar la respuesta en un buffer
                size_t serialized_size_servidor = chat_sist_os__answer__get_packed_size(&respuesta_servidor);
                uint8_t *buffer_servidor = malloc(serialized_size_servidor);
                chat_sist_os__answer__pack(&respuesta_servidor, buffer_servidor);

                // Enviar el buffer de respuesta a través del socket
                if (send(MyInfo.socketFD, buffer_servidor, serialized_size_servidor, 0) < 0)
                {
                    perror("Error al enviar la respuesta");
                    exit(1);
                }

                // Liberar los buffers y el mensaje
                free(buffer_servidor);
            }

            break;
        case 3:
            // Lógica para manejar la opción 3
                printf("\n\n");
                ChatSistOS__Status *estatus_recibido = client_opcion->status;
                // Recorrer la lista de usuarios
                for (int i = 0; i < numUsers; i++) {
                    if (strcmp(userList[i].username, MyInfo.username) == 0) {
                        // revisar si el usuario esta inactivo en este caso activarlo como activo
                        if (userList[i].status == 3){
                            userList[i].status = 1;
                        }
                        // Si el usuario se llama Gabriel, cambiar su estado
                        userList[i].last_active = time(NULL);
                        userList[i].status = estatus_recibido->user_state;

                        ChatSistOS__Answer respuesta_servidor          = CHAT_SIST_OS__ANSWER__INIT;
                        respuesta_servidor.op   =   3 ;
                        respuesta_servidor.response_status_code = 400;
                        respuesta_servidor.response_message = "\nStatus changed succesfully";
                    }
                }
            break;
        case 4:
            printf("\n\n");

            //Usuarios online
            ChatSistOS__UsersOnline usuarios_conectados = CHAT_SIST_OS__USERS_ONLINE__INIT;
            usuarios_conectados.n_users = numUsers;
            usuarios_conectados.users   = malloc(sizeof(ChatSistOS__User *) * numUsers);

            for (int i = 0; i < numUsers; i++)
            {
                if (strcmp(userList[i].username, MyInfo.username) == 0) {
                    // revisar si el usuario esta inactivo en este caso activarlo como activo
                    if (userList[i].status == 3){
                        userList[i].status = 1;
                    }
                    userList[i].last_active = time(NULL);
                }
                ChatSistOS__User *new_user = malloc(sizeof(ChatSistOS__User));
                chat_sist_os__user__init(new_user);
                new_user->user_name = userList[i].username;
                new_user->user_state = userList[i].status;
                new_user->user_ip = userList[i].ip;

                usuarios_conectados.users[i] = new_user;
            }

            // Answer del servidor
            ChatSistOS__Answer respuesta_servidor = CHAT_SIST_OS__ANSWER__INIT;
            respuesta_servidor.op = 4;
            respuesta_servidor.response_status_code = 400;
            respuesta_servidor.response_message = "Lista de usuarios Conectados";
            respuesta_servidor.users_online = &usuarios_conectados;

            // Serializar la respuesta en un buffer
            size_t serialized_size_servidor = chat_sist_os__answer__get_packed_size(&respuesta_servidor);
            uint8_t *buffer_servidor = malloc(serialized_size_servidor);
            chat_sist_os__answer__pack(&respuesta_servidor, buffer_servidor);

            // Enviar el buffer de respuesta a través del socket
            if (send(MyInfo.socketFD, buffer_servidor, serialized_size_servidor, 0) < 0)
            {
                perror("Error al enviar la respuesta");
                exit(1);
            }
            free(buffer_servidor);
            // Lógica para manejar la opción 4
            break;
        case 5:
            {
                printf("\n\n");
                int user_found = 0;
                //Usuarios online
                ChatSistOS__UsersOnline usuarios_conectados = CHAT_SIST_OS__USERS_ONLINE__INIT;
                usuarios_conectados.n_users = numUsers;
                usuarios_conectados.users   = malloc(sizeof(ChatSistOS__User *) * numUsers);

                for (int i = 0; i < numUsers; i++)
                {
                    if (strcmp(userList[i].username, MyInfo.username) == 0) {
                        // revisar si el usuario esta inactivo en este caso activarlo como activo
                        if (userList[i].status == 3){
                            userList[i].status = 1;
                        }
                        userList[i].last_active = time(NULL);
                    }
                    ChatSistOS__User *new_user = malloc(sizeof(ChatSistOS__User));
                    chat_sist_os__user__init(new_user);
                    ChatSistOS__User *empty = malloc(sizeof(ChatSistOS__User));
                    chat_sist_os__user__init(empty);
                    empty->user_name = "Vacio";
                    new_user->user_name = userList[i].username;
                    new_user->user_state = userList[i].status;
                    new_user->user_ip = userList[i].ip;
                    if(strcmp(userList[i].username, client_opcion->userlist->user_name) == 0){
                        usuarios_conectados.users[i] = new_user;
                        user_found = 1;
                    }else{
                        usuarios_conectados.users[i] = empty;
                    }
                    
                }

                // Answer del servidor
                ChatSistOS__Answer respuesta_servidor = CHAT_SIST_OS__ANSWER__INIT;
                respuesta_servidor.op = 5;
                if (user_found == 1)
                {
                    respuesta_servidor.response_status_code = 400;
                }else{
                    respuesta_servidor.response_status_code = 200;
                }
                

                respuesta_servidor.response_message = "Lista de usuarios Conectados";
                respuesta_servidor.users_online = &usuarios_conectados;

                // Serializar la respuesta en un buffer
                size_t serialized_size_servidor = chat_sist_os__answer__get_packed_size(&respuesta_servidor);
                uint8_t *buffer_servidor = malloc(serialized_size_servidor);
                chat_sist_os__answer__pack(&respuesta_servidor, buffer_servidor);

                // Enviar el buffer de respuesta a través del socket
                if (send(MyInfo.socketFD, buffer_servidor, serialized_size_servidor, 0) < 0)
                {
                    perror("Error al enviar la respuesta");
                    exit(1);
                }
                free(buffer_servidor);
            break;
            }
        case 6:
            // Lógica para manejar la opción 6
            for (int i = 0; i < numUsers; i++){
                if (strcmp(userList[i].username, MyInfo.username) == 0) {
                    // revisar si el usuario esta inactivo en este caso activarlo como activo
                    if (userList[i].status == 3){
                        userList[i].status = 1;
                    }
                    userList[i].last_active = time(NULL);
                }
            }
            break;
        case 7:
            chat_sist_os__user_option__free_unpacked(client_opcion, NULL);
            goto salir_del_ciclo;
        default:
            fprintf(stderr, "Opción no válida: %d\n", opcion_elegida);
            break;
        }

        // Libera el desempaquetamiento
        chat_sist_os__user_option__free_unpacked(client_opcion, NULL);
    }
    salir_del_ciclo:
    removeUser(MyInfo.username, MyInfo.ip, MyInfo.socketFD, MyInfo.status);

    printf("\n\n ---- Usuarios dentro del chat ----\n");

    for (int i = 0; i < numUsers; i++)
    {
        printf("Información del usuario #%d:\n", i + 1);
        printf("Nombre de usuario: %s\n", userList[i].username);
        printf("Dirección IP: %s\n", userList[i].ip);
        printf("Descriptor de archivo del socket: %d\n", userList[i].socketFD);
        printf("Estado: %d\n", userList[i].status);
        printf("\n");
    }

    close(client_socket);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Uso: %s <puerto>\n", argv[0]);
        exit(1);
    }

    int server_port = atoi(argv[1]);

    // Crear el socket del servidor

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Error al crear el socket del servidor");
        exit(1);
    }

    // Permitir la reutilización de la dirección y puerto del servidor

    int option = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
    {
        perror("Error al configurar las opciones del socket");
        exit(1);
    }

    // Configurar la dirección y puerto del servidor

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_port);

    // Enlazar el socket del servidor a la dirección y puerto especificados
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Error al enlazar el socket del servidor");
        exit(1);
    }

    // Escuchar conexiones entrantes
    if (listen(server_socket, BACKLOG) < 0)
    {
        perror("Error al escuchar conexiones entrantes");
        exit(1);
    }

    printf("Servidor iniciado en el puerto %d\n", server_port);

    while (1)
    {
        // Esperar a que llegue una conexión
        struct sockaddr_in client_address;
        socklen_t client_address_length = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket < 0)
        {
            perror("Error al aceptar la conexión del cliente");
            exit(1);
        }

        printf("\nCliente conectado desde %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        // Crear un nuevo hilo para el cliente
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, (void *)&client_socket) < 0)
        {
            perror("Error al crear el hilo del cliente");
            exit(1);
        }
        // Crear un hilo para llevar el control de tiempo de los clientes
        pthread_t inactive_users_thread;
        if (pthread_create(&inactive_users_thread, NULL, check_inactive_users, NULL)) {
            perror("Error al crear el hilo del tiempo");
            exit(1);
        }
    }

    return 0;
}