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
            kill(pid, SIGKILL);
    }

    if (errno)
        errno = 0;
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
            c->h1->resp.send_data.cat(buf, ret);
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
                if ((con->h1->con_status == http1::SEND_RESP_HEADERS) && 
                    (con->h1->resp.create_headers == false)
                )
                {
                    cgi_headers_parse(con);
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
void EventHandlerClass::cgi_headers_parse(Connect *c)
{
    char s[512];
    const char *p1 = c->h1->resp.send_data.ptr_remain();
    int data_size = c->h1->resp.send_data.size_remain();
    int header_len = -1;
    int j = 0;
    for (int i = 0; i < data_size; ++i)
    {
        char ch = *(p1++);
        if (ch == '\n')
        {
            header_len = j;
            s[j] = 0;
            j = 0;
            int offset = p1 - c->h1->resp.send_data.ptr_remain();
            if (c->h1->resp.send_data.set_offset(offset) < 0)
            {
                print_err(c, "<%s:%d> Error set_offset(%d)\n", __func__, __LINE__, offset);
                c->err = -RS502;
                http1_end_request(c);
                return;
            }

            if (header_len == 0)
                break;

            if (header_len >= (int)sizeof(s))
            {
                print_err(c, "<%s:%d> Error: data_size=%d, length header >= %d\n", __func__, __LINE__, data_size, (int)sizeof(s));
                c->err = -RS502;
                http1_end_request(c);
                return;
            }

            if (memchr(s, ':', header_len) == NULL)
            {
                print_err(c, "<%s:%d> Error: line is not header\n", __func__, __LINE__);
                c->err = -RS502;
                http1_end_request(c);
                return;
            }

            if (conf->PrintDebugMsg)
                print_err(c, "<%s:%d> [%s]\n", __func__, __LINE__, s);
            if (strstr_case(s, "Status:"))
            {
                sscanf(s + 7, "%d", &c->h1->resp.resp_status);
                if (c->h1->resp.resp_status == RS204)
                {
                    if (c->h1->resp.cgi_type <= PHPCGI)
                    {
                        if (c->h1->resp.cgi.from_script > 0)
                        {
                            if (c->h1->resp.cgi.from_script > 0)
                            {
                                close(c->h1->resp.cgi.from_script);
                                c->h1->resp.cgi.from_script = -1;
                            }
                        }
                    }
                    else
                    {
                        if (c->h1->resp.cgi.fd > 0)
                        {
                            close(c->h1->resp.cgi.fd);
                            c->h1->resp.cgi.fd = -1;
                        }
                    }

                    c->h1->chunk_mode = NO_CHUNK;
                    c->h1->resp.resp_content_len = 0;
                    if (create_response_headers(c) < 0)
                    {
                        c->err = -1;
                        http1_end_request(c);
                    }
                    else
                    {
                        c->h1->resp.create_headers = true;
                    }
                    return;
                }
            }
            else
            {
                c->h1->hdrs << s << "\r\n";
            }

            header_len = -1;
        }
        else if (ch != '\r')
        {
            s[j++] = ch;
        }
    }

    if (header_len == -1)
    {
        return;
    }

    if (c->h1->resp.send_data.size_remain() == 0)
    {
        c->h1->resp.send_data.init();
    }
    else
    {
        if (c->h1->chunk_mode == CHUNK)
        {
            if (c->h1->resp.send_data.set_offset(-8) < 0)
            {
                print_err(c, "<%s:%d> Error set_offset(-8)\n", __func__, __LINE__);
                c->err = -RS502;
                http1_end_request(c);
                return;
            }

            int ret = cgi_set_size_chunk(&c->h1->resp.send_data);
            if (ret < 0)
            {
                print_err(c, "<%s:%d> Error cgi_set_size_chunk()\n", __func__, __LINE__);
                c->err = -RS502;
                http1_end_request(c);
                return;
            }
        }
    }

    if (c->h1->resp.resp_status == 0)
        c->h1->resp.resp_status = RS200;
    if (c->h1->resp.httpMethod == M_HEAD)
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
}
