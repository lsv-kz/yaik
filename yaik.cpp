#include "main.h"

using namespace std;
//======================================================================
static int serverSocket = -1;
int Connect::serverSocket;

static string pidFile;
const char *nameConfifFile = "yaik.conf";
static string confPath;
static string cwd;
static string myFileName;

//static void print_config();
//======================================================================
static void signal_handler(int signo)
{
    if (signo == SIGINT)
    {
        fprintf(stderr, "[%s] - <%s> ####### SIGINT #######\n", log_time().c_str(), __func__);
        shutdown(serverSocket, SHUT_RDWR);
        close(serverSocket);
        serverSocket = -1;
    }
    else if (signo == SIGSEGV)
    {
        fprintf(stderr, "[%s] - <%s> ####### SIGSEGV #######\n", log_time().c_str(), __func__);
        abort();
    }
    else
        fprintf(stderr, "[%s] - <%s> ? signo=%d (%s)\n", log_time().c_str(), __func__, signo, strsignal(signo));
}
//======================================================================
int main(int argc, char *argv[])
{
    pid_t pid;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        fprintf(stderr, "<%s:%d> Error signal(SIGPIPE): %s\n", __func__, __LINE__, strerror(errno));
        return 1;
    }

    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
        fprintf(stderr, "<%s:%d> Error signal(SIGCHLD): %s\n", __func__, __LINE__, strerror(errno));
        return 1;
    }

    if (signal(SIGINT, signal_handler) == SIG_ERR)
    {
        fprintf(stderr, "<%s:%d> Error signal(SIGINT): %s\n", __func__, __LINE__, strerror(errno));
        return 1;
    }

    if (signal(SIGSEGV, signal_handler) == SIG_ERR)
    {
        fprintf(stderr, "<%s:%d> Error signal(SIGSEGV): %s\n", __func__, __LINE__, strerror(errno));
        return 1;
    }
    //------------------------------------------------------------------
    confPath = nameConfifFile;
    if (read_conf_file(confPath.c_str()))
        return 1;
    //------------------------------------------------------------------
    serverSocket = create_server_socket(conf);
    if (serverSocket == -1)
    {
        fprintf(stderr, "<%s:%d> Error: create_server_socket(%s:%s)\n", __func__, __LINE__,
                    conf->ServerAddr.c_str(), conf->ServerPort.c_str());
        if (conf->SecureConnect && conf->ctx)
        {
            SSL_CTX_free(conf->ctx);
            cleanup_openssl();
        }
        return 1;
    }

    Connect::serverSocket = serverSocket;
    //------------------------------------------------------------------
    create_logfiles(conf->LogPath);
    //------------------------------------------------------------------
    pid = getpid();
    cout << "\n[" << get_time().c_str() << "] - server \"" << conf->ServerSoftware.c_str()
         << "\" run, port: " << conf->ServerPort.c_str()
         << "\nhardware_concurrency = " << thread::hardware_concurrency() << "\n";
    if (conf->SecureConnect)
    {
        SSL  *ssl = SSL_new(conf->ctx);
        cout << "SSL version: " << SSL_get_version(ssl) << "\n";
        SSL_free(ssl);
    }

    pid_t gid = getgid();
    cout << "pid="  << pid << "; uid=" << getuid() << "; gid=" << gid << "\n";
    cerr << "   pid="  << pid << "; uid=" << getuid() << "; gid=" << gid
         << "\n   NumCpuCores: " << thread::hardware_concurrency() << "\n";
    //print_config();
    //------------------------------------------------------------------
    for ( int i = 0; environ[i]; )
    {
        char *p, buf[512];
        if ((p = (char*)memccpy(buf, environ[i], '=', strlen(environ[i]))))
        {/*
            if (strstr(environ[i], "DISPLAY") || strstr(environ[i], "XDG_RUNTIME_DIR") ||
                strstr(environ[i], "HOME") ||
                strstr(environ[i], "SESSION_MANAGER") ||
                strstr(environ[i], "PATH")
             )
            {
                i++;
                continue;
            }
*/
            *(p - 1) = 0;
            unsetenv(buf);
        }
    }
    //------------------------------------------------------------------
    accept_connect(serverSocket);

    if (serverSocket > 0)
    {
        shutdown(serverSocket, SHUT_RDWR);
        close(serverSocket);
        serverSocket = -1;
    }

    if (conf->SecureConnect)
    {
        SSL_CTX_free(conf->ctx);
        cleanup_openssl();
    }
    return 0;
}
//======================================================================
void print_limits()
{
    struct rlimit lim;
    if (getrlimit(RLIMIT_NOFILE, &lim) == -1)
        cerr << " Error getrlimit(RLIMIT_NOFILE): " << strerror(errno) << "\n";
    else
        cout << " RLIMIT_NOFILE: cur=" << (long)lim.rlim_cur << ", max=" << (long)lim.rlim_max << "\n";
    cout << " hardware_concurrency(): " << thread::hardware_concurrency() << "\n\n";

    int sndbuf = get_size_sock_buf(AF_INET, SO_SNDBUF, SOCK_STREAM, 0);
    if (sndbuf < 0)
        cerr << " Error get_size_sock_buf(AF_INET, SO_SNDBUF, SOCK_STREAM, 0): " << strerror(-sndbuf) << "\n";
    else
        cout << " AF_INET: SO_SNDBUF=" << sndbuf << "\n";

    sndbuf = get_size_sock_buf(AF_INET, SO_RCVBUF, SOCK_STREAM, 0);
    if (sndbuf < 0)
        cerr << " Error get_size_sock_buf(AF_INET, SO_RCVBUF, SOCK_STREAM, 0): " << strerror(-sndbuf) << "\n\n";
    else
        cout << " AF_INET: SO_RCVBUF=" << sndbuf << "\n\n";
}
//======================================================================
void print_config()
{
    print_limits();

    cout << "   PrintDebugMsg          : " << conf->PrintDebugMsg
         << "\n   SecureConnect          : " << ((conf->SecureConnect) ? "on" : "off")
         << "\n   ServerSoftware         : " << conf->ServerSoftware.c_str()
         << "\n   ServerAddr             : " << conf->ServerAddr.c_str()
         << "\n   ServerPort             : " << conf->ServerPort.c_str()
         << "\n   DocumentRoot           : " << conf->DocumentRoot.c_str()
         << "\n   ScriptPath             : " << conf->ScriptPath.c_str()
         << "\n   LogPath                : " << conf->LogPath.c_str()
         << "\n   UsePHP                 : " << conf->UsePHP.c_str()
         << "\n   PathPHP                : " << conf->PathPHP.c_str()
         << "\n   ListenBacklog          : " << conf->ListenBacklog
         << "\n   TcpNoDelay             : " << conf->TcpNoDelay
         << "\n   MaxConcurrentStreams   : " << conf->MaxConcurrentStreams
         << "\n   MaxAcceptConnections   : " << conf->MaxAcceptConnections
         << "\n   HTTP1_DataBufSize      : " << conf->HTTP1_DataBufSize
         << "\n   HTTP2_DataBufSize      : " << conf->HTTP2_DataBufSize
         << "\n   MaxCgiProc             : " << conf->MaxCgiProc
         << "\n   Timeout                : " << conf->Timeout
         << "\n   TimeoutKeepAlive       : " << conf->TimeoutKeepAlive
         << "\n   TimeoutPoll            : " << conf->TimeoutPoll
         << "\n   TimeoutCGI             : " << conf->TimeoutCGI
         << "\n   ClientMaxBodySize      : " << conf->ClientMaxBodySize
         << "\n   ShowMediaFiles         : " << conf->ShowMediaFiles
         << "\n   User                   : " << conf->user.c_str()
         << "\n   Group                  : " << conf->group.c_str()
         << "\n";

    cout << "   ------------- FastCGI/SCGI -------------\n";
    fcgi_list_addr *i = conf->fcgi_list;
    for (; i; i = i->next)
    {
        cout << "   [" << i->script_name.c_str() << " : " << i->addr.c_str() << "] - " << get_cgi_type(i->type) << "\n";
    }
}
