
/*                 The WebSocket Protocol
The WebSocket Protocol enables two-way communication between a client
running untrusted code in a controlled environment to a remote host
that has opted-in to communications from that code.  The security
model used for this is the origin-based security model commonly used
by web browsers.  The protocol consists of an opening handshake
followed by basic message framing, layered over TCP.  The goal of
this technology is to provide a mechanism for browser-based
applications that need two-way communication with servers that does
not rely on opening multiple HTTP connections (e.g., using
XMLHttpRequest or <iframe>s and long polling).

Standard:
https://tools.ietf.org/html/rfc6455

Frame:
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
*/
#include "WebSocketClientImplCurl.h"
#include <string.h>
#include <thread>
#include <ctime>
using namespace ws;

struct WsHeaderLittleEndian
{
    uint8_t opcode : 4;
    uint8_t rsv3 : 1;
    uint8_t rsv2 : 1;
    uint8_t rsv1 : 1;
    uint8_t fin : 1;

    uint8_t payloadlen : 7;
    uint8_t masked : 1;
};
struct WsHeaderBigEndian
{
    uint8_t fin : 1;
    uint8_t rsv1 : 1;
    uint8_t rsv2 : 1;
    uint8_t rsv3 : 1;
    uint8_t opcode : 4;

    uint8_t masked : 1;
    uint8_t payloadlen : 7;
};

union WsHeader
{
    WsHeaderLittleEndian little;
    WsHeaderBigEndian big;
};

union Endian
{
    uint16_t s;
    char c[sizeof(uint16_t)];
};
static Endian un = { 0x0102 };
static bool HostIsBigEndian() {
    if (un.c[0] == 1)
        return true;
    else
        return false;
}

// transform integer's byte-order from net to host or from host to net
template <class T>
static T net_to_host(T num)
{
    if (HostIsBigEndian())
        return num;
    else
    {
        T ret;
        char* pin = (char*)&num;
        char* pout = (char*)&ret;
        size_t n = sizeof(T);
        for (size_t i = 0; i < n; ++i)
        {
            pout[i] = pin[n - 1 - i];
        }
        return ret;
    }
}

union mask
{
    char chararr[4];
    int32_t integer;
};
static void WsMask(char* data, uint64_t len, const char mask_key[4])
{
    for (int i = 0; i < len; ++i)
    {
        int j = i % 4;
        data[i] = data[i] ^ mask_key[j];
    }
}

//! [default HTTP header]
static const char *defaultHeaders[] = {
    "HTTP/1.1 101 WebSocket Protocol Handshake",
    "Upgrade: websocket",
    "Connection: Upgrade",
    "Sec-WebSocket-Version: 13",
    "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw=="
};
//! [default HTTP header]

WebSocketClientImplCurl::WebSocketClientImplCurl()
    : WebSocketClientImplCurl(defaultHeaders, sizeof(defaultHeaders) / sizeof(char *))
{

}

WebSocketClientImplCurl::WebSocketClientImplCurl(const char ** customHeader, int nlines)
    : m_curl(NULL)
    , m_header_list_ptr(NULL)
    , m_sockfd(0)
    , m_state(WebSocketClientImplCurl::Disconnected)
    , sendbuff(NULL)
    , sendbufflen(0)
    , sendoffset(0)
{
    // Init curl
    m_curl = curl_easy_init();
    if (!m_curl)
        throw "curl init failed";

    // Set HTTP headers
    if (nlines == 0 || customHeader == NULL)
    {
        customHeader = defaultHeaders;
        nlines = sizeof(defaultHeaders) / sizeof(char*);
    }
    m_header_list_ptr = curl_slist_append(NULL, customHeader[0]);
    for (int i = 1; i < nlines; ++i)
    {
        m_header_list_ptr = curl_slist_append(m_header_list_ptr, customHeader[i]);
    }
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_header_list_ptr);

    // Set HTTP callbacks
    curl_easy_setopt(m_curl, CURLOPT_OPENSOCKETFUNCTION, OpenSocketCallback);
    curl_easy_setopt(m_curl, CURLOPT_OPENSOCKETDATA, this);
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, OnHeaderReceived);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, OnMessageReceived);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);


}

WebSocketClientImplCurl::~WebSocketClientImplCurl()
{
    curl_slist_free_all(m_header_list_ptr);
    curl_easy_cleanup(m_curl);
}

void WebSocketClientImplCurl::Connect(const char * url)
{
    curl_easy_setopt(m_curl, CURLOPT_URL, url);
    std::thread th_conn(ConnProc, this);
    th_conn.detach();
}

void WebSocketClientImplCurl::OnConnect(ConnectResult result)
{
}

void WebSocketClientImplCurl::Close()
{
    Message msg(ws::Close, NULL, 0);
    Send(msg);
}

WebSocketClientImplCurl::State WebSocketClientImplCurl::GetState()
{
    return m_state;
}

#define FILL_WS_HEADER(endian) \
        header.endian.fin = true; \
        header.endian.opcode = msg.type; \
        if (header.endian.opcode & 0x7) \
        { \
            /* Non-control frame */ \
            header.endian.masked = true; /* Client must mask data */\
            if (msg.len <= 125ULL) \
            { \
                header.endian.payloadlen = msg.len; \
            } \
            else if (msg.len <= 0xFFFFULL) \
            { \
                header.endian.payloadlen = 126; \
                extendedBtyes = 2; \
            } \
            else \
            { \
                header.endian.payloadlen = 127; \
                extendedBtyes = 8; \
            } \
        }

int WebSocketClientImplCurl::Send(Message msg)
{
    if (sendbufflen > sendoffset)
    {
        return SendRemaining();
    }

    if(GetState() != Connected)
        return -1;
    WsHeader header;
    size_t header_size = sizeof(WsHeader);
    memset(&header, 0, sizeof(WsHeader));

    uint32_t extendedBtyes = 0;
    if (HostIsBigEndian())
    {
        FILL_WS_HEADER(big)
    }
    else
    {
        FILL_WS_HEADER(little)
    }


    mask mask_key;
    int bufflen = sendbufflen = sizeof(WsHeader) + extendedBtyes + sizeof(mask_key) + msg.len;
    char* buff = sendbuff =(char*)malloc(bufflen);

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning(disable: 4302)
#endif
    mask_key.integer = (int32_t)buff+1;  // get a random integer
#ifdef _MSC_VER
#pragma warning( pop )
#endif
    if (!buff)
        throw "Not enough memory: data is too large.";
    char* temp = buff;
    // header
    memcpy(temp, &header, sizeof(header));
    temp += sizeof(header);

    // extended length
    switch (extendedBtyes)
    {
    case 0:
        break;
    case 2:
    {
        uint16_t size = net_to_host((uint16_t)msg.len);
        memcpy(temp, &size, sizeof(size));
        temp += sizeof(size);
        break;
    }
    case 8:
    {
        uint64_t size = net_to_host((uint64_t)msg.len);
        memcpy(temp, &size, sizeof(size));
        temp += sizeof(size);
        break;
    }
    }

    // masking key
    memcpy(temp, &mask_key, sizeof(mask_key));
    temp += sizeof(mask_key);

    // payload
    memcpy(temp, msg.data, msg.len);
    WsMask(temp, msg.len, mask_key.chararr);

    int n = send(this->m_sockfd, buff, bufflen, 0);

    if (n<0)
    {
        ClearSendBuff();
        return -1;
    }

    if (n < bufflen)
    {
        sendoffset += n;
        return sendbufflen - sendoffset;
    }

    ClearSendBuff();
    return 0;
}

int ws::WebSocketClientImplCurl::SendRemaining()
{
    if (sendbufflen > sendoffset)
    {
        int n = send(this->m_sockfd, this->sendbuff, sendoffset, sendbufflen - sendoffset);
        if (n < 0)
        {
            ClearSendBuff();
            return -1;
        }
        sendoffset += n;
        if (sendbufflen > sendoffset)
        {
            return sendbufflen - sendoffset;
        }
        else
        {
            ClearSendBuff();
        }
    }
    return 0;
}

void WebSocketClientImplCurl::OnRecv(Message msg, bool fin)
{

}

long WebSocketClientImplCurl::GetResponseCode()
{
    long response_code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &response_code);
    return response_code;
}

curl_socket_t WebSocketClientImplCurl::OpenSocketCallback(void * clientp, curlsocktype purpose, curl_sockaddr * address)
{
    curl_socket_t *sockfd = &((WebSocketClientImplCurl *)clientp)->m_sockfd;
    *sockfd = socket(address->family, address->socktype, address->protocol);
    return *sockfd;
}

size_t WebSocketClientImplCurl::OnHeaderReceived(char * buffer, size_t size, size_t nitems, void * userdata)
{
    size_t n = size * nitems;
    if (buffer[0] == 'H' && buffer[1] == 'T' && buffer[2] == 'T' && buffer[3] == 'P')
    {
        WebSocketClientImplCurl *pthis = (WebSocketClientImplCurl *)userdata;
        long code = pthis->GetResponseCode();

        if (code == 101)
        {
            pthis->SetState(Connected);
            pthis->OnConnect(Success);
        }
        else
        {
            pthis->SetState(Disconnected);
            pthis->OnConnect(Reject);
        }
    }
    return n;
}

#define CHECK_REMAINING(size) \
        if(remaining < size) \
        { \
            pthis->buffer = std::string(start, len); \
            return datalen; \
        }

#define MOVE_FORWARD(size) \
        tmp += size; \
        remaining -= size;

#define PARSE_WS_HEADER(endian) \
        payloadlen = header.endian.payloadlen; \
 \
        if (header.endian.payloadlen == 126) \
        { \
            CHECK_REMAINING(2) \
            u_short *len = (u_short *)tmp; \
            payloadlen = net_to_host(*len); /* net order to host order */\
            MOVE_FORWARD(2) \
        } \
        else if (header.endian.payloadlen == 127) \
        { \
            CHECK_REMAINING(8) \
            uint64_t *len = (uint64_t *)tmp; \
            payloadlen = net_to_host(*len); /* net order to host order */\
            MOVE_FORWARD(8) \
        }

#define UNMASK_PAYLOAD(endian) \
        if (header.endian.masked) \
        { \
            /* Get mask key */\
            CHECK_REMAINING(4) \
            mask mask_key; \
            memcpy(&mask_key, tmp, 4); \
            MOVE_FORWARD(4) \
 \
            /* Unmask data */\
            CHECK_REMAINING(payloadlen) \
            WsMask(tmp, payloadlen, mask_key.chararr); \
 \
        } \
        frame_type = (FrameType)header.endian.opcode; \
        fin = (bool)header.endian.fin;


size_t WebSocketClientImplCurl::OnMessageReceived(char * ptr, size_t size, size_t nmemb, void * userdata)
{
    WebSocketClientImplCurl *pthis = (WebSocketClientImplCurl *)userdata;
    size_t datalen = size * nmemb;
    pthis->buffer.append(ptr, datalen);

    char * tmp = &pthis->buffer[0];
    size_t remaining = pthis->buffer.size();

    while (remaining)
    {
        char *start = tmp;
        size_t len = remaining;

        CHECK_REMAINING(2)
        WsHeader header;
        memcpy(&header, tmp, 2);
        MOVE_FORWARD(2)

        uint64_t payloadlen = 0;
        FrameType frame_type = Text;
        bool fin = true;

        bool endian = HostIsBigEndian();
        if (endian)
        {
            PARSE_WS_HEADER(big)
        }
        else
        {
            PARSE_WS_HEADER(little)
        }

        if (endian)
        {
            UNMASK_PAYLOAD(big)
        }
        else
        {
            UNMASK_PAYLOAD(little)
        }

        CHECK_REMAINING(payloadlen)
        Message msg(frame_type, tmp, payloadlen);
        // TODO: parse the reserved bits
        pthis->OnRecv(msg, fin);
        MOVE_FORWARD(payloadlen)
    }
    pthis->buffer.clear();
    return datalen;
}

void WebSocketClientImplCurl::RecvProc(void * userdata)
{
    WebSocketClientImplCurl *pthis = (WebSocketClientImplCurl *)userdata;

    CURLcode ret = curl_easy_perform(pthis->m_curl);
}

void WebSocketClientImplCurl::ConnProc(WebSocketClientImplCurl* pthis)
{
    if (pthis->GetState() != Disconnected)
        return;
    pthis->SetState(Connecting);
    CURLcode ret = curl_easy_perform(pthis->m_curl);
    pthis->SetState(Disconnected);
    if (ret == CURLE_OK)
    {
    }
    else if (ret == CURLE_COULDNT_CONNECT || ret == CURLE_OPERATION_TIMEDOUT)
    {
        pthis->OnConnect(Timeout);
    }
    else
    {
        pthis->OnConnect(Reject);
    }
}


inline void WebSocketClientImplCurl::SetState(State newState)
{
    m_state = newState;
}

void ws::WebSocketClientImplCurl::ClearSendBuff()
{
    if (sendbuff)
    {
        free(sendbuff);
        sendbuff = nullptr;
        sendbufflen = 0;
        sendoffset = 0;
    }
}
