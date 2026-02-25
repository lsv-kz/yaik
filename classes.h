#ifndef CLASSES_H_
#define CLASSES_H_
#define _FILE_OFFSET_BITS 64

#include <iostream>
#include "bytes_array.h"
//======================================================================
enum HTTP_METHOD
{
    M_NULL, M_GET, M_HEAD, M_POST, M_OPTIONS, M_PUT,
    M_PATCH, M_DELETE, M_TRACE, M_CONNECT
};

struct Param
{
    std::string name;
    std::string val;
};

enum HTTP2_ERRORS
{
    NO_ERROR,
    PROTOCOL_ERROR,
    INTERNAL_ERROR,
    FLOW_CONTROL_ERROR,
    SETTINGS_TIMEOUT,
    STREAM_CLOSED,
    FRAME_SIZE_ERROR,
    REFUSED_STREAM,
    CANCEL,
    COMPRESSION_ERROR,
    CONNECT_ERROR,
    ENHANCE_YOUR_CALM,
    INADEQUATE_SECURITY,
    HTTP_1_1_REQUIRED,
};

enum FRAME_TYPE
{
    DATA,
    HEADERS,
    PRIORITY,
    RST_STREAM,
    SETTINGS,
    PUSH_PROMISE,
    PING,
    GOAWAY,
    WINDOW_UPDATE,
    CONTINUATION,
    ALTSVC,
    ORIGIN = 0x0C,
    PRIORITY_UPDATE = 0x10,
};

enum HTTP2_FLAGS
{
    FLAG_ACK = 0x1,
    FLAG_END_STREAM = 0x1,
    FLAG_END_HEADERS = 0x4,
    FLAG_PADDED = 0x8,
    FLAG_PRIORITY = 0x20
};

enum SOURCE_DATA
{
    NO_SOURCE,
    DIRECTORY,
    FROM_FILE,
    MULTIPART_DATA,
    FROM_DATA_BUFFER,
    DYN_PAGE,
};

enum CGI_TYPE { CGI, PHPCGI, PHPFPM, FASTCGI, SCGI, };

enum CGI_STATUS
{
    NO_CGI,
    CGI_CREATE,
    FASTCGI_BEGIN,
    FASTCGI_PARAMS,
    SCGI_PARAMS,
    CGI_STDIN,
    CGI_STDOUT,
};

struct VHost
{
    VHost *next;

    std::string hostname;
    std::string DocumentRoot;

    SSL_CTX* ctx;

    std::string Certificate;
    std::string CertificateKey;

    VHost()
    {
        next = NULL;
        ctx = NULL;
    }
};

struct Server
{
    Server *next;
    std::string ip;
    std::string port;
    int sock;
    bool SecureConnect;
    bool EnableHTTP2;
    SSL_CTX *ctx;

    VHost *vhosts;

    Server()
    {
        next = NULL;
        SecureConnect = EnableHTTP2 = false;
        ctx = NULL;
        vhosts = NULL;
    }
};

extern const char *static_tab[][2];

void print_err(const char *format, ...);
void dec_all_cgi();
//======================================================================
typedef struct fcgi_list_addr
{
    std::string script_name;
    std::string addr;
    CGI_TYPE type;
    struct fcgi_list_addr *next;
} fcgi_list_addr;
//----------------------------------------------------------------------
class Config
{
    Config(const Config&){}
    Config& operator=(const Config&);

public:

    bool PrintDebugMsg;

    std::string ServerSoftware;

    std::string DocumentRoot;
    std::string ScriptPath;
    std::string LogPath;
    std::string PidFilePath;

    std::string UsePHP;
    std::string PathPHP;

    int MaxConcurrentStreams;
    int HeaderTableSize;

    int ListenBacklog;
    bool TcpNoDelay;

    int HTTP1_DataBufSize;
    int HTTP2_DataBufSize;

    int MaxAcceptConnections;

    int MaxRequestsPerClient;

    int MaxCgiProc;

    int Timeout;
    int TimeoutKeepAlive;
    int TimeoutPoll;
    int TimeoutCGI;

    long int ClientMaxBodySize;

    bool ShowMediaFiles;

    std::string user;
    std::string group;

    uid_t server_uid;
    gid_t server_gid;

    Server *all_servers;
    int num_servers;

    fcgi_list_addr *fcgi_list;
    //------------------------------------------------------------------
    Config()
    {
        fcgi_list = NULL;
        num_servers = 0;
        all_servers = NULL;
    }

    ~Config()
    {
        free_fcgi_list();
        free_servers();
    }

    void free_fcgi_list()
    {
        fcgi_list_addr *t;
        while (fcgi_list)
        {
            t = fcgi_list;
            fcgi_list = fcgi_list->next;
            if (t)
                delete t;
        }
    }

    void free_servers()
    {
        Server *serv = all_servers;
        for ( ; serv; serv = all_servers)
        {
            VHost *h = serv->vhosts;
            for ( ; h; h = serv->vhosts)
            {
                serv->vhosts = h->next;
                delete h;
            }

            all_servers = serv->next;
            delete serv;
        }
    }

    Server *create_server()
    {
        Server *serv;
        try
        {
            serv = new Server;
        }
        catch (...)
        {
            fprintf(stderr, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
            exit(errno);
        }

        num_servers++;
        return serv;
    }
};
//======================================================================
extern const Config* const conf;
//======================================================================
struct Stream
{
    Stream *prev;
    Stream *next;

    unsigned long numConn;
    unsigned long numReq;

    VHost *vhost;

    int id;
    FRAME_TYPE type;
    int flags;
    time_t Time;

    HTTP_METHOD httpMethod;

    std::string path;
    std::string decode_path;

    std::string query_string;
    std::string decode_query_string;

    char *clean_decode_path;
    int clean_decode_path_size;

    std::string host;
    std::string user_agent;
    std::string referer;
    std::string range;
    std::string sReqContentType;
    std::string sReqContentLen;

    ByteArray buf;
    ByteArray headers;
    ByteArray send_data;
    ByteArray post_data;
    ByteArray frame_win_update;

    int resp_status;
    const char *resp_content_type;
    long long resp_content_len;
    long long post_content_len;
    long long file_size;
    long long offset;
    long stream_window_size;

    SOURCE_DATA source_data;
    int fd;

    long long send_bytes;

    bool rst_stream;
    bool create_headers;
    bool send_headers;
    bool send_end_stream;

    CGI_TYPE cgi_type;
    CGI_STATUS cgi_status;

    struct
    {
        bool start;
        bool end;

        time_t timer;

        pid_t pid;
        std::string path;
        int to_script;
        int from_script;

        const std::string *socket;
        int fd;
        int i_param;
        int size_par;
        std::vector <Param> vPar;
        ByteArray buf_param;
        int fcgi_type;
        int fcgiContentLen;
        int fcgiPaddingLen;
        //-----------------------
        long window_update;
        long windows_size;
    } cgi;

    Stream()
    {
        numConn = numReq = 0;
        vhost = NULL;
        init();
        id = 0;
        Time = time(NULL);
        clean_decode_path = NULL;
        clean_decode_path_size = 0;
        stream_window_size = 0;
        rst_stream = create_headers = send_headers = send_end_stream = false;
        cgi.window_update = 0;
        cgi.windows_size = 0;
    }

    ~Stream()
    {
        if (conf->PrintDebugMsg)
        {
            if ((send_bytes != file_size) && (source_data == FROM_FILE))
                print_err("<%s:%d> !!! ~Resp(%s), send_bytes=%lld(%lld), stream_window_size=%ld, id=%d \n",
                        __func__, __LINE__, clean_decode_path, send_bytes, file_size, stream_window_size, id);
        }

        if (clean_decode_path)
            delete [] clean_decode_path;

        if (fd > 0)
        {
            close(fd);
            fd = -1;
        }

        if (cgi.fd > 0)
        {
            close(cgi.fd);
            cgi.fd = 1;
        }

        if (cgi.to_script > 0)
        {
            close(cgi.to_script);
            cgi.to_script = -1;
        }

        if (cgi.from_script > 0)
        {
            close(cgi.from_script);
            cgi.from_script = -1;
        }

        if (cgi.start)
            dec_all_cgi();
    }

    void init()
    {
        ++numReq;
        path.clear();
        decode_path.clear();
        decode_query_string.clear();
        host.clear();
        user_agent.clear();
        referer.clear();
        range.clear();

        buf.init();
        send_data.init();
        post_data.init();
        headers.init();

        httpMethod = M_NULL;
        sReqContentType.clear();
        sReqContentLen.clear();
        post_content_len = 0;

        Time = 0;

        send_headers = false;
        create_headers = false;
        resp_status = 0;

        source_data = NO_SOURCE;
        fd = -1;
        file_size = 0;
        offset = 0;
        send_bytes = 0;
        resp_content_len = -1;
        resp_content_type = NULL;

        cgi_status = NO_CGI;
        cgi.timer = 0;
        cgi.start = cgi.end = false;
        cgi.socket = NULL;
        cgi.fcgiContentLen = 0;
        cgi.fcgiPaddingLen = 0;
        cgi.fd = cgi.to_script = cgi.from_script = -1;
        cgi.fcgi_type = 0;
        cgi.fcgiContentLen = cgi.fcgiPaddingLen = 0;
    }

private:
    Stream(const Stream&);
    Stream& operator=(const Stream&);
};
//======================================================================
struct Header
{
    char *name;
    char *val;
    int size;
};
//----------------------------------------------------------------------
class DynamicTable
{
    Header *table;
    
    int max_table_size;
    int table_size;

    int max_headers_num;
    int headers_num;
    
    int offset;
    int err;

    DynamicTable();
    DynamicTable(const DynamicTable&);
    DynamicTable& operator= (const DynamicTable&);

public:

    DynamicTable(int size_, int offs)
    {
        max_table_size = size_;
        max_headers_num = max_table_size/20;
        table_size = 0;
        offset = offs;
        headers_num = err = 0;
        table = new(std::nothrow) Header [max_table_size];
        if (!table)
        {
            fprintf(stderr, "<%s:%d> Error: %s\n", __func__, __LINE__, strerror(errno));
            max_table_size = 0;
            err = 1;
            return;
        }
        fprintf(stderr, "<%s:%d> table_size=%d, max_headers_num=%d, offset=%d\n", __func__, __LINE__, max_table_size, max_headers_num, offset);
        table[0].name = NULL;
        table[0].val = NULL;
    }
    //------------------------------------------------------------------
    ~DynamicTable()
    {
        if (table)
        {
            //fprintf(stderr, "<%s:%d> ~~~ Delete Dynamic Table\n", __func__, __LINE__);
            for ( int i = 0; i < headers_num; ++i)
            {
                if (table[i].name && table[i].val)
                {
                    delete [] table[i].name;
                }
            }
            delete [] table;
            table = NULL;
        }
    }
    //------------------------------------------------------------------
    int add(std::string& name, std::string& val);
    //------------------------------------------------------------------
    void print()
    {
        fprintf(stderr, " -------- Dynamic table %d, size %d --------\n", headers_num, table_size);
        for ( int i = 0; i < headers_num; ++i)
        {
            fprintf(stderr, " %04d  [%s: %s]\n", i + offset, table[i].name, table[i].val);
        }
    }
    //------------------------------------------------------------------
    Header *get(int n)
    {
        if ((n < offset) || (n >= (headers_num + offset)))
        {
            fprintf(stderr, "<%s:%d> Error out of range: index=%d, table_len=%d, offset=%d\n", __func__, __LINE__, n, headers_num, offset);
            return NULL;
        }

        return &table[n - offset];
    }
};

#endif
