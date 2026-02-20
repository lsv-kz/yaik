#include "main.h"

using namespace std;
//======================================================================
static mutex mtx_num_conn;
static condition_variable cond_num_conn;

static int num_conn;

int create_connect(const Server *serv,
                   int clientSocket,
                   unsigned long *allConn,
                   sockaddr_storage *clientAddr);
//======================================================================
void start_conn()
{
mtx_num_conn.lock();
    ++num_conn;
mtx_num_conn.unlock();
}
//======================================================================
void decrement_num_conn()
{
mtx_num_conn.lock();
    --num_conn;
mtx_num_conn.unlock();
    cond_num_conn.notify_one();
}
//======================================================================
bool is_maxconn()
{
unique_lock<mutex> lk(mtx_num_conn);
    while (num_conn >= (conf->MaxAcceptConnections - conf->num_servers))
    {
        cond_num_conn.wait(lk);
    }

    return false;
}
//======================================================================
void accept_connect()
{
    unsigned long allConn = 0;
    num_conn = 0;
    struct pollfd *poll_fd;

    poll_fd = new(nothrow) struct pollfd [conf->num_servers];
    if (!poll_fd)
    {
        print_err("<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }
/*
    if (chdir(conf->DocumentRoot.c_str()))
    {
        print_err("<%s:%d> Error chdir(%s): %s\n", __func__, __LINE__, conf->DocumentRoot.c_str(), strerror(errno));
        exit(EXIT_FAILURE);
    }*/
    //------------------------------------------------------------------
    printf(" +++++ pid=%u, uid=%u, gid=%u +++++\n",
                                getpid(), getuid(), getgid());
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
    bool run = true;
    int num_wait = 0;

    const Server *serv = conf->all_servers;
    for ( num_wait = 0; serv; serv = serv->next, num_wait++)
    {
        poll_fd[num_wait].fd = serv->sock;
        poll_fd[num_wait].events = POLLIN;
    }

    if (num_wait != conf->num_servers)
    {
        run = false;
        print_err("<%s:%d> Error: num_wait != conf->num_servers\n", __func__, __LINE__);
        printf("<%s:%d> Error: num_wait != conf->num_servers\n", __func__, __LINE__);
    }

    while (run)
    {
        struct sockaddr_storage clientAddr;
        socklen_t addrSize = sizeof(struct sockaddr_storage);

        is_maxconn();

        int ret_poll = poll(poll_fd, num_wait, -1);
        if (ret_poll < 0)
        {
            print_err("<%s:%d> Error poll()=-1: %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            break;
        }
        else if (ret_poll == 0)
            continue;

        serv = conf->all_servers;
        for ( int num_ = 0; serv; serv = serv->next, num_++)
        {
            if (poll_fd[num_].fd != serv->sock)
            {
                print_err("<%s:%d> Error server %d, socket (%d != %d)\n", __func__, __LINE__, 
                                num_, poll_fd[num_].fd, serv->sock);
                run = false;
                break;
            }

            if (poll_fd[num_].revents == POLLIN)
            {
                int clientSocket = accept(serv->sock, (struct sockaddr *)&clientAddr, &addrSize);
                if (clientSocket == -1)
                {
                    print_err("<%s:%d>  Error accept(%d): %s\n", __func__, __LINE__, serv->sock, strerror(errno));
                    if ((errno == EMFILE) || (errno == ENFILE)) // (errno == EINTR)
                    {
                        run = false;
                        break;
                    }

                    break;
                }

                int ret = create_connect(serv,
                                         clientSocket,
                                         &allConn,
                                         &clientAddr);
                if (ret == 0)
                    //break;
                    continue;
                else if (ret == -1)
                {
                    run = false;
                    break;
                }
            }
            else if (poll_fd[num_].revents)
            {
                print_err("<%s:%d> Error revents=0x%02X, num_=%d\n", __func__, __LINE__, 
                                                poll_fd[num_].revents, num_);
                run = false;
                break;
            }
        }
    }

    print_err("<%s:%d> all_conn=%lu, open_conn=%d\n", __func__, __LINE__, allConn, num_conn);
    close_work_thread();
    work_thr.join();
    if (poll_fd)
        delete [] poll_fd;
    usleep(100000);
}
//======================================================================
int create_connect(const Server *serv,
                   int clientSocket,
                   unsigned long *allConn,
                   sockaddr_storage *clientAddr)
{
    socklen_t addrSize = sizeof(struct sockaddr_storage);
    Connect *con = new(nothrow) Connect;
    if (!con)
    {
        print_err("<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
        return 0;
    }

    con->numConn = ++(*allConn);

    int flags = 1;
    if (ioctl(clientSocket, FIONBIO, &flags) == -1)
    {
        print_err("<%s:%d> Error ioctl(, FIONBIO, 1): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    flags = fcntl(clientSocket, F_GETFD);
    if (flags == -1)
    {
        print_err("<%s:%d> Error fcntl(, F_GETFD): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    flags |= FD_CLOEXEC;
    if (fcntl(clientSocket, F_SETFD, flags) == -1)
    {
        print_err("<%s:%d> Error fcntl(, F_SETFD, FD_CLOEXEC): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    con->ServerPort = serv->port;
    con->serverSocket = serv->sock;
    con->clientSocket = clientSocket;

    int err;
    if ((err = getnameinfo((struct sockaddr *)clientAddr,
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
        return 0;
    }

    con->serv = serv;
    con->SecureConnect = false;
    if (serv->SecureConnect)
    {
        con->tls.err = 0;
        con->tls.ssl = SSL_new(serv->vhosts->ctx);
        if (!con->tls.ssl)
        {
            print_err(con, "<%s:%d> Error SSL_new()\n", __func__, __LINE__);
            shutdown(clientSocket, SHUT_RDWR);
            close(clientSocket);
            delete con;
            return -1;
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
            return 0;
        }

        con->tls.poll_events = POLLIN | POLLOUT;
        con->Protocol = PROTOCOL_SELECT;
        start_conn();
        push_wait_list(con);
    }
    else
    {
        con->Protocol = P_HTTP1;
        con->h1 = new(nothrow) http1;
        if (con->h1)
        {
            con->h1->con_status = http1::READ_REQUEST;
            con->h1->resp.numConn = con->numConn;
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
            return -1;
        }
    }

    return 1;
}
