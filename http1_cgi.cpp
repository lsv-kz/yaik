#include "main.h"

using namespace std;
//======================================================================
const char *get_script_name(const char *name);
const char *base_name(const char *path);
//======================================================================
void kill_chld(pid_t pid)
{
    if (pid > 0)
    {
        if (waitpid(pid, NULL, WNOHANG) == 0)
        {
            if (kill(pid, SIGKILL) == 0)
                waitpid(pid, NULL, 0);
            else
                print_err("<%s:%d> Error kill(): %s\n", __func__, __LINE__, strerror(errno));
        }
    }
}
//======================================================================
const char *get_script_name(const char *name)
{
    const char *p;
    if (!name)
        return "";

    if ((p = strchr(name + 1, '/')))
        return p;

    return "";
}
//======================================================================
const char *base_name(const char *path)
{
    const char *p;

    if (!path)
        return NULL;

    p = strrchr(path, '/');
    if (p)
        return p + 1;

    return path;
}
//======================================================================
int cgi_set_size_chunk(ByteArray *ba)
{
    const char *hex = "0123456789ABCDEF";
    int size = ba->size_remain() - 8;
    int i = 7;
    ba->set_byte('\n', ba->get_offset() + i);
    --i;
    ba->set_byte('\r', ba->get_offset() + i);
    --i;

    for ( ; i >= 0; --i)
    {
        ba->set_byte(hex[size % 16], ba->get_offset() + i);
        size /= 16;
        if (size == 0)
            break;
    }

    if (size != 0)
        return -1;

    ba->set_offset(i);
    ba->cat("\r\n", 2);

    return 0;
}
//======================================================================
int EventHandlerClass::cgi_stdout(Connect *c, int fd)
{
    char buf[16384];
    if (c->h1->resp.create_headers && c->h1->resp.send_data.size())
        return c->h1->resp.send_data.size();

    int ret = read(fd, buf, sizeof(buf));
    if (ret == -1)
    {
        print_err(c, "<%s:%d> Error read(): %s\n", __func__, __LINE__, strerror(errno));
        if (errno == EAGAIN)
            return ERR_TRY_AGAIN;
        return -1;
    }
    else if (ret > 0)
    {
        c->h1->resp.cgi.timer = 0;

        if (c->h1->resp.create_headers == false)
        {
            buf[ret] = 0;
            c->h1->resp.buf.cat(buf, ret);
        }
        else
        {
            if (c->h1->chunk_mode == CHUNK)
            {
                c->h1->resp.send_data.cpy("01234567", 8);
                c->h1->resp.send_data.cat(buf, ret);
                int ret = cgi_set_size_chunk(&c->h1->resp.send_data);
                if (ret < 0)
                {
                    print_err(c, "<%s:%d> Error cgi_set_size_chunk()\n", __func__, __LINE__);
                    return -1;
                }
            }
            else
                c->h1->resp.send_data.cat(buf, ret);
        }
    }

    return ret;
}
//======================================================================
void EventHandlerClass::cgi_worker(Connect *con, int cgi_ind_poll)
{
    int revents = poll_fd[cgi_ind_poll].revents;
    int events = poll_fd[cgi_ind_poll].revents;
    int fd = poll_fd[cgi_ind_poll].fd;

    if (con->h1->resp.cgi_status == CGI_STDIN)
    {
        if (con->h1->resp.cgi_type <= PHPCGI)
        {
            if (con->h1->resp.cgi.to_script != fd)
            {
                print_err(con, "<%s:%d> Error cgi.to_script=%d, fd=%d\n", __func__, __LINE__,
                                        con->h1->resp.cgi.to_script, fd);
                con->err = -RS500;
                http1_end_request(con);
                return;
            }
        }
        else
        {
            if (con->h1->resp.cgi.fd != fd)
            {
                print_err(con, "<%s:%d> Error cgi.fd=%d, fd=%d, 0x%02X\n", __func__, __LINE__,
                                        con->h1->resp.cgi.fd, fd, revents);
                con->err = -RS502;
                http1_end_request(con);
                return;
            }
        }

        if (revents == POLLOUT)
        {
            int ret = cgi_stdin(&con->h1->resp, fd);
            if (ret == ERR_TRY_AGAIN)
            {
                print_err(con, "<%s:%d> Error cgi_stdin ERR_TRY_AGAIN\n", __func__, __LINE__);
                return;
            }
            else if (ret < 0)
            {
                print_err(con, "<%s:%d> Error cgi_stdin()=%d\n", __func__, __LINE__, ret);
                con->err = -RS502;
                http1_end_request(con);
                return;
            }
            else
            {
                if (con->h1->resp.post_content_len <= 0)
                    con->h1->con_status = http1::SEND_RESP_HEADERS;
            }
        }
        else if (revents)
        {
            print_err(con, "<%s:%d> Error events/revents=0x%02X/0x%02X, fd=%d\n", __func__, __LINE__,
                    events, revents, fd);
            con->err = -RS502;
            http1_end_request(con);
        }
    }
    else if (con->h1->resp.cgi_status == CGI_STDOUT)
    {
        if (con->h1->resp.cgi_type <= PHPCGI)
        {
            if (con->h1->resp.cgi.from_script != fd)
            {
                print_err(con, "<%s:%d> Error cgi.from_script=%d, fd=%d, 0x%02X\n", __func__, __LINE__,
                                        con->h1->resp.cgi.from_script, fd, revents);
                con->err = -RS502;
                http1_end_request(con);
                return;
            }
        }
        else
        {
            if (con->h1->resp.cgi.fd != fd)
            {
                print_err(con, "<%s:%d> Error cgi.fd=%d, fd=%d, 0x%02X\n", __func__, __LINE__,
                                        con->h1->resp.cgi.fd, fd, revents);
                con->err = -RS502;
                http1_end_request(con);
                return;
            }
        }

        if (revents & POLLIN)
        {
            if (con->h1->resp.send_data.size() >= conf->HTTP1_DataBufSize)
            {
                return;
            }

            int ret = cgi_stdout(con, fd);
            if (ret == ERR_TRY_AGAIN)
            {
                print_err(con, "<%s:%d> cgi_stdout()=ERR_TRY_AGAIN\n", __func__, __LINE__);
            }
            else if (ret < 0)
            {
                print_err(con, "<%s:%d> Error cgi_stdout()=%d\n", __func__, __LINE__, ret);
                con->err = -RS502;
                http1_end_request(con);
            }
            else if (ret == 0)
            {
                if (con->h1->resp.cgi_type <= PHPCGI)
                {
                    if (con->h1->resp.cgi.from_script > 0)
                    {
                        close(con->h1->resp.cgi.from_script);
                        con->h1->resp.cgi.from_script = -1;
                    }
                }

                con->h1->resp.cgi.end = true;
                if (con->h1->resp.create_headers)
                {
                    if (con->h1->chunk_mode == CHUNK)
                    {
                        char s[] = "0\r\n\r\n";
                        con->h1->resp.send_data.cat_str(s);
                    }

                    if (con->h1->resp.send_data.size() == 0)
                    {
                        http1_end_request(con);
                    }
                }
                else
                {
                    print_err(con, "<%s:%d> Error: empty line not found\n", __func__, __LINE__);
                    con->err = -RS502;
                    http1_end_request(con);
                }
            }
            else
            {
                if (con->h1->resp.create_headers == false)
                {
                    http1_get_cgi_headers(con);
                }
            }
        }
        else if (revents)
        {
            if ((con->h1->resp.headers.size_remain() || con->h1->resp.send_data.size_remain()) && con->h1->resp.create_headers)
            {
                return;
            }

            if (con->h1->resp.cgi_type <= PHPCGI)
            {
                if (con->h1->resp.cgi.from_script > 0)
                {
                    close(con->h1->resp.cgi.from_script);
                    con->h1->resp.cgi.from_script = -1;
                }
            }
            else if (con->h1->resp.cgi_type == SCGI)
            {
                if (con->h1->resp.cgi.fd > 0)
                {
                    close(con->h1->resp.cgi.fd);
                    con->h1->resp.cgi.fd = -1;
                }
            }

            con->h1->resp.cgi.end = true;
            if (con->h1->resp.send_headers == false)
            {
                print_err(con, "<%s:%d> Error 502 Bad Gateway (revents=0x%02X)\n", __func__, __LINE__, revents);
                con->err = -RS502;
                http1_end_request(con);
                return;
            }

            if (con->h1->chunk_mode == CHUNK)
            {
                char s[] = "0\r\n\r\n";
                con->h1->resp.send_data.cpy(s, 5);
            }
            else
            {
                con->h1->resp.send_data.init();
                http1_end_request(con);
            }
        }
    }
}
//======================================================================
void EventHandlerClass::http1_get_cgi_headers(Connect *c)
{
    int ret = cgi_parse_headers(c, &c->h1->resp, false);
    if (ret < 0)
    {
        print_err(c, "<%s:%d> Error cgi_parse_headers() = -1\n", __func__, __LINE__);
        c->err = -RS502;
        http1_end_request(c);
        return;
    }
    else if (ret == 0)
    {
        print_err(c, "<%s:%d> cgi_parse_headers() = 0\n", __func__, __LINE__);
        return;
    }
    
    for (unsigned int i = 0; i < c->h1->resp.cgi.vPar.size(); ++i)
    {
        Param header;
        header = c->h1->resp.cgi.vPar[i];
        if (header.name == "Status")
            continue;
        /*else if (header.name == "Content-Type")
        {
            
        }
        else if (header.name == "Location")
        {
            
        }
        else*/
        {
            c->h1->hdrs.cat_str(header.name.c_str());
            c->h1->hdrs.cat(": ", 2);
            c->h1->hdrs.cat_str(header.val.c_str());
            c->h1->hdrs.cat("\r\n", 2);
        }
    }

    if (c->h1->resp.resp_status == 0)
        c->h1->resp.resp_status = RS200;
    if ((c->h1->resp.httpMethod == M_HEAD) || (c->h1->resp.resp_status == RS204))
    {
        c->h1->resp.cgi.end = true;
        c->h1->resp.send_data.init();
    }
    c->h1->resp.resp_content_len = -1;
    if (create_response_headers(c) < 0)
    {
        c->err = -1;
        http1_end_request(c);
    }
    else
    {
        c->h1->resp.create_headers = true;
    }

    if (c->h1->resp.buf.size_remain() > 0)
    {
        if (c->h1->chunk_mode == CHUNK)
        {
            c->h1->resp.send_data.cpy("01234567", 8);
            c->h1->resp.send_data.cat(c->h1->resp.buf.ptr_remain(), c->h1->resp.buf.size_remain());
            int ret = cgi_set_size_chunk(&c->h1->resp.send_data);
            if (ret < 0)
            {
                print_err(c, "<%s:%d> Error cgi_set_size_chunk()\n", __func__, __LINE__);
                c->err = -RS502;
                http1_end_request(c);
                return;
            }
        }
        else
            c->h1->resp.send_data.cpy(c->h1->resp.buf.ptr_remain(), c->h1->resp.buf.size_remain());
    }
}
