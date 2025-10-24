#include "main.h"

using namespace std;
//======================================================================
void cgi_worker(Connect *con, Stream *resp, struct pollfd*);

static EventHandlerClass event_handler_cl;
//======================================================================
EventHandlerClass::~EventHandlerClass()
{
    if (conn_array)
        delete [] conn_array;
    if (poll_fd)
        delete [] poll_fd;
    if (cgi_array)
        delete [] cgi_array;
}
//======================================================================
EventHandlerClass::EventHandlerClass()
{
    num_request = 0;
    close_thr = num_wait = all_cgi = 0;
    work_list_start = work_list_end = wait_list_start = wait_list_end = NULL;
    cgi_array = NULL;
}
//======================================================================
void EventHandlerClass::init()
{
    conn_array = new(nothrow) Connect* [conf->MaxAcceptConnections];
    if (!conn_array)
    {
        print_err("<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        exit(1);
    }

    poll_fd = new(nothrow) struct pollfd [conf->MaxAcceptConnections];
    if (!poll_fd)
    {
        print_err("<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        exit(1);
    }

    cgi_array = new(nothrow) Stream* [conf->MaxCgiProc];
    if (!cgi_array)
    {
        print_err("<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        exit(1);
    }
}
//======================================================================
void EventHandlerClass::del_from_list(Connect *c)
{
    if (c->prev && c->next)
    {
        c->prev->next = c->next;
        c->next->prev = c->prev;
    }
    else if (c->prev && !c->next)
    {
        c->prev->next = c->next;
        work_list_end = c->prev;
    }
    else if (!c->prev && c->next)
    {
        c->next->prev = c->prev;
        work_list_start = c->next;
    }
    else if (!c->prev && !c->next)
        work_list_start = work_list_end = NULL;
}
//======================================================================
void EventHandlerClass::add_work_list()
{
mtx_thr.lock();
    if (wait_list_start)
    {
        if (work_list_end)
            work_list_end->next = wait_list_start;
        else
            work_list_start = wait_list_start;

        wait_list_start->prev = work_list_end;
        work_list_end = wait_list_end;
        wait_list_start = wait_list_end = NULL;
    }
mtx_thr.unlock();
}
//======================================================================
int EventHandlerClass::cgi_poll()
{
    num_wait = 0;

    Connect *c = work_list_start, *next = NULL;
    for ( ; c; c = next)
    {
        next = c->next;
        if (c->Protocol == P_HTTP1)
        {
            http1_cgi_set(c);
        }
        else if (c->Protocol == P_HTTP2)
        {
            http2_cgi_set(c);
        }

        if (num_wait >= conf->MaxAcceptConnections)
        {
            print_err(c, "<%s:%d> !!! num_wait[%d] >= conf->MaxAcceptConnections[%d]\n", __func__, __LINE__, 
                    num_wait, conf->MaxAcceptConnections);
            break;
        }
    }

    if (num_wait == 0)
        return 0;

    int ret_poll = poll(poll_fd, num_wait, conf->TimeoutPoll);
    if (ret_poll == -1)
    {
        print_err("<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }
    else if (ret_poll == 0)
    {
        return 0;
    }

    for ( int i = 0; (i < num_wait); ++i)
    {
        c = conn_array[i];
        if (c->Protocol == P_HTTP1)
        {
            http1_cgi_poll(c, i);
        }
        else if (c->Protocol == P_HTTP2)
        {
            http2_cgi_poll(c, i);
        }
    }

    return 0;
}
//======================================================================
void EventHandlerClass::http2_cgi_set(Connect *c)
{
    time_t t = time(NULL);
    Stream *resp_next = NULL, *resp = c->h2->get();
    if (!resp)
    {
        return;
    }

    for ( ; resp; resp = resp_next)
    {
        resp_next = resp->next;

        if ((resp->source_data != DYN_PAGE) || resp->cgi.end)
            continue;

        if (num_wait >= conf->MaxAcceptConnections)
            break;

        if ((resp->cgi.timer == 0) || (resp->cgi.start == false))
            resp->cgi.timer = t;
        if ((t - resp->cgi.timer) >= conf->TimeoutCGI)
        {
            print_err(resp, "<%s:%d> TimeoutCGI=%ld, send_headers=%d, cgi.op=%s, id=%d \n", __func__, __LINE__, 
                    t - resp->cgi.timer, resp->send_headers, get_cgi_status(resp->cgi_status), resp->id);
            c->h2->frame_win_update.init();
            resp->frame_win_update.init();
            if (c->h2->try_again == true)
                resp->rst_stream = true;
            else
            {
                if (resp->send_headers)
                    set_rst_stream(c, resp->id, CANCEL);
                else
                    resp_504(resp);
            }
        }
        else
        {
            if ((resp->cgi_status == CGI_CREATE) && (all_cgi < conf->MaxCgiProc))
            {
                if ((resp->cgi_type == CGI) || (resp->cgi_type == PHPCGI))
                {
                    int ret = cgi_create_proc(c, resp);
                    if (ret < 0)
                    {
                        print_err(c, "<%s:%d> Error cgi_create_proc()\n", __func__, __LINE__);
                        if (ret == -RS404)
                            resp_404(resp);
                        else
                            resp_500(resp);
                        continue;
                    }
                }
                else if ((resp->cgi_type == PHPFPM) || (resp->cgi_type == FASTCGI))
                {
                    int ret = fcgi_create_connect(c, resp);
                    if (ret < 0)
                    {
                        resp_500(resp);
                        continue;
                    }
                    else
                        resp->cgi_status = FASTCGI_BEGIN;
                }
                else if (resp->cgi_type == SCGI)
                {
                    int ret = scgi_create_connect(c, resp);
                    if (ret < 0)
                    {
                        resp_500(resp);
                        continue;
                    }
                    else
                        resp->cgi_status = SCGI_PARAMS;
                }

                c->client_timer = 0;
                resp->cgi.start = true;
                ++all_cgi;
            }

            if ((resp->cgi_status == FASTCGI_BEGIN) || (resp->cgi_status == FASTCGI_PARAMS) || (resp->cgi_status == SCGI_PARAMS))
            {
                poll_fd[num_wait].fd = resp->cgi.fd;
                poll_fd[num_wait].events = POLLOUT;
                conn_array[num_wait] = c;
                cgi_array[num_wait] = resp;
                ++num_wait;
            }
            else if ((resp->cgi_status == CGI_STDIN) && resp->post_data.size())
            {
                if ((resp->cgi_type == CGI) || (resp->cgi_type == PHPCGI))
                {
                    poll_fd[num_wait].fd = resp->cgi.to_script;
                }
                else
                {
                    poll_fd[num_wait].fd = resp->cgi.fd;
                }

                poll_fd[num_wait].events = POLLOUT;
                conn_array[num_wait] = c;
                cgi_array[num_wait] = resp;
                ++num_wait;
            }
            else if ((resp->cgi_status == CGI_STDOUT) && (resp->send_data.size() == 0))
            {
                if ((resp->cgi_type == CGI) || (resp->cgi_type == PHPCGI))
                {
                    poll_fd[num_wait].fd = resp->cgi.from_script;
                }
                else
                {
                    poll_fd[num_wait].fd = resp->cgi.fd;
                }

                poll_fd[num_wait].events = POLLIN;
                conn_array[num_wait] = c;
                cgi_array[num_wait] = resp;
                ++num_wait;
            }
        }
    }
}
//======================================================================
void EventHandlerClass::http2_cgi_poll(Connect *c, int poll_ind)
{
    int fd = poll_fd[poll_ind].fd;
    int revents = poll_fd[poll_ind].revents;
    Stream *resp = cgi_array[poll_ind];

    if ((fd != resp->cgi.to_script) && (fd != resp->cgi.from_script) && (fd != resp->cgi.fd))
    {
        //if (conf->PrintDebugMsg)
        {
            print_err(resp, "<%s:%d> revents=0x%02X, (fd(%d) != cgi.fd(%d/%d/%d)), cgi.end=%d, id=%d \n", __func__, __LINE__, 
                revents, fd, resp->cgi.to_script, resp->cgi.from_script, resp->cgi.fd, resp->cgi.end, resp->id);
        }
        return;
    }

    if (revents)
    {
        if ((resp->cgi_type == CGI) || (resp->cgi_type == PHPCGI))
        {
            cgi_worker(c, resp, poll_ind);
        }
        else if ((resp->cgi_type == PHPFPM) || (resp->cgi_type == FASTCGI))
        {
            fcgi_worker(c, resp, poll_ind);
        }
        else if (resp->cgi_type == SCGI)
        {
            if (resp->cgi_status == SCGI_PARAMS)
                scgi_worker(c, resp, poll_ind);
            else
                cgi_worker(c, resp, poll_ind);
        }
        else
        {
            print_err("<%s:%d> Error cgi_type=%d, id=%d \n", __func__, __LINE__, resp->cgi_type, resp->id);
        }
    }
    else
    {
        if (conf->PrintDebugMsg)
            print_err("<%s:%d> revents=0, id=%d \n", __func__, __LINE__, resp->id);
    }
}
//======================================================================
void EventHandlerClass::http1_cgi_set(Connect *c)
{
    time_t t = time(NULL);

    if ((c->h1->resp.source_data != DYN_PAGE) || c->h1->resp.cgi.end)
    {
        return;
    }

    if ((c->h1->resp.cgi.timer == 0) || (c->h1->resp.cgi.start == false))
            c->h1->resp.cgi.timer = t;
    if ((t - c->h1->resp.cgi.timer) >= conf->TimeoutCGI)
    {
        print_err(c, "<%s:%d> TimeoutCGI=%ld, %s, cgi_status=%s\n", __func__, __LINE__, 
                    t - c->h1->resp.cgi.timer, c->h1->get_str_status(), get_cgi_status(c->h1->resp.cgi_status));
        if (c->h1->resp.send_headers == false)
            c->err = -RS504;
        else
            c->err = -1;
        c->h1->resp.cgi.end = true;
        http1_end_request(c);
        return;
    }

    if ((c->h1->resp.cgi_status == CGI_CREATE) && (all_cgi < conf->MaxCgiProc))
    {
        if ((c->h1->resp.cgi_type == CGI) || (c->h1->resp.cgi_type == PHPCGI))
        {
            int ret = cgi_create_proc(c, &c->h1->resp);
            if (ret < 0)
            {
                print_err(c, "<%s:%d> Error cgi_create_proc()=%d\n", __func__, __LINE__, ret);
                c->err = ret;
                c->h1->resp.cgi.end = true;
                http1_end_request(c);
                return;
            }

            if (c->h1->resp.httpMethod == M_POST)
            {
                if (c->h1->resp.post_content_len <= 0)
                    c->h1->con_status = http1::SEND_RESP_HEADERS;
                else
                    c->h1->con_status = http1::READ_POSTDATA;
            }
            else
                c->h1->con_status = http1::SEND_RESP_HEADERS;
        }
        else if (c->h1->resp.cgi_type == SCGI)
        {
            int ret = scgi_create_connect(c, &c->h1->resp);
            if (ret < 0)
            {
                print_err(c, "<%s:%d> Error scgi_create_proc()=%d\n", __func__, __LINE__, ret);
                c->err = ret;
                c->h1->resp.cgi.end = true;
                http1_end_request(c);
                return;
            }
            else
                c->h1->resp.cgi_status = SCGI_PARAMS;
        }
        else if ((c->h1->resp.cgi_type == PHPFPM) || (c->h1->resp.cgi_type == FASTCGI))
        {
            int ret = fcgi_create_connect(c, &c->h1->resp);
            if (ret < 0)
            {
                c->err = ret;
                c->h1->resp.cgi.end = true;
                http1_end_request(c);
                return;
            }
            else
            {
                c->h1->resp.cgi_status = FASTCGI_BEGIN;
            }
        }
        else
        {
            print_err(c, "<%s:%d> 404 Not Found\n", __func__, __LINE__);
            c->err = -RS404;
            http1_end_request(c);
            return;
        }

        c->h1->mode_send = (c->h1->connKeepAlive) ? CHUNK : NO_CHUNK;
        c->client_timer = 0;
        c->h1->resp.cgi.start = true;
        ++all_cgi;
        return;
    }

    if ((c->h1->resp.cgi_status == FASTCGI_BEGIN) || 
        (c->h1->resp.cgi_status == FASTCGI_PARAMS) || 
        (c->h1->resp.cgi_status == SCGI_PARAMS)
    )
    {
        poll_fd[num_wait].fd = c->h1->resp.cgi.fd;
        poll_fd[num_wait].events = POLLOUT;
        conn_array[num_wait] = c;
        cgi_array[num_wait] = &c->h1->resp;
        ++num_wait;
    }
    else if ((c->h1->resp.cgi_status == CGI_STDIN) && c->h1->resp.post_data.size_remain())
    {
        if ((c->h1->resp.cgi_type == CGI) || (c->h1->resp.cgi_type == PHPCGI))
        {
            poll_fd[num_wait].fd = c->h1->resp.cgi.to_script;
        }
        else
        {
            poll_fd[num_wait].fd = c->h1->resp.cgi.fd;
        }

        poll_fd[num_wait].events = POLLOUT;
        conn_array[num_wait] = c;
        cgi_array[num_wait] = &c->h1->resp;
        ++num_wait;
    }
    else if ((c->h1->resp.cgi_status == CGI_STDOUT) && 
             ((c->h1->resp.send_data.size_remain() == 0) || (c->h1->resp.create_headers == false))
    )
    {
        if ((c->h1->resp.cgi_type == CGI) || (c->h1->resp.cgi_type == PHPCGI))
        {
            poll_fd[num_wait].fd = c->h1->resp.cgi.from_script;
        }
        else
        {
            poll_fd[num_wait].fd = c->h1->resp.cgi.fd;
        }

        poll_fd[num_wait].events = POLLIN;
        conn_array[num_wait] = c;
        cgi_array[num_wait] = &c->h1->resp;
        ++num_wait;
    }
}
//======================================================================
void EventHandlerClass::http1_cgi_poll(Connect *c, int poll_ind)
{
    if (poll_fd[poll_ind].revents)
    {
        if ((c->h1->resp.cgi_type == CGI) || (c->h1->resp.cgi_type == PHPCGI))
        {
            cgi_worker(c, poll_ind);
        }
        else if (c->h1->resp.cgi_type == SCGI)
        {
            if (c->h1->resp.cgi_status == SCGI_PARAMS)
                scgi_worker(c, poll_ind);
            else
                cgi_worker(c, poll_ind);
        }
        else if ((c->h1->resp.cgi_type == PHPFPM) || (c->h1->resp.cgi_type == FASTCGI))
        {
            fcgi_worker(c, poll_ind);
        }
        else
        {
            print_err("<%s:%d> Error cgi_type=%d\n", __func__, __LINE__, c->h1->resp.cgi_type);
            c->err = -RS502;
            http1_end_request(c);
        }
    }
}
//======================================================================
void EventHandlerClass::set_poll()
{
    num_wait = 0;
    Connect *c = work_list_start, *next = NULL;
    for ( ; c; c = next)
    {
        if (num_wait >= conf->MaxAcceptConnections)
        {
            print_err(c, "<%s:%d> !!! num_wait[%d] >= conf->MaxAcceptConnections[%d]\n", __func__, __LINE__, 
                    num_wait, conf->MaxAcceptConnections);
            break;
        }

        next = c->next;
        if (c->Protocol == P_HTTP2)
        {
            http2_set_poll(c);
        }
        else if (c->Protocol == P_HTTP1)
        {
            http1_set_poll(c);
        }
        else
        {
            print_err(c, "<%s:%d> \"We are not here.\" - Putin\n", __func__, __LINE__);
        }
    }
}
//======================================================================
void EventHandlerClass::http1_set_poll(Connect *c)
{
    time_t t = time(NULL);
    int Timeout = conf->Timeout;
    if (c->client_timer == 0)
        c->client_timer = t;

    if ((c->h1->con_status == http1::READ_REQUEST) && (c->numReq > 1))
        Timeout = conf->TimeoutKeepAlive;

    if ((t - c->client_timer) >= Timeout)
    {
        print_err(c, "<%s:%d> Timeout=%ld, %s\n", __func__, __LINE__, t - c->client_timer, c->h1->get_str_status());
        if (c->h1->con_status == http1::SSL_SHUTDOWN)
            close_connect(c);
        else
        {
            c->err = -1;
            http1_end_request(c);
        }
    }
    else
    {
        if (conf->SecureConnect)
        {
            int ret = 0, pending = 0;
            while ((pending = SSL_pending(c->tls.ssl)))
            {
                if (conf->PrintDebugMsg)
                {
                    print_err(c, "<%s:%d> ***** SSL_pending()=%d, %s\n", __func__, __LINE__, pending, c->h1->get_str_status());
                }

                if (c->h1->con_status == http1::READ_REQUEST)
                {
                    ret = http1_worker(c, POLLIN);
                    if (ret < 0)
                    {
                        print_err(c, "<%s:%d> Error read_request_headers()=%d\n", __func__, __LINE__, ret);
                        return;
                    }
                }
                else if ((c->h1->resp.post_content_len > 0) && 
                         (c->h1->resp.httpMethod == M_POST) && 
                         (c->h1->resp.post_data.size() == 0)
                )
                {
                    int ret = read_post_data(c);
                    if (ret <= 0)
                    {
                        if (ret != ERR_TRY_AGAIN)
                        {
                            print_err(c, "<%s:%d> Error read_post_data()=%d\n", __func__, __LINE__, ret);
                            c->err = ret;
                            http1_end_request(c);
                            return;
                        }
                    }
                }
                else
                    break;
            }

            if (ret < 0)
                return;
        }

        poll_fd[num_wait].events = 0;
        if (c->h1->con_status == http1::READ_REQUEST)
        {
            poll_fd[num_wait].fd = c->clientSocket;
            poll_fd[num_wait].events = POLLIN;
        }
        else if ((c->h1->con_status == http1::READ_POSTDATA) && (c->h1->resp.post_data.size() <= 32768))
        {
            poll_fd[num_wait].fd = c->clientSocket;
            poll_fd[num_wait].events = POLLIN;
        }
        else if ((c->h1->con_status == http1::SEND_RESP_HEADERS) && c->h1->resp.headers.size_remain())
        {
            poll_fd[num_wait].fd = c->clientSocket;
            poll_fd[num_wait].events = POLLOUT;
        }
        else if (c->h1->con_status == http1::SEND_ENTITY)
        {
            if (((c->h1->resp.source_data == DYN_PAGE) && c->h1->resp.send_data.size_remain()) ||
                 (c->h1->resp.source_data != DYN_PAGE)
            )
            {
                poll_fd[num_wait].fd = c->clientSocket;
                poll_fd[num_wait].events = POLLOUT;
            }
        }
        else if (c->h1->con_status == http1::SSL_SHUTDOWN)
        {
            if (c->tls.shutdown_timer == 0)
                c->tls.shutdown_timer = t;
            if ((t - c->tls.shutdown_timer) >= 3)
            {
                print_err(c, "<%s:%d> SSL_SHUTDOWN: timeout\n", __func__, __LINE__);
                close_connect(c);
                return;
            }
            else
            {
                poll_fd[num_wait].fd = c->clientSocket;
                poll_fd[num_wait].events = c->tls.poll_events;
            }
        }

        if (poll_fd[num_wait].events)
            conn_array[num_wait++] = c;
    }
}
//======================================================================
void EventHandlerClass::http2_set_poll(Connect *c)
{
    time_t t = time(NULL);
    int Timeout = conf->Timeout;
    if (c->client_timer == 0)
        c->client_timer = t;

    if (c->h2->con_status == http2::PROCESSING_REQUESTS)
    {
        if (conf->TimeoutKeepAlive > 0)
            Timeout = conf->TimeoutKeepAlive;
    }

    if ((t - c->client_timer) >= Timeout)
    {
        print_err(c, "<%s:%d> Timeout=%ld, %s\n", __func__, __LINE__, t - c->client_timer, c->h2->get_str_status());
        if (c->h2->con_status == http2::SSL_SHUTDOWN)
            close_connect(c);
        else
            ssl_shutdown(c);
    }
    else
    {
        if (conf->SecureConnect)
        {
            int ret = 0, pending = 0;
            while ((pending = SSL_pending(c->tls.ssl)) && (c->h2->con_status != http2::SEND_SETTINGS))
            {
                if (conf->PrintDebugMsg)
                {
                    print_err(c, "<%s:%d> ***** SSL_pending()=%d, %s\n", __func__, __LINE__, 
                                pending, c->h2->get_str_status());
                }

                if ((c->h2->con_status == http2::RECV_SETTINGS) || (c->h2->con_status == http2::PROCESSING_REQUESTS))
                {
                    if (c->h2->goaway.size())
                    {
                        print_err(c, "<%s:%d> c->h2->goaway.size() > 0, %s\n", __func__, __LINE__, c->h2->get_str_status());
                        break;
                    }

                    if ((ret = recv_frame(c)) < 0)
                        break;
                }
                else if ((c->h2->con_status == http2::PREFACE_MESSAGE) || (c->h2->con_status == http2::SSL_SHUTDOWN))
                {
                    if ((ret = http2_connection(c)) < 0)
                        break;
                }
            }

            if (ret < 0)
                return;
        }
        else
        {
            print_err(c, "<%s:%d> \"We are not here.\" - Putin\n", __func__, __LINE__);
            return;
        }
            
        if (c->h2->work_stream == NULL)
            c->h2->work_stream = c->h2->start_stream;

        poll_fd[num_wait].fd = c->clientSocket;
        poll_fd[num_wait].events = 0;
        if (c->h2->con_status == http2::SSL_SHUTDOWN)
        {
            if (c->tls.shutdown_timer == 0)
                c->tls.shutdown_timer = t;
            if ((t - c->tls.shutdown_timer) >= 3)
            {
                print_err(c, "<%s:%d> SSL_SHUTDOWN: timeout\n", __func__, __LINE__);
                close_connect(c);
                return;
            }
            else
                poll_fd[num_wait].events = c->tls.poll_events;
        }
        else if ((c->h2->con_status == http2::PREFACE_MESSAGE) || (c->h2->con_status == http2::RECV_SETTINGS))
            poll_fd[num_wait].events = POLLIN;
        else if (c->h2->con_status == http2::SEND_SETTINGS)
            poll_fd[num_wait].events = POLLOUT;
        else // con_status = PROCESSING_REQUESTS
        {
            if (c->h2->goaway.size() || c->h2->ping.size() || c->h2->start_list_send_frame || c->h2->try_again)
            {
                poll_fd[num_wait].events = POLLOUT;
                conn_array[num_wait++] = c;
                return;
            }

            if (c->h2->frame_win_update.size() || (c->h2->cgi_window_update > 0))
            {
                poll_fd[num_wait].events = POLLOUT;
                conn_array[num_wait++] = c;
                return;
            }

            poll_fd[num_wait].events = POLLIN;
            if (c->h2->work_stream)
            {
                if ((c->h2->connect_window_size <= 0) && c->h2->work_stream->send_headers)
                {
                    if (conf->PrintDebugMsg)
                        print_err(c, "<%s:%d> connect_window_size <= 0\n", __func__, __LINE__);
                    conn_array[num_wait++] = c;
                    return;
                }
            }

            Stream *resp = c->h2->work_stream, *resp_next = NULL;
            for ( ; resp; resp = resp_next)
            {
                resp_next = resp->next;
                if (resp_next == NULL)
                    resp_next = c->h2->start_stream;

                if (resp->frame_win_update.size() || 
                    resp->headers.size() || 
                    resp->send_data.size() || 
                    resp->rst_stream ||
                    (resp->send_headers && (resp->stream_window_size > 0))
                )
                {
                    poll_fd[num_wait].events |= POLLOUT;
                    c->h2->work_stream = resp;
                    break;
                }
                    
                if (resp_next == c->h2->work_stream)
                    break;
            }
        }

        if (poll_fd[num_wait].events)
            conn_array[num_wait++] = c;
    }
}
//======================================================================
int EventHandlerClass::_poll()
{
    if (num_wait == 0)
        return 0;
    int time_poll = conf->TimeoutPoll;
    if (all_cgi > 0)
        time_poll = 0;
    int ret_poll = poll(poll_fd, num_wait, time_poll);
    if (ret_poll == -1)
    {
        print_err("<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }
    else if (ret_poll == 0)
    {
        return 0;
    }

    for ( int i = 0; i < num_wait; ++i)
    {
        Connect *c = conn_array[i];
        if (poll_fd[i].revents == 0)
            continue;
        if (c->Protocol == P_HTTP2)
        {
            http2_poll(c, i);
        }
        else if (c->Protocol == P_HTTP1)
        {
            http1_poll(c, poll_fd[i].revents);
        }
        else
        {
            print_err(c, "<%s:%d> \"We are not here.\" - Putin\n", __func__, __LINE__);
        }
    }
    return 0;
}
//======================================================================
int EventHandlerClass::http1_poll(Connect *c, int revents)
{
    http1_worker(c, revents);
    return 0;
}
//======================================================================
int EventHandlerClass::http2_poll(Connect *c, int conn_ind)
{
    int revents = poll_fd[conn_ind].revents;
    if (revents & ((~POLLIN) & (~POLLOUT)))
    {
        print_err(c, "<%s:%d> !!! Error: events=0x%02x, revents=0x%02x, %s\n", __func__, __LINE__, poll_fd[conn_ind].events, revents, c->h2->get_str_status());
        if (c->h2->con_status == http2::SSL_SHUTDOWN)
        {
            close_connect(c);
        }
        else
            ssl_shutdown(c);
        return -1;
    }

    if (poll_fd[conn_ind].fd != c->clientSocket)
    {
        print_err(c, "<%s:%d> !!! Error: fd=%d/clientSocket=%d\n", __func__, __LINE__, poll_fd[conn_ind].fd, c->clientSocket);
        ssl_shutdown(c);
        return -1;
    }

    c->fd_revents = revents;
    if (poll_fd[conn_ind].revents & POLLIN)
    {
        if ((c->h2->con_status == http2::PREFACE_MESSAGE) || (c->h2->con_status == http2::SSL_SHUTDOWN))
        {
            if (http2_connection(c) < 0)
                return -1;
        }
        else if ((c->h2->con_status == http2::RECV_SETTINGS) || (c->h2->con_status == http2::PROCESSING_REQUESTS))
        {
            if (recv_frame(c) < 0)
                return -1;
        }
    }

    if (poll_fd[conn_ind].revents & POLLOUT)
    {
        if (c->h2->con_status == http2::SSL_SHUTDOWN)
        {
            http2_connection(c);
        }
        else if ((c->h2->con_status == http2::SEND_SETTINGS) || (c->h2->con_status == http2::PROCESSING_REQUESTS))
        {
            send_frames(c);
        }
    }

    return 0;
}
//======================================================================
void EventHandlerClass::close_connect(Connect *c)
{
    if (conf->PrintDebugMsg)
        print_err(c, "<%s:%d> Close connection\n", __func__, __LINE__);

    if (c->tls.ssl && conf->SecureConnect)
    {
        SSL_clear(c->tls.ssl);
        SSL_free(c->tls.ssl);
    }

    shutdown(c->clientSocket, SHUT_RDWR);
    if (close(c->clientSocket))
    {
        print_err(c, "<%s:%d> Error close(): %s\n", __func__, __LINE__, strerror(errno));
    }

    event_handler_cl.del_from_list(c);
    delete c;
    decrement_num_conn();
}
//======================================================================
void EventHandlerClass::ssl_shutdown(Connect *c)
{
    if (conf->PrintDebugMsg)
        print_err(c, "<%s:%d> ssl_shutdown\n", __func__, __LINE__);

    if (c->tls.ssl)
    {
        if ((c->tls.err != SSL_ERROR_SSL) && (c->tls.err != SSL_ERROR_SYSCALL))
        {
            if (c->Protocol == P_HTTP2)
                c->h2->con_status = http2::SSL_SHUTDOWN;
            else if (c->Protocol == P_HTTP1)
                c->h1->con_status = http1::SSL_SHUTDOWN;
            c->client_timer = 0;
            c->tls.shutdown_timer = 0;
            for ( int i = 0; i < 2; ++i)
            {
                int ret = SSL_shutdown(c->tls.ssl);
                if (conf->PrintDebugMsg)
                    print_err(c, "<%s:%d> SSL_shutdown()=%d\n", __func__, __LINE__, ret);
                if (ret == -1)
                {
                    c->tls.err = SSL_get_error(c->tls.ssl, ret);
                    if (conf->PrintDebugMsg)
                        print_err(c, "<%s:%d> Error SSL_shutdown()=%d: %s\n", __func__, __LINE__, ret, ssl_strerror(c->tls.err));
                    if (c->tls.err == SSL_ERROR_ZERO_RETURN)
                    {
                        close_connect(c);
                        return;
                    }
                    else if (c->tls.err == SSL_ERROR_WANT_READ)
                    {
                        c->tls.poll_events = POLLIN;
                        return;
                    }
                    else if (c->tls.err == SSL_ERROR_WANT_WRITE)
                    {
                        c->tls.poll_events = POLLOUT;
                        return;
                    }
                    else
                    {
                        break;
                    }
                }
                else if (ret == 0)
                    continue;
                else
                    break;
            }
        }
    }

    close_connect(c);
}
//======================================================================
int EventHandlerClass::wait_connection()
{
    {
    unique_lock<mutex> lk(mtx_thr);
        while ((!work_list_start) && (!wait_list_start) && (!close_thr))
        {
            cond_thr.wait(lk);
        }
    }

    if (close_thr)
        return 1;
    return 0;
}
//======================================================================
void EventHandlerClass::push_wait_list(Connect *c)
{
    c->client_timer = 0;
    c->next = NULL;
mtx_thr.lock();
    c->prev = wait_list_end;
    if (wait_list_start)
    {
        wait_list_end->next = c;
        wait_list_end = c;
    }
    else
        wait_list_start = wait_list_end = c;
mtx_thr.unlock();
    cond_thr.notify_one();
}
//======================================================================
void EventHandlerClass::close_connections()
{
    if (work_list_start)
    {
        Connect *c = work_list_start, *next = NULL;
        for ( ; c; c = next)
        {
            next = c->next;
            if (c->tls.ssl && conf->SecureConnect)
            {
                if((c->tls.err != SSL_ERROR_SSL) && (c->tls.err != SSL_ERROR_SYSCALL))
                    SSL_shutdown(c->tls.ssl);
                SSL_free(c->tls.ssl);
            }

            shutdown(c->clientSocket, SHUT_RDWR);
            close(c->clientSocket);
            delete c;
        }
    }

    if (wait_list_start)
    {
        Connect *c = wait_list_start, *next = NULL;
        for ( ; c; c = next)
        {
            next = c->next;
            if (c->tls.ssl && conf->SecureConnect)
            {
                if((c->tls.err != SSL_ERROR_SSL) && (c->tls.err != SSL_ERROR_SYSCALL))
                    SSL_shutdown(c->tls.ssl);
                SSL_free(c->tls.ssl);
            }

            shutdown(c->clientSocket, SHUT_RDWR);
            close(c->clientSocket);
            delete c;
        }
    }
}
//======================================================================
void EventHandlerClass::close_event_handler()
{
    close_thr = 1;
    cond_thr.notify_one();
}
//======================================================================
void EventHandlerClass::dec_all_cgi()
{
    --all_cgi;
}
//======================================================================
void event_handler()
{
    event_handler_cl.init();
printf(" +++++ worker thread run +++++\n");
    while (1)
    {
        if (event_handler_cl.wait_connection())
            break;
        event_handler_cl.add_work_list();

        if (event_handler_cl.cgi_poll() < 0)
            break;

        event_handler_cl.set_poll();

        if (event_handler_cl._poll() < 0)
            break;
    }

    event_handler_cl.close_connections();
    print_err("<%s:%d> ***** exit event_handler *****\n", __func__, __LINE__);
}
//======================================================================
void push_wait_list(Connect *c)
{
    event_handler_cl.push_wait_list(c);
}
//======================================================================
void close_work_thread()
{
    event_handler_cl.close_event_handler();
}
//======================================================================
void dec_all_cgi()
{
    event_handler_cl.dec_all_cgi();
}
