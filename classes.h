#ifndef CLASSES_H_
#define CLASSES_H_
#define _FILE_OFFSET_BITS 64

#include <iostream>
#include "bytes_array.h"
//======================================================================
enum HTTP_METHOD {
    M_NULL, M_GET, M_HEAD, M_POST, M_OPTIONS, M_PUT,
    M_PATCH, M_DELETE, M_TRACE, M_CONNECT
};

struct Header
{
    char *name;
    char *val;
};

struct Param
{
    std::string name;
    std::string val;
};

enum HTTP2_ERRORS
{
    NO_ERROR, PROTOCOL_ERROR, INTERNAL_ERROR, FLOW_CONTROL_ERROR, SETTINGS_TIMEOUT, STREAM_CLOSED, FRAME_SIZE_ERROR, 
    REFUSED_STREAM, CANCEL, COMPRESSION_ERROR, CONNECT_ERROR, ENHANCE_YOUR_CALM, INADEQUATE_SECURITY, HTTP_1_1_REQUIRED, 
};

enum FRAME_TYPE
{
    DATA, HEADERS, PRIORITY, RST_STREAM, SETTINGS, PUSH_PROMISE, PING,
    GOAWAY, WINDOW_UPDATE, CONTINUATION, ALTSVC, ORIGIN = 0x0C, PRIORITY_UPDATE = 0x10,
};

enum HTTP2_FLAGS { FLAG_ACK = 0x1, FLAG_END_STREAM = 0x1, FLAG_END_HEADERS = 0x4, FLAG_PADDED = 0x8, FLAG_PRIORITY = 0x20 };

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

enum CGI_STATUS { NO_CGI,  CGI_CREATE, FASTCGI_BEGIN, FASTCGI_PARAMS, SCGI_PARAMS, CGI_STDIN, CGI_STDOUT, };

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

    SSL_CTX *ctx;

    bool SecureConnect;
    bool SelectHTTP2;

    std::string ServerSoftware;

    std::string ServerAddr;
    std::string ServerPort;

    std::string Certificate;
    std::string CertificateKey;
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

    fcgi_list_addr *fcgi_list;
    //------------------------------------------------------------------
    Config()
    {
        fcgi_list = NULL;
    }

    ~Config()
    {
        free_fcgi_list();
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
};

extern const Config* const conf;
//======================================================================
struct Stream
{
    Stream *prev;
    Stream *next;

    unsigned long numConn;
    unsigned long numReq;

    int id;
    FRAME_TYPE type;
    int flags;
    time_t Time;

    HTTP_METHOD httpMethod;

    std::string path;
    std::string decode_path;
    std::string query_string;
    char *clean_decode_path;
    int clean_decode_path_size;

    std::string authority;
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

        std::string scriptName;

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
        host.clear();
        user_agent = "-";
        referer = "-";
        range.clear();

        buf.init();
        send_data.init();
        post_data.init();
        headers.init();
        send_data.init();
        post_data.init();

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
class DynamicTable
{
    Header *table;
    int table_size;
    int table_len;
    int offset;
    int err;

    DynamicTable();
    DynamicTable(const DynamicTable&);
    DynamicTable& operator= (const DynamicTable&);

public:

    DynamicTable(int n, int offs)
    {
        table_size = n;
        offset = offs;
        table_len = err = 0;
        table = new(std::nothrow) Header [table_size];
        if (!table)
        {
            fprintf(stderr, "<%s:%d> Error: %s\n", __func__, __LINE__, strerror(errno));
            table_size = 0;
            err = 1;
            return;
        }

        table[0].name = NULL;
        table[0].val = NULL;
    }
    //------------------------------------------------------------------
    ~DynamicTable()
    {
        if (table)
        {
            //fprintf(stderr, "<%s:%d> ~~~ Delete Dynamic Table\n", __func__, __LINE__);
            for ( int i = 0; i < table_len; ++i)
            {
                if (table[i].name && table[i].val)
                {
                    delete [] table[i].name;
                    delete [] table[i].val;
                }
            }
            delete [] table;
            table = NULL;
        }
    }
    //------------------------------------------------------------------
    int add(const char *name, const char *val)
    {
        if (table_size == 0)
            return 0;
        if (table_len == table_size)
        {
            --table_len;
            delete [] table[table_len].name;
            table[table_len].name = NULL;
            delete [] table[table_len].val;
            table[table_len].val = NULL;
        }

        for ( int i = table_len; i > 0; --i)
        {
            table[i] = table[i - 1];
        }

        table[0].name = NULL;
        table[0].val = NULL;

        int len = strlen(name);
        char *n = new(std::nothrow) char [len + 1];
        if (!n)
        {
            return -1;
        }
        memcpy(n, name, len + 1);
        
        len = strlen(val);
        char *v = new(std::nothrow) char [len + 1];
        if (!v)
        {
            delete [] n;
            return -1;
        }
        memcpy(v, val, len + 1);

        table[0].name = n;
        table[0].val = v;
        ++table_len;
        return 0;
    }
    //------------------------------------------------------------------
    void print()
    {
        fprintf(stderr, " -------- Dynamic table %d --------\n", table_len);
        for ( int i = 0; i < table_len; ++i)
        {
            fprintf(stderr, " %04d  [%s: %s]\n", i + offset, table[i].name, table[i].val);
        }
    }
    //------------------------------------------------------------------
    Header *get(int n)
    {
        if ((n < offset) || (n >= (table_len + offset)))
        {
            fprintf(stderr, "<%s:%d> Error out of range: index=%d, table_len=%d, offset=%d\n", __func__, __LINE__, n, table_len, offset);
            return NULL;
        }

        return &table[n - offset];
    }
};

#endif
