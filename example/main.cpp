#include "WebSocketClientImplCurl.h"
#include <string.h>
#include <thread>

static const char* message = "Hello!";
using namespace ws;

class MyWsClient : public WebSocketClientImplCurl
{
    virtual void OnConnect(ConnectResult result)override;
    virtual void OnRecv(Message msg, bool fin) override;
};

static const char g_url[] = "http://127.0.0.1:8000/ws";

int main(int argc, char *argv[])
{
    MyWsClient cl;

	char * url = new char[sizeof(g_url)];
	memcpy(url, g_url, sizeof(g_url));
    cl.Connect(url);
	delete [] url;

	std::this_thread::sleep_for(std::chrono::seconds(1));

	cl.Close();

	std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0;
}

void MyWsClient::OnConnect(ConnectResult result)
{
    if (result == ws::WebSocketClientImplCurl::Success)
    {
        Message msg(ws::Text, message, strlen(message));

        Send(msg);

    }
    else
    {
        printf("Websocket connect failed.\n");
    }
}

void MyWsClient::OnRecv(Message msg, bool fin)
{
    printf("receive message(type %d): %s\n", msg.type, msg.data);
}
