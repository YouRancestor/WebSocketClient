#pragma once
#include <stdint.h>

#ifdef WEBSOCKET_CLIENT_STATIC
#define WEBSOCKET_CLIENT_API
#else
#if defined _WIN32 || defined __CYGWIN__
#ifdef WEBSOCKET_CLIENT_EXPORTS
#ifdef __GNUC__
#define WEBSOCKET_CLIENT_API __attribute__ ((dllexport))
#else
#define WEBSOCKET_CLIENT_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#ifdef __GNUC__
#define WEBSOCKET_CLIENT_API __attribute__ ((dllimport))
#else
#define WEBSOCKET_CLIENT_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#endif
#else
#if __GNUC__ >= 4
#define WEBSOCKET_CLIENT_API __attribute__ ((visibility ("default")))
#define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#else
#define WEBSOCKET_CLIENT_API
#endif
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

typedef enum websocket_frame_type_t
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
} websocket_frame_type_t;

typedef struct websocket_message_t
{
    websocket_frame_type_t type;
    const char* data;
    int len; // size of data in bytes
}websocket_message_t;

typedef enum websocket_client_connect_result_t
{
    Success = 0,
    Timeout = 1,
    Reject = 2
} websocket_client_connect_result_t;

typedef struct websocket_client_t websocket_client_t;

typedef void (*websocket_client_connect_callback)(websocket_client_connect_result_t result, void* opaque);

typedef void (*websocket_client_receive_callback)(websocket_message_t msg, int fin, void* opaque);

/**
 * @brief create a websocket client instance
 * @return websocket client instance
 * @sa @anchor websocket_client_create_with_custom_http_header @anchor websocket_client_destroy
 */
WEBSOCKET_CLIENT_API websocket_client_t* websocket_client_create();

/**
 * @brief create a websocket client instance
 * @param custom_header array for customize HTTP header, one array item represents one line in HTTP
 * header, including the request line
 * @param nlines @em custom_header array size
 * @return  websocket client instance
 * @sa @anchor websocket_client_create @anchor websocket_client_destroy
 */
WEBSOCKET_CLIENT_API websocket_client_t* websocket_client_create_with_custom_http_header(const char** custom_header, int nlines);

/**
 * @brief destroy the specific websocket client instance
 * @param client websocket client instance created by @anchor websocket_client_create
 * or @anchor websocket_client_create_with_custom_http_header
 * @sa @anchor websocket_client_create @anchor websocket_client_create_with_custom_http_header
 */
WEBSOCKET_CLIENT_API void websocket_client_destroy(websocket_client_t* client);

/**
 * @brief connect to websocket server
 * @param client websocket client instance
 * @param url the websocket server's url to connect
 * @note This function is non-blocking, it starts a thread to establish the connection and returns immediately. \n
 * The @em url string will be saved as a copy, you can release it after this function returns. \n
 * Set callbacks by calling @anchor websocket_client_set_callbacks before connect to server.
 */
WEBSOCKET_CLIENT_API void websocket_client_connect_server(websocket_client_t* client, const char* url);

/**
 * @brief send message to server
 * @param client websocket client instance
 * @param msg the message to send
 * @return the total number of bytes sent
 * @note You should make sure the connection has established or the message will not be sent.
 */
WEBSOCKET_CLIENT_API int websocket_client_send_sessage(websocket_client_t* client, websocket_message_t msg);

/**
 * @brief websocket_client_set_callbacks
 * @param client websocket client instance
 * @param conn_cb callback to check connetion result
 * @param recv_cb callback to receive message
 * @param opaque private pointer for the above callbacks
 */
WEBSOCKET_CLIENT_API void websocket_client_set_callbacks(websocket_client_t* client,
                                                         websocket_client_connect_callback conn_cb,
                                                         websocket_client_receive_callback recv_cb,
                                                         void* opaque );

#ifdef __cplusplus
}
#endif // __cplusplus
