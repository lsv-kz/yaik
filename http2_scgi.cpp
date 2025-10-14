#include "main.h"

using namespace std;
//======================================================================
static int scgi_create_params(Connect *c, Stream *resp);
static int scgi_set_param(Stream *r);
static int scgi_set_size_data(Stream *r);
//======================================================================
static int scgi_set_size_data(Stream *resp)
{
    resp->cgi.buf_param.set_byte(':', 7);
    int i = 6;
    int size = resp->cgi.buf_param.size() - 8;
    for ( ; i >= 0; --i)
    {
        resp->cgi.buf_param.set_byte((size % 10) + '0', i);
        size /= 10;
        if (size == 0)
            break;
    }

    if (size != 0)
        return -1;

    resp->cgi.buf_param.cat(",", 1);
    resp->cgi.buf_param.set_offset(i);

    return 0;
}
//======================================================================
int scgi_create_connect(Connect *con, Stream *resp)
{
    resp->cgi.fd = create_fcgi_socket(resp->cgi.socket->c_str());
    if (resp->cgi.fd < 0)
    {
        print_err(resp, "<%s:%d> Error connect to scgi\n", __func__, __LINE__);
        return resp->cgi.fd;
    }

    int ret = scgi_create_params(con, resp);
    if (ret < 0)
        return ret;
    else
        resp->cgi.timer = 0;
    return 0;
}
//======================================================================
int scgi_create_params(Connect *c, Stream *resp)
{
    int i = 0;
    Param param;
    resp->cgi.vPar.clear();

    if (resp->httpMethod == M_POST)
    {
        param.name = "CONTENT_LENGTH";
        if (resp->sReqContentLen.size())
            param.val = resp->sReqContentLen;
        else
            param.val = "0";
        resp->cgi.vPar.push_back(param);
        ++i;

        param.name = "CONTENT_TYPE";
        if (resp->sReqContentType.size())
            param.val = resp->sReqContentType.c_str();
        else
            param.val = "";
        resp->cgi.vPar.push_back(param);
        ++i;
    }
    else
    {
        param.name = "CONTENT_LENGTH";
        param.val = "0";
        resp->cgi.vPar.push_back(param);
        ++i;

        param.name = "CONTENT_TYPE";
        param.val = "";
        resp->cgi.vPar.push_back(param);
        ++i;
    }

    param.name = "PATH";
    param.val = "/bin:/usr/bin:/usr/local/bin";
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "SERVER_SOFTWARE";
    param.val = conf->ServerSoftware;
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "SCGI";
    param.val = "1";
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "DOCUMENT_ROOT";
    param.val = conf->DocumentRoot;
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "REMOTE_ADDR";
    param.val = c->remoteAddr;
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "REMOTE_PORT";
    param.val = c->remotePort;
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "REQUEST_URI";
    param.val = resp->path;
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "DOCUMENT_URI";
    param.val = resp->clean_decode_path;
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "REQUEST_METHOD";
    param.val = get_str_method(resp->httpMethod);
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "SERVER_PROTOCOL";
    param.val = "HTTP2";
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "SERVER_PORT";
    param.val = conf->ServerPort;
    resp->cgi.vPar.push_back(param);
    ++i;

    if (resp->host.size())
    {
        param.name = "HTTP_HOST";
        param.val = resp->host;
        resp->cgi.vPar.push_back(param);
        ++i;
    }

    if (resp->referer.size())
    {
        param.name = "HTTP_REFERER";
        param.val = resp->referer;
        resp->cgi.vPar.push_back(param);
        ++i;
    }

    if (resp->user_agent.size())
    {
        param.name = "HTTP_USER_AGENT";
        param.val = resp->user_agent;
        resp->cgi.vPar.push_back(param);
        ++i;
    }

    param.name = "SCRIPT_NAME";
    param.val = resp->cgi.scriptName;
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "QUERY_STRING";
    if (resp->query_string.size())
        param.val = resp->query_string;
    else
        param.val = "";
    resp->cgi.vPar.push_back(param);
    ++i;

    if (i != (int)resp->cgi.vPar.size())
    {
        print_err(resp, "<%s:%d> Error: create scgi param list\n", __func__, __LINE__);
        return -1;
    }

    resp->cgi.size_par = i;
    resp->cgi.i_param = 0;

    int ret = scgi_set_param(resp);
    if (ret <= 0)
    {
        print_err(resp, "<%s:%d> Error scgi_set_param()\n", __func__, __LINE__);
        return -RS502;
    }

    return 0;
}
//======================================================================
int scgi_set_param(Stream *resp)
{
    resp->cgi.buf_param.cpy("\0\0\0\0\0\0\0\0", 8);
    for ( ; resp->cgi.i_param < resp->cgi.size_par; ++resp->cgi.i_param)
    {
        int len_name = resp->cgi.vPar[resp->cgi.i_param].name.size();
        if (len_name == 0)
        {
            print_err(resp, "<%s:%d> Error: len_name=0\n", __func__, __LINE__);
            return -RS502;
        }

        int len_val = resp->cgi.vPar[resp->cgi.i_param].val.size();
        int len = len_name + len_val + 2;

        if ((len + resp->cgi.buf_param.size()) > 16000)
        {
            break;
        }

        resp->cgi.buf_param.cat(resp->cgi.vPar[resp->cgi.i_param].name.c_str(), len_name);
        resp ->cgi.buf_param.cat("\0", 1);

        if (len_val > 0)
        {
            resp->cgi.buf_param.cat(resp->cgi.vPar[resp->cgi.i_param].val.c_str(), len_val);
        }

        resp->cgi.buf_param.cat("\0", 1);
    }

    if(resp->cgi.i_param < resp->cgi.size_par)
    {
        print_err(resp, "<%s:%d> Error: size of param > size of buf\n", __func__, __LINE__);
        return -RS502;
    }

    if (resp->cgi.buf_param.size())
    {
        scgi_set_size_data(resp);
    }
    else
    {
        print_err(resp, "<%s:%d> Error: size param = 0\n", __func__, __LINE__);
        return -RS502;
    }

    return resp->cgi.buf_param.size();
}
//======================================================================
void EventHandlerClass::scgi_worker(Connect* con, Stream *resp, struct pollfd *poll_fd)
{
    int revents = poll_fd->revents;
    if (resp->cgi_status == SCGI_PARAMS)
    {
        if (revents == POLLOUT)
        {
            int ret = write_to_fcgi(resp->cgi.fd, resp->cgi.buf_param.ptr_remain(), resp->cgi.buf_param.size_remain());
            if (ret < 0)
            {
                if (ret != ERR_TRY_AGAIN)
                    resp_502(resp);
                return;
            }

            resp->cgi.timer = 0;
            resp->cgi.buf_param.set_offset(ret);
            if (resp->cgi.buf_param.size_remain() == 0)
            {
                resp->cgi.buf_param.init();
                if (resp->httpMethod == M_POST)
                {
                    if(resp->post_content_len <= 0)
                        resp->cgi_status = CGI_STDOUT;
                    else
                        resp->cgi_status = CGI_STDIN;
                }
                else
                {
                    resp->post_data.init();
                    resp->cgi_status = CGI_STDOUT;
                }
            }
        }
        else
        {
            print_err(resp, "<%s:%d> Error 0x%02X(0x%02X), fd=%d\n", __func__, __LINE__, poll_fd->revents, poll_fd->events, poll_fd->fd);
            resp_502(resp);
        }
    }
}
