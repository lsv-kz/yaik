#ifndef SERVER_H_
#define SERVER_H_
#define _FILE_OFFSET_BITS 64

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <climits>
#include <iomanip>
#include <vector>

#include <mutex>
#include <thread>
#include <condition_variable>

#include <errno.h>
#include <signal.h>
#include <stdarg.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <pthread.h>
#include <sys/resource.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "string__.h"
#include "classes.h"

const int  MAX_URI = 2048;
const int  ERR_TRY_AGAIN = -1000;
const int  MAX_STREAM = 128;

const char proto_alpn_h2[] = { 2, 'h', '2', 8, 'h', 't', 't', 'p', '/', '1', '.', '1' };
const char proto_alpn[] = { 8, 'h', 't', 't', 'p', '/', '1', '.', '1', 2, 'h', '2' };

const int hpack_mask = 0x40;

enum
{
    RS101 = 101,
    RS200 = 200, RS204 = 204, RS206 = 206,
    RS301 = 301, RS302,
    RS400 = 400, RS401, RS402, RS403, RS404, RS405, RS406, RS407,
    RS408, RS411 = 411, RS413 = 413, RS414, RS415, RS416, RS417, RS418, RS429 = 429, RS431 = 431,
    RS500 = 500, RS501, RS502, RS503, RS504, RS505
};

enum PROTOCOL { P_HTTP1, P_HTTP2};
//----------------------------------------------------------------------
#define FCGI_KEEP_CONN  1
#define FCGI_RESPONDER  1

#define FCGI_VERSION_1           1
#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE            (FCGI_UNKNOWN_TYPE)
#define requestId               1
//======================================================================
extern char **environ;
extern const Config* const conf;
//======================================================================
struct FrameRedySend
{
    FrameRedySend *prev;
    FrameRedySend *next;
    
    int id;
    ByteArray frame;
};
//======================================================================
enum CHUNK_MODE { NO_CHUNK, CHUNK, CHUNK_END };
//======================================================================
struct http2
{
    enum HTTP2_STATUS
    {
        SSL_ACCEPT = 1,
        PREFACE_MESSAGE,
        SET_SETTINGS,
        PROCESSING_REQUESTS,
        SSL_SHUTDOWN,
    } con_status;

    Stream *start_stream;
    Stream *end_stream;

    Stream *work_stream;

    FrameRedySend *start_list_send_frame;
    FrameRedySend *end_list_send_frame;
    //-----------------------
    int body_len;
    FRAME_TYPE type;
    int flags;
    int id;

    long init_window_size;
    long connect_window_size;
    long max_frame_size;
    long cgi_window_update;
    long cgi_window_size;

    char header[9];
    int header_len;

    ByteArray body;
    ByteArray goaway;
    ByteArray ping;
    ByteArray settings;
    ByteArray frame_win_update;

    DynamicTable *dyn_tab;

    bool recv_settings;
    bool recv_settings_ack;
    bool send_settings_ack;

    bool try_again;
    //----------------------
    void init()
    {
        header_len = 0;
        body.init();
    }

    http2();
    ~http2();
    Stream *add(unsigned long, unsigned long);
    void del_from_list(Stream *r);
    int close_stream(int id);
    int set_window_size(unsigned long num_conn, int id, long n);
    Stream *get(int id);
    Stream *get();
    int size();
    int get_str(std::string& s, int *len);
    int get_header(int ind, std::string& name, std::string& val, int *len);
    int parse(Stream *r);
    void push_to_list(FrameRedySend*);
    void del_from_list(FrameRedySend*);
    //------------------------------------------------------------------
    const char *get_str_status()
    {
        switch (con_status)
        {
            case http2::SSL_ACCEPT:
                return "SSL_ACCEPT";
            case http2::PREFACE_MESSAGE:
                return "PREFACE_MESSAGE";
            case http2::SET_SETTINGS:
                return "SET_SETTINGS";
            case http2::PROCESSING_REQUESTS:
                return "PROCESSING_REQUESTS";
            case http2::SSL_SHUTDOWN:
                return "SSL_SHUTDOWN";
        }

        return "?";
    }
    //------------------------------------------------------------------

private:

    int max_streams;    //SETTINGS_MAX_CONCURRENT_STREAMS (0x3)
    int num_streams;
    int err;

    http2(const http2&);
    http2& operator=(const http2&);
};
//----------------------------------------------------------------------
struct http1
{
    enum HTTP1_STATUS {
        SSL_ACCEPT = 1,
        SSL_SHUTDOWN,
        READ_REQUEST,
        READ_POSTDATA,
        SEND_RESP_HEADERS,
        SEND_ENTITY,
    } con_status;

    Stream resp;
    String hdrs;
    CHUNK_MODE chunk_mode;
    bool connKeepAlive;
    bool try_again;
    //------------------------------------------------------------------
    http1()
    {
        connKeepAlive = true;
        hdrs.clear();
        chunk_mode = NO_CHUNK;
        try_again = false;
    }
    //------------------------------------------------------------------
    ~http1()
    {
        
    }
    //------------------------------------------------------------------
    void init()
    {
        resp.init();
        //----------------------
        hdrs.clear();
        chunk_mode = NO_CHUNK;
    }
    //------------------------------------------------------------------
    const char *get_str_status()
    {
        switch (con_status)
        {
            case SSL_ACCEPT:
                return "SSL_ACCEPT";
            case READ_REQUEST:
                return "READ_REQUEST";
            case READ_POSTDATA:
                return "READ_POSTDATA";
            case SEND_RESP_HEADERS:
                return "SEND_RESP_HEADERS";
            case SEND_ENTITY:
                return "SEND_ENTITY";
            case SSL_SHUTDOWN:
                return "SSL_SHUTDOWN";
        }
    
        return "?";
    }
};
//======================================================================
class Connect
{
    Connect(const Connect&){}
    Connect& operator=(const Connect&);
public:
    Connect()
    {
        numReq = 1;
        err = 0;
        fd_revents = 0;
        tls.ssl = NULL;
        tls.err = 0;
        tls.poll_events = 0;
        h1 = NULL;
        h2 = NULL;
    }

    ~Connect()
    {
        if (h1)
            delete h1;
        if (h2)
            delete h2;
    }

    Connect *prev;
    Connect *next;

    static int serverSocket;

    unsigned long numConn;
    unsigned long numReq;

    char remoteAddr[NI_MAXHOST];
    char remotePort[NI_MAXSERV];

    PROTOCOL Protocol;

    int err;
    int  clientSocket;
    time_t client_timer;
    int fd_revents;

    struct
    {
        SSL *ssl;
        int err;
        int poll_events;
        time_t shutdown_timer;
    } tls;

    http1 *h1;
    http2 *h2;
};
//======================================================================
class EventHandlerClass
{
    std::mutex mtx_thr, mtx_cgi;
    std::condition_variable cond_thr;

    int num_poll, all_cgi;
    int close_thr;
    unsigned long num_request;

    Connect **conn_array;
    struct pollfd *poll_fd;
    Stream **cgi_array;

    Connect *work_list_start;
    Connect *work_list_end;

    Connect *wait_list_start;
    Connect *wait_list_end;

    void del_from_list(Connect *c);
    void worker(Connect *c);

    int http1_worker(Connect *con, int revents);

    int http2_connection(Connect *c);

    int recv_frame(Connect *c);
    int recv_frame_(Connect *c);
    int parse_frame(Connect *c);

    void send_frames(Connect *c);
    int send_frames_(Connect *c);

    int send_frame_settings(Connect *c);
    int send_frame_headers(Connect *c, Stream *r);
    int send_frame_data(Connect *c, Stream *r);
    int send_window_update(Connect *c);
    int send_window_update(Connect *c, Stream *r);
    int send_frame_goawey(Connect *c);
    int send_frame_ping(Connect *c);
    int send_frame_rststream(Connect *c);

    void http1_set_poll(Connect *c);
    void http2_set_poll(Connect *c);
    int http1_poll(Connect *c, int);
    int http2_poll(Connect *c, int);

    void cgi_worker(Connect *c, Stream *r, int i);
    int cgi_create_proc(Connect *c, Stream *r);
    int cgi_fork(Connect *c, Stream *r, int* serv_cgi, int* cgi_serv);
    int cgi_stdin(Stream *r, int fd);
    int cgi_stdout(Stream *r, int fd);
    void cgi_headers_parse(Connect *c);

    void cgi_worker(Connect *c, int i);
    int cgi_stdout(Connect *c, int fd);

    int scgi_worker(Connect *c, Stream *r, int i);

    void fcgi_worker(Connect *c, Stream *r, int i);
    void fcgi_get_headers(Connect *c, Stream *r);

    void fcgi_worker(Connect *c, int i);
    void fcgi_get_headers(Connect *c);

    void http1_end_request(Connect *c);

    int http1_cgi_set(Connect *c);
    void http1_cgi_poll(Connect *c, int);
    int http2_cgi_set(Connect *c);
    void http2_cgi_poll(Connect *c, int);

    void ssl_shutdown(Connect *c);
    void close_connect(Connect *c);

public:

    EventHandlerClass();
    ~EventHandlerClass();

    void init();

    int wait_connection();
    void add_work_list();
    int cgi_poll();
    void set_poll();
    int _poll();

    void dec_all_cgi();
    void push_wait_list(Connect *c);
    void close_event_handler();
    void close_connections();
};
//--------------------- accept_connect.cpp -----------------------------
void decrement_num_conn();
void accept_connect(int serverSocket);
//------------------------ socket.cpp-----------------------------------
int create_server_socket(const Config *c);
int create_fcgi_socket(const char *);
int write_to_client(Connect *c, const char *buf, int len, int id);
int read_from_client(Connect *c, char *buf, int len);
int peek(Connect *c, char *buf, int len);
int socket_read_line(Connect *c);
int write_to_fcgi(int fd, const char *buf, int len);
int get_size_sock_buf(int domain, int optname, int type, int protocol);
//-------------------------- http1.cpp ---------------------------------
int create_response_headers(Connect *c);
int read_post_data(Connect *c);
//------------------------ http1_cgi.cpp -------------------------------
int cgi_set_size_chunk(ByteArray *ba);
//------------------------ http2_cgi.cpp -------------------------------
void kill_chld(pid_t pid);
int is_cgi(Stream *resp);
//------------------------ http2_fcgi.cpp ------------------------------
void fcgi_set_header(ByteArray* ba, unsigned char type);
void fcgi_set_header(char *s, unsigned char type, int dataLen);
int fcgi_create_connect(Connect *c, Stream *r);
//------------------------ scgi.cpp-------------------------------
int scgi_create_connect(Connect *c, Stream *r);
//-------------------------- config.cpp --------------------------------
int read_conf_file(const char *path_conf);
int set_max_fd(int max_open_fd);
void free_fcgi_list();
void setDataBufSize(int n);
//------------------------- index_dir.cpp ------------------------------
int index_dir(Connect *c, std::string& path, const char *uri, ByteArray *b);
//----------------------- percent_coding.cpp----------------------------
int encode(const char *s_in, char *s_out, int len_out);
int decode(const char *s_in, int len_in, std::string& s_out);
//------------------------- function.cpp -------------------------------
std::string get_time();
std::string get_time(time_t t);
std::string log_time();
std::string log_time(time_t t);

const char *strstr_case(const char * s1, const char *s2);
int strlcmp_case(const char *s1, const char *s2, int len);
int strcmp_case(const char *s1, const char *s2);
int pow_(int x, int y);
int bytes_to_int(unsigned char prefix, int pref_len, const char *s, int *len, int size);
int int_to_bytes(ByteArray& buf, int data, int pref_len, int mask);

HTTP_METHOD get_int_method(const char *s);
const char *get_str_method(int i);
const char *get_str_frame_type(FRAME_TYPE);
const char *get_cgi_type(CGI_TYPE n);
const char *get_cgi_status(CGI_STATUS s);
const char *get_http2_error(int err);

int clean_path(char *path, int len);
const char *content_type(const char *s);
long long file_size(const char *s);

void set_frame(Stream *resp, char *s, int len, int type, HTTP2_FLAGS flags, int id);
void set_frame_headers(Stream *r);
void set_frame_data(Stream *resp, int len, int flag);
int set_frame_data(Connect *con, Stream *r);
void add_header(Stream *r, int ind);
void add_header(Stream *r, int ind, const char *val);
void set_frame_window_update(Connect *c, int len);
void set_frame_window_update(Stream *r, int len);
void set_frame_goaway(Connect *c, HTTP2_ERRORS error);
int set_rst_stream(Connect *c, int id, HTTP2_ERRORS error);
int set_response(Connect *c, Stream *r);
SOURCE_DATA get_content_type(const char *path);
int parse_range(const char *s, long long file_size, long long *offset, long long *content_length);

void resp_204(Stream *resp);
void resp_400(Stream *resp);
void resp_403(Stream *resp);
void resp_404(Stream *resp);
void resp_411(Stream *resp);
void resp_413(Stream *resp);
void resp_414(Stream *resp);
void resp_431(Stream *resp);
void resp_500(Stream *resp);
void resp_502(Stream *resp);
void resp_504(Stream *resp);
void hex_print_stderr(const char *s, int line, const void *p, int n);
const char *http2_status_resonse(int st);
//--------------------------- log.cpp ----------------------------------
void create_logfiles(const std::string &);
void close_logs();
void print_err(const char *format, ...);
void print_err(Connect *c, const char *format, ...);
void print_err(Stream *r, const char *format, ...);
void print_log(Connect *c, Stream *r);
void print_log(Connect *c);
//----------------------- huffman_code.cpp -----------------------------
void huffman_encode(const char *s, ByteArray& out);
int huffman_decode(const char *s, int len, std::string& s_out);
//----------------------- event_handler.cpp ----------------------------
void push_wait_list(Connect *c);
void event_handler();
void close_work_thread();
//---------------------------- ssl.cpp ---------------------------------
void init_openssl();
void cleanup_openssl();
SSL_CTX *create_context();
int configure_context(SSL_CTX *ctx);
SSL_CTX *Init_SSL();
const char *ssl_strerror(int err);
int ssl_read(Connect *c, char *buf, int len);
int ssl_peek(Connect *c, char *buf, int len);
int ssl_write(Connect *c, const char *buf, int len, int id);
int ssl_accept(Connect *c);
//----------------------------------------------------------------------


#endif
