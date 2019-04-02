#pragma once
#include <curl/curl.h>
#include <stdint.h>
namespace ws {

    enum FrameType
    {
        // Non-control frame.
        Continuation = 0x0,
        Text = 0x1,
        Binary = 0x2,
        NonControlReserved1 = 0x3,
        NonControlReserved2 = 0x4,
        NonControlReserved3 = 0x5,
        NonControlReserved4 = 0x6,
        NonControlReserved5 = 0x7,

        // Control frame.
        Close = 0x8,
        Ping = 0x9,
        Pong = 0xA,
        ControlReserved1 = 0xB,
        ControlReserved2 = 0xC,
        ControlReserved3 = 0xD,
        ControlReserved4 = 0xE,
        ControlReserved5 = 0xF,
    };



    struct Message
    {
        Message(FrameType type, const char* data, uint64_t len) :type(type), data(data), len(len) {}
        FrameType type;
        const char* data;
        uint64_t len; // size of data in bytes
    };


    class WebSocketClientImplCurl
    {
    public:
        WebSocketClientImplCurl(); // Using default headers
        WebSocketClientImplCurl(const char** customHeader, int nlines);
        virtual ~WebSocketClientImplCurl();
        enum ConnectResult
        {
            Success = 0,
            Timeout = 1,
            Reject = 2
        };
        void Connect(const char* url, int timeout);
        virtual void OnConnect(ConnectResult result);
        void Close();
        virtual void OnClose();


        enum State
        {
            Disconnected = 0,
            Connecting,
            Connected,
        };

        State GetState();

        int Send(Message msg);
        virtual void OnRecv(Message msg, bool fin);

        long GetResponseCode();

    private:
        static curl_socket_t OpenSocketCallback(void *clientp, curlsocktype purpose, struct curl_sockaddr *address);
        static size_t OnHeaderReceived(char *buffer, size_t size, size_t nitems, void *userdata);
        static size_t OnMessageReceived(char *ptr, size_t size, size_t nmemb, void *userdata);
        static void RecvProc(void* userdata);

        static void ConnProc(WebSocketClientImplCurl* pthis);

        void SetState(State newState);

        CURL* m_curl;
        curl_slist* m_header_list_ptr;
        curl_socket_t m_sockfd;   // send message to server through this fd

        State m_state;    // connection state
    };

}