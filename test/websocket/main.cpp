
#include <curl/curl.h>
#include <assert.h>
#include <stdint.h>
#include <random>
#include <thread>
static const char* msg = "{\"type\":\"connect_device\",\"session_id\":\"unknown\",\"user_id\":\"admin\"}\n";
union mask
{
char chararr[4];
int32_t integer;
};
static mask mask_key = {0xff,0xf0,0x0f, 0x00};
static char* p = NULL;
static size_t payloadlen = 0;
static size_t len = 0;
static char* preparebuffer(size_t *plen)
{
    if (!p)
    {
        srand(mask_key.integer);
        mask_key.integer = rand();
        payloadlen = strlen(msg);
        // TODO: websocket包头根据载荷长度会有所不同
        len = payloadlen + 2 + 4;
        p = (char *)malloc(len);
        char *tmp = p;
        *tmp++ = 0x81;
        *tmp++ = (char)payloadlen | 0x80;
        memcpy(tmp, mask_key.chararr, 4);
        tmp += 4;
        for (int i = 0; i < payloadlen; ++i)
        {
            int j = i % 4;
            tmp[i] = msg[i] ^ mask_key.chararr[j];
        }
    }
    *plen = len;
    return p;
}
static void freebuffer()
{
    if (p)
    {
        free(p);
    }
}
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t n = size*nmemb;
    char flag = *ptr;
    ptr += 1;
    static bool firsttime = true;
    if (firsttime)
    {
        size_t len;
        char * p = preparebuffer(&len);
        curl_socket_t socketfd = *(curl_socket_t*)userdata;
        send(socketfd, p, len, 0);
        firsttime = false;
    }
    if ( !(flag & 0x80) )
    {
        // 还有后续数据包
        return n;
    }
    
    switch (flag & 0x0f)
    {
    case 0x0:   // 附加数据帧
        break;
    case 0x1:   // 文本数据帧
        printf("%s\n",ptr+1);
        break;
    case 0x2:   // 二进制数据帧
        break;
    //case 0x3 ... 0x7:   // 非控制帧保留
    //    break;
    case 0x8:   // 关闭连接
        break;
    case 0x9:   // ping
        break;
    case 0xA:   // pong
        break;
    //case 0xB ... 0xF:   // 控制帧保留
    //    break;
    default:
        break;
    }

    return n;
}

static void getcodeproc(CURL* handle, bool* stop)
{
    long response_code = 0;
    while (!*stop)
    {
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 0)
        {
            printf("response_code: %03ld\n", response_code);
            break;
        }
    }
}

static curl_socket_t opensocket_callback(void *clientp,
    curlsocktype purpose,
    struct curl_sockaddr *address)
{
    curl_socket_t *sockfd = (curl_socket_t *)clientp;
    *sockfd = socket(address->family, address->socktype, address->protocol);
    return *sockfd;
}

static size_t on_header_received(char *buffer, size_t size, size_t nitems, void *userdata)
{
    size_t n = size * nitems;
    if (buffer[0] != 'H' || buffer[1] != 'T' || buffer[2] != 'T' || buffer[3] != 'P')
        return n;
    long response_code = 0;
    CURL* handle = (CURL*)userdata;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
    printf("response_code in header callback: %03ld\n", response_code);
    return n;
}

static const char *defaultHeaders[5] = {
    "HTTP/1.1 101 WebSocket Protocol Handshake",
    "Upgrade: WebSocket",
    "Connection: Upgrade",
    "Sec-WebSocket-Version: 13",
    "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw=="
};

struct WsHeader
{
    uint8_t opcode : 4;
    uint8_t rsv3 : 1;
    uint8_t rsv2 : 1;
    uint8_t rsv1 : 1;
    uint8_t fin : 1;

    uint8_t payloadlen : 7;
    uint8_t masked : 1;
};

int main(int argc, char *argv[])
{
    WsHeader h;
    uint8_t init[2] = { 0x12, 0x48 };
    printf("%d\n",sizeof(h));
    memcpy(&h, &init, sizeof(h));

    printf("%x, %x, %x, %x, "
        "%x, %x, %x\n", 
        h.fin, h.rsv1, h.rsv2, h.rsv3, h.opcode, 
        h.masked, h.payloadlen);
    system("pause");
    return 0;

    size_t buflen;
    char* buf = preparebuffer(&buflen);

    curl_socket_t sockfd;
    CURL* handle = curl_easy_init();
    // Add headers
    curl_slist* header_list_ptr = curl_slist_append(NULL, defaultHeaders[0]);
    for (size_t i = 1; i < sizeof(defaultHeaders) / sizeof(char*); ++i)
    {
        header_list_ptr = curl_slist_append(header_list_ptr, defaultHeaders[i]);
    }
    curl_easy_setopt(handle, CURLOPT_URL, "http://192.168.1.44:8888/ws");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header_list_ptr);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, on_header_received);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, handle);
    curl_easy_setopt(handle, CURLOPT_OPENSOCKETFUNCTION, opensocket_callback);
    curl_easy_setopt(handle, CURLOPT_OPENSOCKETDATA, &sockfd);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &sockfd);
    //curl_easy_setopt(handle, CURLOPT_CONNECT_ONLY, 0L);
    //bool stop = false;
    //std::thread th1(getcodeproc, handle, &stop);
    CURLcode ret = curl_easy_perform(handle);
    //stop = true;
    //th1.join();
    printf("return val: %d.\n", ret);

    system("pause");
    freebuffer();
    curl_slist_free_all(header_list_ptr);
    curl_easy_cleanup(handle);
    return 0;
}