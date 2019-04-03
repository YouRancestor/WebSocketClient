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

        /**
         * @brief WebSocketClientImplCurl
         * @param customHeader array for customize HTTP header, one array item represents one line in HTTP
         * header, including the request line
         * @param nlines @em customHeader array size
         * @note This funcion copies the header strings @em customHeader specified.
         *
         * For example the default header array is defined as follows.
         * @snippet WebSocketClientImplCurl.cpp default HTTP header
         */
        WebSocketClientImplCurl(const char** customHeader, int nlines);
        virtual ~WebSocketClientImplCurl();
        enum ConnectResult
        {
            Success = 0,
            Timeout = 1,
            Reject = 2
        };

        /**
         * @brief Connect to websocket server.
         * @param url the websocket server url
         * @param timeout in milliseconds, setting to 0 means it never times out during transfer
         * @note This function is non-blocking, it starts a thread to establish the connection and returns immediately. \n
         * The @em url string will be saved as a copy, you can release it after this function returns.
         */
        void Connect(const char* url, long timeout);

        /**
         * @brief On connect
         * @param result the connection result
         *
         * This function will be invoked when the connection process is finished, both in success and failure
         * cases.
         */
        virtual void OnConnect(ConnectResult result);

        /**
         * @brief Close the websocket connection.
         *
         * Send a message which @em FrameType is @em Close to server.
         * If the connection @em State is not @em Connected, this function will do nothing.
         */
        void Close();

        enum State
        {
            Disconnected = 0,
            Connecting,
            Connected,
        };

        /**
         * @brief Get the client's conntion state.
         * @return current conntion state
         */
        State GetState();

		/**
         * @brief Send message to server.
         * @param msg the message to send
		 */
        int Send(Message msg);

        /**
         * @brief On receive
         * @param msg received message
         * @param fin if this data frame is a last frame
         *
         * This function will be invoked when receives a message from server.
         * @note @em msg will be invalid after this function returns, save @em msg.data as a copy if needed.
         */
        virtual void OnRecv(Message msg, bool fin);

    protected:
        /**
         * @brief Get the status code of HTTP response
         * @return state code
         */
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
