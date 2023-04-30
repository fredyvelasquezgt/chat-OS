#include <iostream>
#include <string>
#include <string.h>
#include <time.h>
#include "user.h"

using namespace std;

string universal_compatible_status[]={"ACTIVO","OCUPADO","INACTIVO"};

User::User()
{
	username = "";
	socket = 0;
	status = ACTIVE;
}

string User::to_string()
{
	string data;
	data = "\t" + username + "\t" + ip_address + "\t" + User::get_status();
	return data;
}

string User::get_status()
{
	if (status == ACTIVE)
		return universal_compatible_status[0];
	else if (status == BUSY)
		return universal_compatible_status[1];
	else
		return universal_compatible_status[2];
}

void User::set_status(string new_status)
{
	if (strcmp(new_status.c_str(), universal_compatible_status[0].c_str()) == 0)
		status = ACTIVE;
	else if (strcmp(new_status.c_str(), universal_compatible_status[1].c_str()) == 0)
		status = BUSY;
	else
		status = INACTIVE;
}

void User::update_last_activity_time() {
	time(&last_activity);
}