#include <stdio.h>
#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <map>
#include <vector>
#include "payload.pb.h"
#include "user.h"
#include <time.h>

#define IP "127.0.0.1"
#define DELAY 50
#define BUFFER_SIZE 8192
#define CONNECTED 1
#define DISCONNECTED 0
#define BROADCAST_SENDER "broadcast"
#define PRIVATE_SENDER "private"

using namespace std;
using namespace google::protobuf;

map<string, vector<string>> inbox_messages;
string universal_compatible_status[]={"ACTIVO","OCUPADO","INACTIVO"};
int connected;
bool has_server_response;
string current_server_message;
string current_error_message;

/****************************************
*           Ncurses Config
****************************************/

void make_line(WINDOW *window,int x, int y)
{
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    attron(COLOR_PAIR(1));

    wmove(window, y, 0);
    whline(window,' ', x);

    attroff(COLOR_PAIR(1));
    wmove(window, y+1, 0);
}

void make_main_menu(char* username, int option)
{
    mvprintw(0, 0, "### WELCOME TO THE CHAT ###");
    mvprintw(1, 0, username);

    if(option==1)
        {

            attron(COLOR_PAIR(1));
            mvprintw(3, 0,"Show connected users");
            attroff(COLOR_PAIR(1));
            mvprintw(4, 0,"Show user info");
            mvprintw(5, 0,"Change status");
            mvprintw(6, 0,"Send broadcast message");
            mvprintw(7, 0,"Send private message");
            mvprintw(8, 0,"Help");
            mvprintw(9, 0,"Exit");

        }
        else if(option==2)
        {
            mvprintw(3, 0,"Show connected users");
            attron(COLOR_PAIR(1));
            mvprintw(4, 0,"Show user info");
            attroff(COLOR_PAIR(1));
            mvprintw(5, 0,"Change status");
            mvprintw(6, 0,"Send broadcast message");
            mvprintw(7, 0,"Send private message");
            mvprintw(8, 0,"Help");
            mvprintw(9, 0,"Exit");
        }
        else if(option==3)
        {
            mvprintw(3, 0,"Show connected users");
            mvprintw(4, 0,"Show user info");
            attron(COLOR_PAIR(1));
            mvprintw(5, 0,"Change status");
            attroff(COLOR_PAIR(1));
            mvprintw(6, 0,"Send broadcast message");
            mvprintw(7, 0,"Send private message");
            mvprintw(8, 0,"Help");
            mvprintw(9, 0,"Exit");
        }
        else if(option==4)
        {
            mvprintw(3, 0,"Show connected users");
            mvprintw(4, 0,"Show user info");
            mvprintw(5, 0,"Change status");
            attron(COLOR_PAIR(1));
            mvprintw(6, 0,"Send broadcast message");
            attroff(COLOR_PAIR(1));
            mvprintw(7, 0,"Send private message");
            mvprintw(8, 0,"Help");
            mvprintw(9, 0,"Exit");
        }
        else if(option==5)
        {
            mvprintw(3, 0,"Show connected users");
            mvprintw(4, 0,"Show user info");
            mvprintw(5, 0,"Change status");
            mvprintw(6, 0,"Send broadcast message");
            attron(COLOR_PAIR(1));
            mvprintw(7, 0,"Send private message");
            attroff(COLOR_PAIR(1));
            mvprintw(8, 0,"Help");
            mvprintw(9, 0,"Exit");
        }
        else if(option==6)
        {
            mvprintw(3, 0,"Show connected users");
            mvprintw(4, 0,"Show user info");
            mvprintw(5, 0,"Change status");
            mvprintw(6, 0,"Send broadcast message");
            mvprintw(7, 0,"Send private message");
            attron(COLOR_PAIR(1));
            mvprintw(8, 0,"Help");
            attroff(COLOR_PAIR(1));
            mvprintw(9, 0,"Exit");
        }
        else if(option==7)
        {
            mvprintw(3, 0,"Show connected users");
            mvprintw(4, 0,"Show user info");
            mvprintw(5, 0,"Change status");
            mvprintw(6, 0,"Send broadcast message");
            mvprintw(7, 0,"Send private message");
            mvprintw(8, 0,"Help");
            attron(COLOR_PAIR(1));
            mvprintw(9, 0,"Exit");
            attroff(COLOR_PAIR(1));
        }
}


void print_status_options(int position)
{
    mvprintw(0, 0,"Status change");

    if(position==0)
    {
        attron(COLOR_PAIR(1));
        mvprintw(2, 0, universal_compatible_status[0].c_str()); 
        attroff(COLOR_PAIR(1));
        mvprintw(3, 0, universal_compatible_status[1].c_str()); 
        mvprintw(4, 0, universal_compatible_status[2].c_str());
    }
    else if(position==1)
    {
        mvprintw(2, 0, universal_compatible_status[0].c_str()); 
        attron(COLOR_PAIR(1));
        mvprintw(3, 0, universal_compatible_status[1].c_str()); 
        attroff(COLOR_PAIR(1));
        mvprintw(4, 0, universal_compatible_status[2].c_str());
    }
    else if(position==2)
    {
        mvprintw(2, 0, universal_compatible_status[0].c_str()); 
        mvprintw(3, 0, universal_compatible_status[1].c_str()); 
        attron(COLOR_PAIR(1));
        mvprintw(4, 0, universal_compatible_status[2].c_str());
        attroff(COLOR_PAIR(1));
    }

}

void print_help()
{
    mvprintw(0, 0,"Help");
    mvprintw(2, 0,  "--------------------------------------------");
    mvprintw(3, 0,  "To navigate you use the keyboard arrows, to select an option you use the Enter key and return you use the ESC key.");


    mvprintw(6, 0,  "MENU OPTIONS:");

    mvprintw(8, 0,  "Show connected users:");
    mvprintw(9, 0,  "The information of the users who are registered in the server is displayed, to see extra information, select a user.");


    mvprintw(12, 0,  "Show user info:");
    mvprintw(13, 0,  "This option allows you to show user info based on their usernames");


    mvprintw(16, 0,  "Change status:");
    mvprintw(17, 0,  "This option allows you to change the status of the user (Active, Busy, Inactive).");


    mvprintw(20, 0,  "Broadcast message:");
    mvprintw(21, 0,  "Chat with all connected users.");

    mvprintw(23, 0,  "Private message:");
    mvprintw(24, 0,  "Private chat with a single user.");    
}


/****************************************
*               GUI Utils
****************************************/


string get_chat_name(int position)
{
    int idx = 0;
    map<string,vector<string>>::iterator iter;
    vector<string> chat;

    for (iter=inbox_messages.begin(); iter != inbox_messages.end(); iter++)
    {
        if(idx++==position) return(iter->first);
    }
}

void add_new_inbox_message(string username, string init_message)
{
    if(inbox_messages.find(username) == inbox_messages.end()) {
        // Not found, create new element
        vector<string> vect; 
        vect.push_back(init_message);
        inbox_messages[username]=vect;
    } 
    else 
    {
        // Found
        inbox_messages[username].push_back(init_message);
    }
    
}

void print_inbox_chat(int max, int init_screen_position, bool show_broadcast = false)
{
    int idx=0;
    vector<string> chat;

    if (show_broadcast) {
        if(inbox_messages.find(BROADCAST_SENDER) == inbox_messages.end()) {
            // Not found, create new element
            vector<string> vect; 
            inbox_messages[BROADCAST_SENDER]=vect;
        } 
        chat = inbox_messages.at(BROADCAST_SENDER);
    } else {
        if(inbox_messages.find(PRIVATE_SENDER) == inbox_messages.end()) {
            // Not found, create new element
            vector<string> vect; 
            inbox_messages[PRIVATE_SENDER]=vect;
        } 
        chat = inbox_messages.at(PRIVATE_SENDER);
    }


    if(!chat.empty())
    {
        // If you want to add seen & unseen feature to your messages
        // chat[chat.size()-1]->seen = 1;

        int fix = (chat.size() - max);
        if(fix < 0) fix = 0;
        idx=0;

        mvprintw(1, 0, "Chats: %d", chat.size());
        for (int i=(0 + fix); i < chat.size(); i++) 
        {
            string final_message = chat[i];

            char dest[final_message.size() + 1];
            strcpy(dest, final_message.c_str());
            mvprintw(init_screen_position + (idx++), 0, "%s", dest);
        }
    }
}



/****************************************
*               Chat & Threads
****************************************/

void *listen(void *args){
    // Classmates private message pattern
    // Classmates broadcast message pattern
    const string classmates_broadcast_pattern[2] = {"Mensaje general de ", "(general):"};
    const string classmates_private_pattern[2] = {"Mensaje privado de ", "(private):"};


    while (true)
	{
		char buffer[BUFFER_SIZE];
		int received_message = recv(*(int *)args, buffer, BUFFER_SIZE, 0);
        
		Payload payload;
		payload.ParseFromString(buffer);
        string message = payload.message();

        if (payload.code() == 500)
        {
            has_server_response = true;
            current_error_message = message;
            break;
		}

        if (payload.code() == 200 || payload.flag() == Payload_PayloadFlag_general_chat) 
        {
            
          

            // Broadcast Messages
            bool is_real_broadcast = false, is_real_private = false;
            if(payload.flag() == Payload_PayloadFlag_general_chat) {
                //Omit broadcast that just have Server Log Info (not a real broadcast message)
                for (int i = 0; i < 2; i++) 
                {
                    if (message.find(classmates_broadcast_pattern[i]) != string::npos) 
                    {
                        is_real_broadcast = true;
                    } 
                }
                
                if (is_real_broadcast) add_new_inbox_message(BROADCAST_SENDER, message);
            }
            // Private Messages
            else if (payload.flag() == Payload_PayloadFlag_private_chat) {
                //Omit private message that just have Server Log Info (not a real private message)
                for (int i = 0; i < 2; i++) 
                {
                    if (message.find(classmates_private_pattern[i]) != string::npos)
                    {
                        is_real_private = true;
                    }
                }
                
                if (is_real_private) add_new_inbox_message(PRIVATE_SENDER, message);
            }

            has_server_response = true;
            if(!is_real_broadcast && !is_real_private) current_server_message = message;
            current_error_message = "";
        } else 
        {
            cout << "FATAL ERROR WHILE LISTENING SERVER..." << endl;
            current_error_message = "FATAL ERROR WHILE LISTENING SERVER...";
            break;
        }


		if (connected == 0) pthread_exit(0);
	}
}

void send_message_to_server(string username, string ip, Payload_PayloadFlag flag, string message, string extra, char *buffer, int socket)   
{
    string message_serialized;
    Payload *request_payload = new Payload();
	request_payload->set_sender(username);
	request_payload->set_flag(flag);
    if (extra != NULL && extra != "") request_payload->set_extra(extra);
    if (message != NULL && message != "") request_payload->set_message(message);
	request_payload->set_ip(ip);

	request_payload->SerializeToString(&message_serialized);

	strcpy(buffer, message_serialized.c_str());
	send(socket, buffer, message_serialized.size() + 1, 0);
	has_server_response = false;
}


int main(int argc, char *argv[]) 
{
    /*
        The gethostbyname() and gethostbyaddr() functions are deprecated on 
        most platforms, and they don't implement support for IPv6. 
        So, we need a collection of hints for the IP field
    */
    int socket_fd, err;
	struct addrinfo hints = {}, *addrs, *addr;
	char server_ip[INET_ADDRSTRLEN];
	char buffer[BUFFER_SIZE];
	char *username, *server, *port;
    
    GOOGLE_PROTOBUF_VERIFY_VERSION;

	if (argc != 4)
	{
		printf("Usage: %s <username> <server-ip> <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

    username = argv[1];
    server = argv[2];
    port = argv[3];

    // Socket connection
    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // Since your original code was using sockaddr_in and
                               // PF_INET, I'm using AF_INET here to match.  Use
                               // AF_UNSPEC instead if you want to allow getaddrinfo()
                               // to find both IPv4 and IPv6 addresses for the hostname.
                               // Just make sure the rest of your code is equally family-
                               // agnostic when dealing with the IP addresses associated
                               // with this connection. For instance, make sure any uses
                               // of sockaddr_in are changed to sockaddr_storage,
                               // and pay attention to its ss_family field, etc...
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    err = getaddrinfo(server, port, &hints, &addrs);
	if (err != 0)
	{
		fprintf(stderr, "%s: %s\n", server, gai_strerror(err));
		return EXIT_FAILURE;
	}

	for (addr = addrs; addr != NULL; addr = addr->ai_next)
	{
		if ((socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol)) == -1)
		{
			continue;
		}

		if (connect(socket_fd, addr->ai_addr, addr->ai_addrlen) == -1)
		{
			close(socket_fd);
			continue;
		}
		break;
	}

	if (addr == NULL)
	{
		printf("ERROR ON GET SERVER IP\n");
		return EXIT_FAILURE;
	}

	inet_ntop(
        addr->ai_family, 
        &((struct sockaddr_in *) addr->ai_addr)->sin_addr, 
        server_ip, 
        sizeof server_ip
    );
	freeaddrinfo(addrs);

	printf("STARTING HANDSHAKE WITH SERVER IP: %s\n", server_ip);
    // Server handshake
    string message_serialized;
    Payload *handshake_payload = new Payload;

	handshake_payload->set_sender(username);
	handshake_payload->set_flag(Payload_PayloadFlag_register_);
	handshake_payload->set_ip(server_ip);
	handshake_payload->SerializeToString(&message_serialized);

	strcpy(buffer, message_serialized.c_str());
	send(socket_fd, buffer, message_serialized.size() + 1, 0);
	recv(socket_fd, buffer, BUFFER_SIZE, 0);

	Payload server_payload;
	server_payload.ParseFromString(buffer);
    string server_message = server_payload.message();

	if(server_payload.code() != 200) {
		printf("ERROR: SERVER RESPONSE WAS: %s\n", server_message.c_str());
        return EXIT_FAILURE;
    }
	connected = CONNECTED;

	pthread_t client_thread_id;
	pthread_attr_t attrs;

	pthread_attr_init(&attrs);
	pthread_create(&client_thread_id, &attrs, listen, (void *)&socket_fd);

    printf("Finally connected...\n");

	// Manage menu of Ncurses
    char input_buffer[100] = {0}, *s = input_buffer;
    int entered_key;
    int screen_number=1, main_menu_option=1, position_status_change=0;
    string temporal_private_recipient, temporal_private_message;
    WINDOW *window;
 
    if ((window = initscr()) == NULL) {
        fprintf(stderr, "Error: initscr()\n");
        exit(EXIT_FAILURE);
    }

    // Allow special keys
    keypad(stdscr, TRUE); 
    // disable auto echo
    noecho();
    // disable line-buffering
    cbreak();      
    // wait DELAY milliseconds for input
    timeout(DELAY);  
    
    bool sent_request;
    while (connected != DISCONNECTED) {
        erase();

        int x,y;
        getmaxyx(stdscr,y,x);

        if(screen_number==1)
        {
            // Main Menu
            current_error_message = "";
            current_server_message = "";
            temporal_private_recipient = "";
            temporal_private_message = "";
            sent_request = false;

            start_color();
            init_pair(1, COLOR_BLACK, COLOR_CYAN);

            make_main_menu(username, main_menu_option);        
           
            refresh();
            if ((entered_key = getch()) != ERR) 
            {
                if (entered_key == 27) {
                    //If user press [ESC]
                    connected=DISCONNECTED;
                }
                else if(entered_key==KEY_UP)
                {
                    if(main_menu_option--<=1) main_menu_option=7;

                }
                else if(entered_key==KEY_DOWN)
                {
                    if(main_menu_option++>=7) main_menu_option=1;
                    
                }
                else if (entered_key=='\n') 
                {
                    if(main_menu_option==1)
                    {
                        main_menu_option=1;
                        screen_number=9;
                        
                    }
                    else if(main_menu_option==2)
                    {
                        main_menu_option=1;
                        screen_number=10;
                    }
                    else if(main_menu_option==3)
                    {
                        main_menu_option=1;
                        screen_number=7;
                    }
                    else if(main_menu_option==4)
                    {
                        main_menu_option=1;
                        screen_number=2;
                    }
                    else if(main_menu_option==5)
                    {
                        main_menu_option=1;
                        screen_number=5;
                    }
                    else if(main_menu_option==6)
                    {
                        main_menu_option=1;
                        screen_number=8;
                    }
                    else if(main_menu_option==7) connected=DISCONNECTED;
                }

            }
        }
        else if(screen_number==2)
        {
            mvprintw(0, 0,"--- All Broadcast Messages ---");
            sent_request = false;
             
            if(current_error_message!="")
            {
                char error_message_char[current_error_message.size() + 1];
                strcpy(error_message_char, current_error_message.c_str());
                mvprintw(y-1, 0,"%s", error_message_char);
            }

            print_inbox_chat(y-7, 2, true);
            
            make_line(window, x, y-5);
            mvprintw(y-4, 0, "> %s", input_buffer);

            refresh();
             
            // getch (with cbreak and timeout as above)
            // waits 100ms and returns ERR if it doesn't read anything.
            if ((entered_key = getch()) != ERR) {
                if (entered_key == '\n') {
                    //If user press [ENTER]
                    if(strlen(input_buffer) > 0) 
                    {
                        if (!sent_request) {
                            send_message_to_server(username, server_ip, Payload_PayloadFlag_general_chat,
                                string(input_buffer), "", buffer, socket_fd);
                            // add_new_inbox_message(BROADCAST_SENDER, "(Me) " + string(input_buffer));
                            sent_request = true;
                        }
                    }


                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;
                }
                else if (entered_key == 27) {
                    //If user press [ESC]
                    screen_number=1;
                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;
                }
                else if (entered_key == KEY_BACKSPACE) {
                    if (s > input_buffer)
                        *--s = 0;
                }
                else if (s - input_buffer < (long)sizeof input_buffer - 1) {
                    *s++ = entered_key;
                    *s = 0;
                }
            }

        }
        else if(screen_number==5)
        {
            mvprintw(0, 0,"--- All Private Messages ---");
            temporal_private_recipient = "";
             
            if(current_error_message!="")
            {
                char error_message_char[current_error_message.size() + 1];
                strcpy(error_message_char, current_error_message.c_str());
                mvprintw(y-1, 0,"%s", error_message_char);
            }

            print_inbox_chat(y-7, 2, false);
            
            make_line(window, x, y-5);
            mvprintw(y-4, 0, "New Message To > %s", input_buffer);

            refresh();
             
            // getch (with cbreak and timeout as above)
            // waits 100ms and returns ERR if it doesn't read anything.
            if ((entered_key = getch()) != ERR) {
                if (entered_key == '\n') {
                    //If user press [ENTER]
                    // Set message recipient
                    if(strlen(input_buffer) > 0) {
                        screen_number = 6;
                        temporal_private_recipient = string(input_buffer);
                    }

                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;

                }
                else if (entered_key == 27) {
                    //If user press [ESC]
                    screen_number=1;
                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;
                }
                else if (entered_key == KEY_BACKSPACE) {
                    if (s > input_buffer)
                        *--s = 0;
                }
                else if (s - input_buffer < (long)sizeof input_buffer - 1) {
                    *s++ = entered_key;
                    *s = 0;
                }
            }
        }
        else if(screen_number==6)
        {
            // Private chat
            string title = "--- New Private Message To " + temporal_private_recipient + "---";
            mvprintw(0, 0, title.c_str());

            temporal_private_message = "";
            sent_request = false;

            mvprintw(y-4, 0, "> %s", input_buffer);
            refresh();

            if ((entered_key = getch()) != ERR) {
                if (entered_key == '\n') {
                    //If user press [ENTER]
                    // Set message content

                    if(strlen(input_buffer) > 0) 
                    {
                        temporal_private_message = string(input_buffer);
                        if (!sent_request) {
                            send_message_to_server(username, server_ip, Payload_PayloadFlag_private_chat,
                                temporal_private_message, temporal_private_recipient, buffer, socket_fd);
                            add_new_inbox_message(PRIVATE_SENDER, "(Me -> " + temporal_private_recipient + "): " + temporal_private_message);
                            sent_request = true;
                        }
                    }


                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;
                }
                else if (entered_key == 27) {
                    //If user press [ESC]
                    screen_number=5;
                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;
                }
                else if (entered_key == KEY_BACKSPACE) {
                    if (s > input_buffer)
                        *--s = 0;
                }
                else if (s - input_buffer < (long)sizeof input_buffer - 1) {
                    *s++ = entered_key;
                    *s = 0;
                }
            }

        }
        else if(screen_number==7)
        {
            // Change Status
            print_status_options(position_status_change);

            if(current_error_message!="")
            {
                char error_message_char[current_error_message.size() + 1];
                strcpy(error_message_char, current_error_message.c_str());
                mvprintw(y-1, 0,"ERR: %s",error_message_char);
            }
            
            refresh();

            if ((entered_key = getch()) != ERR) {
                if (entered_key == '\n') {
                    //If user press [ENTER] 
                    // TODO: change status
                    if (!sent_request) {
                        send_message_to_server(username, server_ip, Payload_PayloadFlag_update_status, 
                            "", universal_compatible_status[position_status_change], buffer, socket_fd);
                        sent_request = true;
                    }
                    screen_number=1;
                    position_status_change=0;

                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;
                }
                else if (entered_key == 27) {
                    //If user press [ESC]
                    screen_number=1;
                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;
                }
                else if(entered_key==KEY_UP)
                {
                    if(position_status_change--<=0) position_status_change=2;

                }
                else if(entered_key==KEY_DOWN)
                {
                    if(position_status_change++>=2) position_status_change=0;
                    
                }
                else if (entered_key == KEY_BACKSPACE) {
                    if (s > input_buffer)
                        *--s = 0;
                }
                else if (s - input_buffer < (long)sizeof input_buffer - 1) {
                    *s++ = entered_key;
                    *s = 0;
                }
            }

        }
        else if(screen_number==8)
        {
            print_help();
            refresh();
            // Help
            if ((entered_key = getch()) != ERR) 
            {
                if (entered_key == 27) {
                    //If user press [ESC]
                    screen_number=1;
                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;
                }
            }
        

        }
        else if(screen_number==9)
        {
            if (!sent_request) {
                send_message_to_server(username, server_ip, Payload_PayloadFlag_user_list, 
                    "", "", buffer, socket_fd);
                sent_request = true;
            }
            //Connected Users
            mvprintw(0, 0,"--- Connected Users ---");
            mvprintw(1, 0, "%s", current_server_message.c_str());

            if(current_error_message!="")
            {
                char error_message_char[current_error_message.size() + 1];
                strcpy(error_message_char, current_error_message.c_str());
                mvprintw(y-1, 0,"ERR: %s",error_message_char);
            }


            refresh();
            if ((entered_key = getch()) != ERR) 
            {
                if (entered_key == 27) {
                    //If user press [ESC]
                    screen_number=1;
                }

            }
        }
        else if(screen_number==10)
        {
            mvprintw(0, 0,"--- Show User Info ---");
            sent_request = false;

            // Print user info here (basically, the server message...);
            mvprintw(1, 0, "%s", current_server_message.c_str());

            if(current_error_message!="")
            {
                char error_message_char[current_error_message.size() + 1];
                strcpy(error_message_char, current_error_message.c_str());
                mvprintw(y-1, 0,"ERR: %s",error_message_char);
            }

            mvprintw(y-4, 0, "Enter username to search> %s", input_buffer);
            refresh();

            if ((entered_key = getch()) != ERR) 
            {
                if (entered_key == '\n') {
                    //If user press [ENTER]

                    if(strlen(input_buffer) > 0) 
                    {
                        if (!sent_request) {
                            send_message_to_server(username, server_ip, Payload_PayloadFlag_user_info,
                                "", string(input_buffer), buffer, socket_fd);
                            sent_request = true;
                        }
                    }


                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;
                }
                else if (entered_key == 27) {
                    //If user press [ESC]
                    screen_number=1;
                    *s = 0;
                    sscanf(input_buffer, "%d", &connected);
                    s = input_buffer;
                    *s = 0;
                }
                else if (entered_key == KEY_BACKSPACE) {
                    if (s > input_buffer)
                        *--s = 0;
                }
                else if (s - input_buffer < (long)sizeof input_buffer - 1) {
                    *s++ = entered_key;
                    *s = 0;
                }
            }
        }
        usleep(DELAY);
    }
 
    delwin(window);
    endwin();

	// Close connection
	pthread_cancel(client_thread_id);
	connected = DISCONNECTED;
	close(socket_fd);
	return EXIT_SUCCESS;
}
