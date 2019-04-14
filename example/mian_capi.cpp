#include "websocket_client.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>

static const char g_url[] = "http://127.0.0.1:8000/ws";
static const char* message = "Hello!";

void on_connect(websocket_connect_result result, void* opaque);

void on_receive(websocket_message msg, int fin, void* opaque);


int main(int argc, char* argv[])
{
	websocket_client_t* wsclient = websocket_create_client();
	char* url = (char* )malloc(sizeof(g_url));
	if (url)
	{
		strcpy_s(url, sizeof(g_url), g_url);
	}
	else
	{
		printf("not enough memory");
		return 1;
	}

	websocket_set_callbacks(wsclient, on_connect, on_receive, wsclient);

	websocket_connect_server(wsclient, url);
	free(url);

	std::this_thread::sleep_for(std::chrono::seconds(2));

	websocket_close_connection(wsclient);

	std::this_thread::sleep_for(std::chrono::seconds(5));

	return 0;
}

void on_connect(websocket_connect_result result, void* opaque)
{
	websocket_client_t* wsclient = (websocket_client_t*)opaque;
	if (result == Success)
	{
		websocket_message msg;
		msg.type = Text;
		msg.data = message;
		msg.len = strlen(message);

		websocket_send_sessage(wsclient, msg);

	}
	else
	{
		printf("Websocket connect failed.\n");
	}

}

void on_receive(websocket_message msg, int fin, void* opaque)
{
	printf("receive message(type %d): %s\n", msg.type, msg.data);
}
