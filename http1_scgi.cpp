#include "main.h"

using namespace std;
//======================================================================
static int scgi_create_params(Connect *c);
static int scgi_set_param(Connect *c);
//======================================================================
static int scgi_set_size_data(ByteArray *ba)
{
    ba->set_byte(':', 7);
    int i = 6;
    int size = ba->size() - 8;
    for ( ; i >= 0; --i)
    {
        ba->set_byte((size % 10) + '0', i);
        size /= 10;
        if (size == 0)
            break;
    }

    if (size != 0)
        return -1;

    ba->cat(",", 1);
    ba->set_offset(i);

    return 0;
}
//======================================================================
int scgi_create_connect(Connect *c)
{
    c->h1->resp.cgi.fd = create_fcgi_socket(c->h1->resp.cgi.socket->c_str());
    if (c->h1->resp.cgi.fd < 0)
    {
        print_err(c, "<%s:%d> Error connect to scgi\n", __func__, __LINE__);
        return c->h1->resp.cgi.fd;
    }

    int ret = scgi_create_params(c);
    if (ret < 0)
        return ret;
    else
    {
        c->h1->resp.cgi.timer = 0;
        c->client_timer = 0;
        c->h1->resp.send_data.init();
        int opt = 1;
        ioctl(c->h1->resp.cgi.fd, FIONBIO, &opt);
    }
    return 0;
}
//======================================================================
int scgi_create_params(Connect *c)
{
    int i = 0;
    Param param;
    c->h1->resp.cgi.vPar.clear();
    if (c->h1->resp.cgi.vPar.capacity() < 50)
        c->h1->resp.cgi.vPar.reserve(50);

    if (c->h1->resp.httpMethod == M_POST)
    {
        param.name = "CONTENT_LENGTH";
        if (c->h1->resp.sReqContentLen.size())
            param.val = c->h1->resp.sReqContentLen;
        else
            param.val = "0";
        c->h1->resp.cgi.vPar.push_back(param);
        ++i;

        param.name = "CONTENT_TYPE";
        if (c->h1->resp.sReqContentType.size())
            param.val = c->h1->resp.sReqContentType;
        else
            param.val = "";
        c->h1->resp.cgi.vPar.push_back(param);
        ++i;
    }
    else
    {
        param.name = "CONTENT_LENGTH";
        param.val = "0";
        c->h1->resp.cgi.vPar.push_back(param);
        ++i;

        param.name = "CONTENT_TYPE";
        param.val = "";
        c->h1->resp.cgi.vPar.push_back(param);
        ++i;
    }

    param.name = "PATH";
    param.val = "/bin:/usr/bin:/usr/local/bin";
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "SERVER_SOFTWARE";
    param.val = conf->ServerSoftware;
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "SCGI";
    param.val = "1";
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "DOCUMENT_ROOT";
    param.val = conf->DocumentRoot;
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "REMOTE_ADDR";
    param.val = c->remoteAddr;
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "REMOTE_PORT";
    param.val = c->remotePort;
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "REQUEST_URI";
    param.val = c->h1->resp.path;
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "DOCUMENT_URI";
    param.val = c->h1->resp.clean_decode_path;
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "REQUEST_METHOD";
    param.val = get_str_method(c->h1->resp.httpMethod);
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "SERVER_PROTOCOL";
    param.val = "HTTP/1.1";
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "SERVER_PORT";
    param.val = conf->ServerPort;
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    if (c->h1->resp.referer.size())
    {
        param.name = "HTTP_REFERER";
        param.val = c->h1->resp.referer;
        c->h1->resp.cgi.vPar.push_back(param);
        ++i;
    }

    if (c->h1->resp.user_agent.size())
    {
        param.name = "HTTP_USER_AGENT";
        param.val = c->h1->resp.user_agent;
        c->h1->resp.cgi.vPar.push_back(param);
        ++i;
    }

    param.name = "SCRIPT_NAME";
    param.val = c->h1->resp.cgi.scriptName;
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    param.name = "QUERY_STRING";
    if (c->h1->resp.query_string.size())
        param.val = c->h1->resp.query_string;
    else
        param.val = "";
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    if (i != (int)c->h1->resp.cgi.vPar.size())
    {
        print_err(c, "<%s:%d> Error: create scgi param list\n", __func__, __LINE__);
        return -1;
    }

    c->h1->resp.cgi.size_par = i;
    c->h1->resp.cgi.i_param = 0;

    int ret = scgi_set_param(c);
    if (ret <= 0)
    {
        print_err(c, "<%s:%d> Error scgi_set_param()\n", __func__, __LINE__);
        return -RS502;
    }

    return 0;
}
//======================================================================
int scgi_set_param(Connect *c)
{
    c->h1->resp.cgi.buf_param.cpy("\0\0\0\0\0\0\0\0", 8);
    for ( ; c->h1->resp.cgi.i_param < c->h1->resp.cgi.size_par; ++c->h1->resp.cgi.i_param)
    {
        int len_name = c->h1->resp.cgi.vPar[c->h1->resp.cgi.i_param].name.size();
        if (len_name == 0)
        {
            print_err(c, "<%s:%d> Error: len_name=0\n", __func__, __LINE__);
            return -RS502;
        }

        int len_val = c->h1->resp.cgi.vPar[c->h1->resp.cgi.i_param].val.size();
        int len = len_name + len_val + 2;

        if ((len + c->h1->resp.cgi.buf_param.size()) > 16000)
        {
            break;
        }

        c->h1->resp.cgi.buf_param.cat(c->h1->resp.cgi.vPar[c->h1->resp.cgi.i_param].name.c_str(), len_name);
        c ->h1->resp.cgi.buf_param.cat("\0", 1);

        if (len_val > 0)
        {
            c->h1->resp.cgi.buf_param.cat(c->h1->resp.cgi.vPar[c->h1->resp.cgi.i_param].val.c_str(), len_val);
        }

        c->h1->resp.cgi.buf_param.cat("\0", 1);
    }

    if (c->h1->resp.cgi.i_param < c->h1->resp.cgi.size_par)
    {
        print_err(c, "<%s:%d> Error: size of param > size of buf\n", __func__, __LINE__);
        return -RS502;
    }

    if (c->h1->resp.cgi.buf_param.size())
    {
        scgi_set_size_data(&c->h1->resp.cgi.buf_param);
    }
    else
    {
        print_err(c, "<%s:%d> Error: size param = 0\n", __func__, __LINE__);
        return -RS502;
    }

    return c->h1->resp.cgi.buf_param.size();
}
//======================================================================
void EventHandlerClass::scgi_worker(Connect* c, struct pollfd *poll_fd)
{
    int revents = poll_fd->revents;
    if (c->h1->resp.cgi_status == SCGI_PARAMS)
    {
        if (revents == POLLOUT)
        {
            int ret = write_to_fcgi(c->h1->resp.cgi.fd, c->h1->resp.cgi.buf_param.ptr_remain(), c->h1->resp.cgi.buf_param.size_remain());
            if (ret < 0)
            {
                if (ret != ERR_TRY_AGAIN)
                {
                    c->err = -RS502;
                    http1_end_request(c);
                }
                return;
            }

            c->h1->resp.cgi.timer = 0;
            c->h1->resp.cgi.buf_param.set_offset(ret);
            if (c->h1->resp.cgi.buf_param.size_remain() == 0)
            {
                c->h1->resp.cgi.buf_param.init();
                if (c->h1->resp.httpMethod == M_POST)
                {
                    if (c->h1->resp.post_content_len <= 0)
                        c->h1->resp.cgi_status = CGI_STDOUT;
                    else
                        c->h1->resp.cgi_status = CGI_STDIN;
                }
                else
                {
                    c->h1->resp.post_data.init();
                    c->h1->resp.cgi_status = CGI_STDOUT;
                }
            }
        }
        else
        {
            print_err(c, "<%s:%d> Error 0x%02X(0x%02X), fd=%d\n", __func__, __LINE__, poll_fd->revents, poll_fd->events, poll_fd->fd);
            c->err = -RS502;
            http1_end_request(c);
        }
    }
}
