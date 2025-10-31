#include "main.h"

using namespace std;
//======================================================================
static mutex mtx_conn;
static condition_variable cond_close_conn;

static mutex mtx_;
static condition_variable cond_;
static Connect *list_start;
static Connect *list_end;

static int num_conn, thr_exit = 0;
//======================================================================
static void close_connect(Connect *c);
static void thread_ssl_accept();
//======================================================================
static void push_list(Connect *c)
{
mtx_.lock();
    c->next = NULL;
    c->prev = list_end;
    if (list_end)
        list_end->next = c;
    list_end = c;
    if (!list_start)
        list_start = c;
mtx_.unlock();
    cond_.notify_one();
}
//======================================================================
static void delete_from_list(Connect *c)
{
mtx_.lock();
    if (!c->next && !c->prev)
    {
        list_start = list_end = NULL;
    }
    else if (!c->next && c->prev)
    {
        list_end = c->prev;
        c->prev->next = NULL;
    }
    else if (c->next && c->prev)
    {
        c->next->prev = c->prev;
        c->prev->next = c->next;
    }
    else if (c->next && !c->prev)
    {
        list_start = c->next;
        c->next->prev = NULL;
    }
mtx_.unlock();
}
//======================================================================
static void start_conn()
{
mtx_conn.lock();
    ++num_conn;
mtx_conn.unlock();
}
//======================================================================
void decrement_num_conn()
{
mtx_conn.lock();
    --num_conn;
mtx_conn.unlock();
    cond_close_conn.notify_one();
}
//======================================================================
static void is_maxconn()
{
unique_lock<mutex> lk(mtx_conn);
    while (num_conn >= conf->MaxAcceptConnections)
    {
        cond_close_conn.wait(lk);
    }
}
//======================================================================
void accept_connect(int serverSocket)
{
    unsigned long allConn = 0;
    num_conn = 0;
    list_start = list_end = NULL;

    if (chdir(conf->DocumentRoot.c_str()))
    {
        print_err("<%s:%d> Error chdir(%s): %s\n", __func__, __LINE__, conf->DocumentRoot.c_str(), strerror(errno));
        exit(EXIT_FAILURE);
    }
    //------------------------------------------------------------------
    printf(" +++++ pid=%u, uid=%u, gid=%u +++++\n",
                                getpid(), getuid(), getgid());
    //------------------------------------------------------------------
    thread thr_accept;
    try
    {
        thr_accept = thread(thread_ssl_accept);
    }
    catch (...)
    {
        print_err("<%s:%d> Error create thread(thread_ssl_accept): errno=%d\n", __func__, __LINE__, errno);
        exit(errno);
    }
    //------------------------------------------------------------------
    thread work_thr;
    try
    {
        work_thr = thread(event_handler);
    }
    catch (...)
    {
        print_err("<%s:%d> Error create thread(event_handler): errno=%d\n", __func__, __LINE__, errno);
        exit(errno);
    }
    //------------------------------------------------------------------
    int run = 1;
    struct pollfd poll_fd[2];
    poll_fd[0].fd = serverSocket;
    poll_fd[0].events = POLLIN;

    while (run)
    {
        struct sockaddr_storage clientAddr;
        socklen_t addrSize = sizeof(struct sockaddr_storage);

        is_maxconn();

        int ret_poll = poll(poll_fd, 1, -1);
        if (ret_poll < 0)
        {
            print_err("<%s:%d> Error poll()=-1: %s\n", __func__, __LINE__, strerror(errno));
            //if (errno == EINTR)
                //continue;
            break;
        }
        else if (ret_poll == 0)
            continue;

        if (poll_fd[0].revents == POLLIN)
        {
            int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrSize);
            if (clientSocket == -1)
            {
                print_err("<%s:%d>  Error accept(): %s\n", __func__, __LINE__, strerror(errno));
                if ((errno == EMFILE) || (errno == ENFILE)) // (errno == EINTR)
                    continue;
                break;
            }

            Connect *con = new(nothrow) Connect;
            if (!con)
            {
                print_err("<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
                shutdown(clientSocket, SHUT_RDWR);
                close(clientSocket);
                continue;
            }

            con->numConn = ++allConn;

            int flags = 1;
            if (ioctl(clientSocket, FIONBIO, &flags) == -1)
            {
                print_err("<%s:%d> Error ioctl(, FIONBIO, 1): %s\n", __func__, __LINE__, strerror(errno));
                break;
            }

            flags = fcntl(clientSocket, F_GETFD);
            if (flags == -1)
            {
                print_err("<%s:%d> Error fcntl(, F_GETFD): %s\n", __func__, __LINE__, strerror(errno));
                break;
            }

            flags |= FD_CLOEXEC;
            if (fcntl(clientSocket, F_SETFD, flags) == -1)
            {
                print_err("<%s:%d> Error fcntl(, F_SETFD, FD_CLOEXEC): %s\n", __func__, __LINE__, strerror(errno));
                break;
            }

            con->serverSocket = serverSocket;
            con->clientSocket = clientSocket;

            int err;
            if ((err = getnameinfo((struct sockaddr *)&clientAddr,
                    addrSize,
                    con->remoteAddr,
                    sizeof(con->remoteAddr),
                    con->remotePort,
                    sizeof(con->remotePort),
                    NI_NUMERICHOST | NI_NUMERICSERV)))
            {
                print_err(con, "<%s:%d> Error getnameinfo()=%d: %s\n", __func__, __LINE__, err, gai_strerror(err));
                con->remoteAddr[0] = 0;
                con->remotePort[0] = 0;
                shutdown(clientSocket, SHUT_RDWR);
                close(clientSocket);
                delete con;
                continue;
            }

            if (conf->SecureConnect)
            {
                con->tls.err = 0;
                con->tls.ssl = SSL_new(conf->ctx);
                if (!con->tls.ssl)
                {
                    print_err(con, "<%s:%d> Error SSL_new()\n", __func__, __LINE__);
                    shutdown(clientSocket, SHUT_RDWR);
                    close(clientSocket);
                    delete con;
                    break;
                }

                int ret = SSL_set_fd(con->tls.ssl, con->clientSocket);
                if (ret == 0)
                {
                    con->tls.err = SSL_get_error(con->tls.ssl, ret);
                    print_err(con, "<%s:%d> Error SSL_set_fd(): %s\n", __func__, __LINE__, ssl_strerror(con->tls.err));
                    SSL_free(con->tls.ssl);
                    shutdown(clientSocket, SHUT_RDWR);
                    close(clientSocket);
                    delete con;
                    continue;
                }

                con->tls.poll_events = POLLIN | POLLOUT;
                con->client_timer = time(NULL);
                start_conn();
                push_list(con);
            }
            else
            {
                con->Protocol = P_HTTP1;
                con->h1 = new(nothrow) http1;
                if (con->h1)
                {
                    con->h1->con_status = http1::READ_REQUEST;
                    con->h1->resp.numConn = con->numReq;
                    con->h1->resp.numReq = 1;
                    start_conn();
                    push_wait_list(con);
                }
                else
                {
                    print_err(con, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
                    shutdown(clientSocket, SHUT_RDWR);
                    close(clientSocket);
                    delete con;
                }
            }
        }
        else if (poll_fd[0].revents & POLLERR)
        {
            print_err("<%s:%d> Error revents=0x%02X\n", __func__, __LINE__, poll_fd[0].revents);
            break;
        }
    }

    thr_exit = 1;
    cond_.notify_one();
    thr_accept.join();
    close_work_thread();
    work_thr.join();
    print_err("<%s:%d> all_conn=%lu, open_conn=%d\n", __func__, __LINE__, allConn, num_conn);
    usleep(100000);
}
//======================================================================
void thread_ssl_accept()
{
    static struct pollfd *poll_fd;
    poll_fd = new(nothrow) struct pollfd [conf->MaxAcceptConnections];
    if (!poll_fd)
    {
        print_err("<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }

    for ( ; ; )
    {
        {
    unique_lock<mutex> lk(mtx_);
            while (!list_start)
            {
                cond_.wait(lk);
                if (thr_exit)
                    break;
            }

            if (thr_exit)
                break;
        }
        
        Connect *c = list_start, *next = NULL;
        int num_poll = 0;
        for ( ; c; c = next)
        {
            next = c->next;
            time_t t = time(NULL);
            if ((t - c->client_timer) >= conf->Timeout)
            {
                print_err(c, "<%s:%d> Timeout=%ld\n", __func__, __LINE__, t - c->client_timer);
                close_connect(c);
            }
            else
            {
                poll_fd[num_poll].fd = c->clientSocket;
                poll_fd[num_poll].events = c->tls.poll_events;
                ++num_poll;
            }
        }

        int ret_poll = poll(poll_fd, num_poll, conf->TimeoutPoll);
        if (ret_poll < 0)
        {
            print_err("<%s:%d> Error poll()=-1: %s\n", __func__, __LINE__, strerror(errno));
            break;
        }
        else if (ret_poll == 0)
            continue;
    
        c = list_start;
        next = NULL;
        for ( int i = 0; c && (i < num_poll); ++i, c = next )
        {
            next = c->next;
            int revents = poll_fd[i].revents;
            if (revents & (POLLIN | POLLOUT))
            {
                int ret = ssl_accept(c);
                if (ret == 1)
                {
                    delete_from_list(c);
                    c->tls.poll_events = POLLIN;
                    c->client_timer = 0;
                    if (c->Protocol == P_HTTP2)
                    {
                        c->h2 = new(nothrow) http2;
                        if (c->h2)
                        {
                            c->h2->con_status = http2::PREFACE_MESSAGE;
                            c->numReq = 0;
                            push_wait_list(c);
                        }
                        else
                        {
                            print_err(c, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
                            close_connect(c);
                        }
                    }
                    else if (c->Protocol == P_HTTP1)
                    {
                        c->h1 = new(nothrow) http1;
                        if (c->h1)
                        {
                            c->h1->con_status = http1::READ_REQUEST;
                            c->h1->resp.numConn = c->numReq;
                            c->h1->resp.numReq = 1;
                            push_wait_list(c);
                        }
                        else
                        {
                            print_err(c, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
                            close_connect(c);
                        }
                    }
                    else
                    {
                        print_err(c, "<%s:%d> Error Protocol\n", __func__, __LINE__);
                        SSL_free(c->tls.ssl);
                        shutdown(c->clientSocket, SHUT_RDWR);
                        close(c->clientSocket);
                        delete c;
                    }
                }
                else if (ret < 0)
                {
                    print_err(c, "<%s:%d> Error ssl_accept()\n", __func__, __LINE__);
                    close_connect(c);
                }
            }
            else if (revents)
            {
                print_err("<%s:%d>  Error revents=0x%02X\n", __func__, __LINE__, poll_fd[0].revents);
                close_connect(c);
            }
        }
    }

    delete [] poll_fd;
}
//======================================================================
void close_connect(Connect *c)
{
    delete_from_list(c);
    if (c->tls.ssl)
    {
        SSL_clear(c->tls.ssl);
        SSL_free(c->tls.ssl);
    }

    shutdown(c->clientSocket, SHUT_RDWR);
    close(c->clientSocket);
    delete_from_list(c);
    delete c;
    decrement_num_conn();
}
//======================================================================
void Exit(int n)
{
    thr_exit = 1;
    cond_.notify_one();
    exit(n);
}
