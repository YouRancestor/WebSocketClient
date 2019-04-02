#include "WebSocketClientImplCurl.h"
static const char* msg = "Hello!";
using namespace ws;

class MyWsClient : public WebSocketClientImplCurl
{
    virtual void OnConnect(ConnectResult result)override;
    virtual void OnRecv(Message msg, bool fin) override;
};

int main(int argc, char *argv[])
{
    MyWsClient cl;
    cl.Connect("http://127.0.0.1:8000/ws", 0);
    
    while (true)
    {
        Sleep(1000);
    }
    return 0;
}

void MyWsClient::OnConnect(ConnectResult result)
{
    if (result == ws::WebSocketClientImplCurl::Success)
    {
        Message msg(ws::Text, msg, strlen(msg));

        Send(msg);

    }
    else
    {
        printf("Websocket connect failed.\n");
    }
}

void MyWsClient::OnRecv(Message msg, bool fin)
{
    printf("receive message: %s\n",msg.data);
}