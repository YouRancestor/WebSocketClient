#include "WebSocketClientImplCurl.h"
#include <string.h>
#include <thread>
#include <cstdlib>
#include <ctime>
using namespace ws;

struct WsHeaderLittleEdian
{
    uint8_t opcode : 4;
    uint8_t rsv3 : 1;
    uint8_t rsv2 : 1;
    uint8_t rsv1 : 1;
    uint8_t fin : 1;

    uint8_t payloadlen : 7;
    uint8_t masked : 1;
};
struct WsHeaderBigEdian
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
    WsHeaderLittleEdian little;
    WsHeaderBigEdian big;
};

union Endian
{
    uint16_t s;
    char c[sizeof(uint16_t)];
};
static Endian un = { 0x0102 };
static bool HostIsBigEdian() {
    if (un.c[0] == 1)
        return true;
    else
        return false;
}

// transform integer's byte-order from net to host or from host to net
template <class T>
static T net_to_host(T num)
{
    if (HostIsBigEdian())
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


static const char *defaultHeaders[] = {
    "HTTP/1.1 101 WebSocket Protocol Handshake",
    "Upgrade: WebSocket",
    "Connection: Upgrade",
    "Sec-WebSocket-Version: 13",
    "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw=="
};

WebSocketClientImplCurl::WebSocketClientImplCurl()
    : WebSocketClientImplCurl(defaultHeaders, sizeof(defaultHeaders) / sizeof(char *))
{

}

WebSocketClientImplCurl::WebSocketClientImplCurl(const char ** customHeader, int nlines)
    : m_curl(NULL)
    , m_header_list_ptr(NULL)
    , m_sockfd(0)
    , m_state(WebSocketClientImplCurl::Disconnected)
{
    // Init random seed for masking key
    std::srand(std::time(nullptr));
    std::rand();
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

void WebSocketClientImplCurl::Connect(const char * url, long timeout)
{
    curl_easy_setopt(m_curl, CURLOPT_URL, url);
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, timeout);
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

void WebSocketClientImplCurl::OnClose()
{
}


inline WebSocketClientImplCurl::State WebSocketClientImplCurl::GetState()
{
    return m_state;
}

int WebSocketClientImplCurl::Send(Message msg)
{
    if(GetState() != Connected)
        return -1;
    WsHeader header;
    size_t header_size = sizeof(WsHeader);
    memset(&header, 0, sizeof(WsHeader));

    uint32_t extendedBtyes = 0;
    if (HostIsBigEdian())
    {
        header.big.fin = true;
        header.big.opcode = msg.type;
        if (!(header.big.opcode & 0x80))
        {
            // Non-control frame
            header.big.masked = true; // Client must mask data
            if (msg.len <= 125ULL)
            {
                header.big.payloadlen = msg.len;
            }
            else if (msg.len <= 0xFFFFULL)
            {
                header.big.payloadlen = 126;
                extendedBtyes = 2;
            }
            else
            {
                header.big.payloadlen = 127;
                extendedBtyes = 8;
            }
        }
    }
    else
    {
        header.little.fin = true;
        header.little.opcode = msg.type;
        if (!(header.little.opcode & 0x80))
        {
            // Non-control frame
            header.little.masked = true; // Client must mask data
            if (msg.len <= 125ULL)
            {
                header.little.payloadlen = msg.len;
            }
            else if (msg.len <= 0xFFFFULL)
            {
                header.little.payloadlen = 126;
                extendedBtyes = 2;
            }
            else
            {
                header.little.payloadlen = 127;
                extendedBtyes = 8;
            }
        }

    }


    mask mask_key;
    uint64_t bufflen = sizeof(WsHeader) + extendedBtyes + sizeof(mask_key) + msg.len;
    char* buff = (char*)malloc(bufflen);
#pragma warning(disable: 4302)
    mask_key.integer = (int32_t)buff+1;  // get a random integer
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

    free(buff);
    return n;
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


size_t WebSocketClientImplCurl::OnMessageReceived(char * ptr, size_t size, size_t nmemb, void * userdata)
{
    WebSocketClientImplCurl *pthis = (WebSocketClientImplCurl *)userdata;
    size_t datalen = size * nmemb;

    char * tmp = ptr;

    WsHeader header;
    memcpy(&header, ptr, 2);
    tmp += 2;
    uint64_t payloadlen = 0;
    FrameType frame_type = Text;
    bool fin = true;

    if (HostIsBigEdian())
    {
        payloadlen = header.big.payloadlen;

        if (header.big.payloadlen == 126)
        {
            u_short *len = (u_short *)tmp;
            payloadlen = net_to_host(*len); // net order to host order
            tmp += 2;
        }
        else if (header.big.payloadlen == 127)
        {
            uint64_t *len = (uint64_t *)tmp;
            payloadlen = net_to_host(*len); // net order to host order
            tmp += 8;
        }

        if (header.big.masked)
        {
            // Get mask key
            mask mask_key;
            memcpy(&mask_key, tmp, 4);
            tmp += 4;

            // Unmask data
            WsMask(tmp, payloadlen, mask_key.chararr);

        }
        frame_type = (FrameType)header.big.opcode;
        fin = (bool)header.big.fin;
    }
    else
    {
        payloadlen = header.little.payloadlen;

        if (header.little.payloadlen == 126)
        {
            u_short *len = (u_short *)tmp;
            payloadlen = net_to_host(*len); // net order to host order
            tmp += 2;
        }
        else if (header.little.payloadlen == 127)
        {
            uint64_t *len = (uint64_t *)tmp;
            payloadlen = net_to_host(*len); // net order to host order
            tmp += 8;
        }

        if (header.little.masked)
        {
            // Get mask key
            mask mask_key;
            memcpy(&mask_key, tmp, 4);
            tmp += 4;

            // Unmask data
            WsMask(tmp, payloadlen, mask_key.chararr);

        }
        frame_type = (FrameType)header.little.opcode;
        fin = (bool)header.little.fin;
    }
    
    Message msg(frame_type, tmp, payloadlen);
    pthis->OnRecv(msg, fin);
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
    if (ret == CURLE_COULDNT_CONNECT)
    {
        pthis->OnConnect(Timeout);
    }
    else
    {
        pthis->OnClose();
    }
}


inline void WebSocketClientImplCurl::SetState(State newState)
{
    m_state = newState;
}
