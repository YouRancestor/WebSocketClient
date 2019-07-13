#include <websocket_client.h>
#include <WebSocketClientImplCurl.h>
using namespace ws;
struct websocket_client_t : public WebSocketClientImplCurl
{
    websocket_client_t() :conn_cb(NULL), recv_cb(NULL), opaque(NULL) {}
    virtual void OnConnect(ConnectResult result)override;
    virtual void OnRecv(Message msg, bool fin) override;

    websocket_client_connect_callback conn_cb;
    websocket_client_receive_callback recv_cb;
    void* opaque;
};

websocket_client_t* websocket_client_create()
{
    return new websocket_client_t;
}
 void websocket_client_destroy(websocket_client_t* client)
{
    delete client;
}

void websocket_client_connect_server(websocket_client_t* client, const char* url)
{
    client->Connect(url);
}

int websocket_client_send_sessage(websocket_client_t* client, websocket_message_t msg)
{
    return client->Send(ws::Message((FrameType)msg.type, msg.data, msg.len));
}

void websocket_client_set_callbacks(
    websocket_client_t* client,
    websocket_client_connect_callback conn_cb,
    websocket_client_receive_callback recv_cb,
    void* opaque)
{
    client->conn_cb = conn_cb;
    client->recv_cb = recv_cb;
    client->opaque = opaque;
}

void websocket_client_t::OnConnect(ConnectResult result)
{
    if(this->conn_cb)
        this->conn_cb((websocket_client_connect_result_t)result, this->opaque);
}

void websocket_client_t::OnRecv(Message msg, bool fin)
{
    if (this->recv_cb)
    {
        websocket_message_t message;
        message.type = (websocket_frame_type_t)msg.type;
        message.data = msg.data;
        message.len = msg.len;
        this->recv_cb(message, fin, this->opaque);
    }
}
