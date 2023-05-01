#include <time.h>
#include <iostream>
#include <string>
#include <netinet/in.h>

using namespace std;

enum UserStatus
{
  ACTIVE,
  BUSY,
  INACTIVE
};

class User
{
public:
  User();
  string username; // unique
  UserStatus status;
  time_t last_activity;
  char ip_address[INET_ADDRSTRLEN]; // unique
  int socket;
  string to_string();
  string get_status();
  void set_status(string new_status);
  void update_last_activity_time();
};
















#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define INET_ADDRSTRLEN 16

typedef enum {
    ACTIVE,
    BUSY,
    INACTIVE
} UserStatus;

typedef struct {
    char username[256];
    UserStatus status;
    time_t last_activity;
    char ip_address[INET_ADDRSTRLEN];
    int socket;
} User;

void init_user(User *user) {
    memset(user->username, 0, sizeof(user->username));
    user->status = ACTIVE;
    user->last_activity = time(NULL);
    memset(user->ip_address, 0, INET_ADDRSTRLEN);
    user->socket = -1;
}

char* user_to_string(User *user) {
    static char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "Username: %s\nStatus: %d\nLast activity: %ld\nIP address: %s\nSocket: %d\n",
             user->username, user->status, user->last_activity, user->ip_address, user->socket);
    return buffer;
}

char* user_get_status(User *user) {
    switch (user->status) {
        case ACTIVE:
            return "ACTIVE";
        case BUSY:
            return "BUSY";
        case INACTIVE:
            return "INACTIVE";
        default:
            return "UNKNOWN";
    }
}

void user_set_status(User *user, UserStatus new_status) {
    user->status = new_status;
}

void user_update_last_activity_time(User *user) {
    user->last_activity = time(NULL);
}

int main() {
    User user1;
    init_user(&user1);
    strcpy(user1.username, "user1");
    user1.status = BUSY;
    strcpy(user1.ip_address, "192.168.0.1");
    user1.socket = 1;

    printf("%s", user_to_string(&user1));
    printf("User status: %s\n", user_get_status(&user1));
    user_set_status(&user1, INACTIVE);
    printf("User status: %s\n", user_get_status(&user1));
    user_update_last_activity_time(&user1);
    printf("Last activity: %ld\n", user1.last_activity);

    return 0;
}
