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



	enum websocket_frame_type
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

	enum websocket_connect_result
	{
		Success = 0,
		Timeout = 1,
		Reject = 2
	};

	typedef struct websocket_message
	{
		websocket_frame_type type;
		const char* data;
		int len; // size of data in bytes
	}websocket_message;


	typedef struct websocket_client_t websocket_client_t;

	typedef void (*websocket_connect_callback)(websocket_connect_result result, void* opaque);

	typedef void (*websocket_receive_callback)(websocket_message msg, int fin, void* opaque);

	WEBSOCKET_CLIENT_API websocket_client_t* websocket_create_client();

	WEBSOCKET_CLIENT_API void websocket_connect_server(websocket_client_t* client, const char* url);

	WEBSOCKET_CLIENT_API int websocket_send_sessage(websocket_client_t* client, websocket_message msg);

	WEBSOCKET_CLIENT_API void websocket_close_connection(websocket_client_t* client);

	WEBSOCKET_CLIENT_API void websocket_set_callbacks( websocket_client_t* client,
													   websocket_connect_callback conn_cb,
													   websocket_receive_callback recv_cb,
													   void* opaque );



#ifdef __cplusplus
}
#endif // __cplusplus