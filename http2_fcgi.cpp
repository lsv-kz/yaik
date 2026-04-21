#include "main.h"

using namespace std;
//======================================================================
void fcgi_set_header(ByteArray* ba, unsigned char type)
{
    int dataLen = ba->size() - 8;

    ba->set_byte(FCGI_VERSION_1, 0);
    ba->set_byte((unsigned char)type, 1);
    ba->set_byte((unsigned char) ((1 >> 8) & 0xff), 2);
    ba->set_byte((unsigned char) ((1) & 0xff), 3);
    ba->set_byte((unsigned char) ((dataLen >> 8) & 0xff), 4);
    ba->set_byte((unsigned char) ((dataLen) & 0xff), 5);
    ba->set_byte(0, 6);
    ba->set_byte(0, 7);
}
//======================================================================
void fcgi_set_header(char *s, unsigned char type, int dataLen)
{
    s[0] = (unsigned char)FCGI_VERSION_1;
    s[1] = (unsigned char)type;
    s[2] = (unsigned char)((1 >> 8) & 0xff);
    s[3] = (unsigned char)((1) & 0xff);
    s[4] = (unsigned char)((dataLen >> 8) & 0xff);
    s[5] = (unsigned char)((dataLen) & 0xff);
    s[6] = 0;
    s[7] = 0;
}
//======================================================================
void fcgi_set_param(Stream *resp)
{
    resp->cgi.buf_param.reserve(4096);
    resp->cgi.buf_param.cpy("\0\0\0\0\0\0\0\0", 8);

    for ( ; resp->cgi.i_param < resp->cgi.size_par; ++resp->cgi.i_param)
    {
        int len_name = resp->cgi.vPar[resp->cgi.i_param].name.size();
        int len_val = resp->cgi.vPar[resp->cgi.i_param].val.size();

        char s[8], *p = s;
        int i = 0;

        if (len_name < 0x80)
        {
            *(p++) = (unsigned char)len_name;
            ++i;
        }
        else
        {
            *(p++) = (unsigned char)((len_name >> 24) | 0x80);
            *(p++) = (unsigned char)(len_name >> 16);
            *(p++) = (unsigned char)(len_name >> 8);
            *(p++) = (unsigned char)len_name;
            i += 4;
        }

        if (len_val < 0x80)
        {
            *(p++) = (unsigned char)len_val;
            ++i;
        }
        else
        {
            *(p++) = (unsigned char)((len_val >> 24) | 0x80);
            *(p++) = (unsigned char)(len_val >> 16);
            *(p++) = (unsigned char)(len_val >> 8);
            *(p++) = (unsigned char)len_val;
            i += 4;
        }

        resp->cgi.buf_param.cat(s, i);
        resp->cgi.buf_param.cat(resp->cgi.vPar[resp->cgi.i_param].name.c_str(), len_name);
        if (len_val > 0)
        {
            resp->cgi.buf_param.cat(resp->cgi.vPar[resp->cgi.i_param].val.c_str(), len_val);
        }
    }

    fcgi_set_header(&resp->cgi.buf_param, FCGI_PARAMS);
}
//======================================================================
int fcgi_create_params(Connect *c, Stream *resp)
{
    int i = 0;
    Param param;
    resp->cgi.vPar.clear();
    if (resp->cgi.vPar.capacity() < 50)
        resp->cgi.vPar.reserve(50);

    if (resp->cgi_type == PHPFPM)
    {
        param.name = "REDIRECT_STATUS";
        param.val = "true";
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

    param.name = "GATEWAY_INTERFACE";
    param.val = "CGI/1.1";
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "DOCUMENT_ROOT";
    param.val = resp->vhost->DocumentRoot;
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "DOCUMENT_URI";
    param.val = resp->clean_decode_path;
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "REQUEST_URI";
    param.val = resp->path;
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

    param.name = "REQUEST_METHOD";
    param.val = get_str_method(resp->httpMethod);
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "SERVER_PROTOCOL";
    if (c->Protocol == P_HTTP2)
        param.val = "HTTP/2.0";
    else
        param.val = "HTTP/1.1";
    resp->cgi.vPar.push_back(param);
    ++i;

    param.name = "SERVER_PORT";
    param.val = c->ServerPort;
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
    param.val = resp->clean_decode_path;
    resp->cgi.vPar.push_back(param);
    ++i;

    if (resp->cgi_type == PHPFPM)
    {
        param.name = "SCRIPT_FILENAME";
        param.val = conf->DocumentRoot + resp->clean_decode_path;
        resp->cgi.vPar.push_back(param);
        ++i;
    }

    if (resp->httpMethod == M_POST)
    {
        if (resp->sReqContentType.size())
        {
            param.name = "CONTENT_TYPE";
            param.val = resp->sReqContentType.c_str();
            resp->cgi.vPar.push_back(param);
            ++i;
        }

        if (resp->sReqContentLen.size())
        {
            param.name = "CONTENT_LENGTH";
            param.val = resp->sReqContentLen;
            resp->cgi.vPar.push_back(param);
            ++i;
        }
    }

    param.name = "QUERY_STRING";
    param.val = resp->query_string;
    resp->cgi.vPar.push_back(param);
    ++i;

    if (i != (int)resp->cgi.vPar.size())
    {
        print_err(resp, "<%s:%d> Error: create fcgi param list\n", __func__, __LINE__);
        return -RS500;
    }

    resp->cgi.size_par = i;
    resp->cgi.i_param = 0;
    resp->cgi.timer = 0;

    return 0;
}
//======================================================================
int fcgi_create_connect(Connect *c, Stream *resp)
{
    if ((resp->cgi_type != PHPFPM) && (resp->cgi_type != FASTCGI))
    {
        print_err(resp, "<%s:%d> ? req->scriptType=%d \n", __func__, __LINE__, resp->cgi_type);
        return -1;
    }

    if (resp->cgi_type == PHPFPM)
        resp->cgi.socket = &conf->PathPHP;

    resp->cgi.fd = create_fcgi_socket(resp->cgi.socket->c_str());
    if (resp->cgi.fd < 0)
    {
        print_err(resp, "<%s:%d> Error connect to fcgi\n", __func__, __LINE__);
        return -1;
    }

    char s[16];
    s[0] = FCGI_VERSION_1;
    s[1] = FCGI_BEGIN_REQUEST;
    s[2] = (unsigned char) ((1 >> 8) & 0xff);
    s[3] = (unsigned char) ((1) & 0xff);
    s[4] = (unsigned char) ((8 >> 8) & 0xff);
    s[5] = (unsigned char) ((8) & 0xff);
    s[6] = 0;
    s[7] = 0;

    s[8] = (unsigned char) ((FCGI_RESPONDER >> 8) & 0xff);
    s[9] = (unsigned char) (FCGI_RESPONDER        & 0xff);
    s[10] = (unsigned char) 0;
    memset(s + 11, 0, 5);
    resp->cgi.buf_param.cpy(s, 16);

    int ret = fcgi_create_params(c, resp);
    if (ret < 0)
        return -1;
    resp->cgi_status = FASTCGI_BEGIN;
    return 0;
}
//======================================================================
void EventHandlerClass::fcgi_worker(Connect* c, Stream *resp, int cgi_ind_poll)
{
    int revents = poll_fd[cgi_ind_poll].revents;
    int events = poll_fd[cgi_ind_poll].revents;
    int fd = poll_fd[cgi_ind_poll].fd;

    if (resp->cgi_status == FASTCGI_BEGIN)
    {
        if (revents == POLLOUT)
        {
            int ret = write_to_fcgi(resp->cgi.fd, resp->cgi.buf_param.ptr_remain(), resp->cgi.buf_param.size_remain());
            if (ret < 0)
            {
                if (ret != ERR_TRY_AGAIN)
                    set_error_message(c, resp, RS502);
                return;
            }

            resp->cgi.timer = 0;
            resp->cgi.buf_param.set_offset(ret);
            if (resp->cgi.buf_param.size_remain() == 0)
            {
                resp->cgi.buf_param.init();
                resp->cgi_status = FASTCGI_PARAMS;
            }
        }
        else if (revents != 0)
        {
            print_err(resp, "<%s:%d> Error 0x%02X(0x%02X), fd=%d, id=%d \n", __func__, __LINE__,
                        revents, events, fd, resp->id);
            set_error_message(c, resp, RS502);
        }
    }
    else if (resp->cgi_status == FASTCGI_PARAMS)
    {
        if (revents == POLLOUT)
        {
            if (resp->cgi.buf_param.size_remain() == 0)
            {
                fcgi_set_param(resp);
            }

            int ret = write_to_fcgi(resp->cgi.fd, resp->cgi.buf_param.ptr_remain(), resp->cgi.buf_param.size_remain());
            if (ret < 0)
            {
                if (ret != ERR_TRY_AGAIN)
                    set_error_message(c, resp, RS502);
                return;
            }

            resp->cgi.timer = 0;
            resp->cgi.buf_param.set_offset(ret);
            if (resp->cgi.buf_param.size_remain() == 0)
            {
                if (resp->cgi.buf_param.size() == 8)
                {
                    resp->cgi.vPar.clear();
                    resp->cgi_status = CGI_STDIN;
                    if ((resp->post_content_len == 0) && (resp->post_data.size() == 0))
                    {
                        resp->post_data.cpy("\0\0\0\0\0\0\0\0", 8);
                        fcgi_set_header(&resp->post_data, FCGI_STDIN);
                        resp->post_content_len = -1;
                    }
                }

                resp->cgi.buf_param.init();
            }
        }
        else
        {
            print_err(resp, "<%s:%d> FASTCGI_PARAMS Error revents=0x%02X, id=%d \n",
                            __func__, __LINE__, revents, resp->id);
            set_error_message(c, resp, RS502);
        }
    }
    else if (resp->cgi_status == CGI_STDIN)
    {
        if (revents != POLLOUT)
        {
            print_err(resp, "<%s:%d> FASTCGI_STDIN Error revents=0x%02X, id=%d \n", __func__, __LINE__,
                            revents, resp->id);
            set_error_message(c, resp, RS502);
            return;
        }

        int ret = write(fd, resp->post_data.ptr_remain(), resp->post_data.size_remain());
        if (ret <= 0)
        {
            if (errno != EAGAIN)
            {
                print_err(resp, "<%s:%d> Error write()=%d: %s; id=%d \n", __func__, __LINE__,
                            ret, strerror(errno), resp->id);
                resp->post_data.init();
                set_error_message(c, resp, RS502);
            }
            return;
        }

        resp->post_data.set_offset(ret);
        if (resp->post_data.size_remain() == 0)
        {
            resp->cgi.timer = 0;
            resp->post_data.init();
            if (resp->post_content_len < 0)
                resp->cgi_status = CGI_STDOUT;
        }
        else
        {
            print_err(resp, "<%s:%d> !!! Error write()=%d(%d), id=%d \n", __func__, __LINE__,
                            ret, resp->post_data.size_remain(), resp->id);
        }
    }
    else if (resp->cgi_status == CGI_STDOUT)
    {
        if (revents != POLLIN)
        {
            print_err(resp, "<%s:%d> FASTCGI_STDOUT Error revents=0x%02X, id=%d \n", __func__, __LINE__,
                            revents, resp->id);
            set_error_message(c, resp, RS502);
            return;
        }

        if (resp->buf.size() && resp->create_headers)
            return;

        if (resp->cgi.fcgiContentLen == 0)
        {
            if (resp->cgi.fcgiPaddingLen > 0)
            {
                char s[256];
                if (resp->cgi.fcgiPaddingLen > (int)sizeof(s))
                {
                    resp->post_data.init();
                    set_error_message(c, resp, RS502);
                    return;
                }

                int ret = read(fd, s, resp->cgi.fcgiPaddingLen);
                if (ret <= 0)
                {
                    resp->post_data.init();
                    set_error_message(c, resp, RS502);
                    return;
                }

                resp->cgi.timer = 0;
                resp->cgi.fcgiPaddingLen -= ret;
                return;
            }

            resp->buf.init();
            char s[8];
            int ret = read(fd, s, 8);
            if (ret != 8)
            {
                if ((ret == -1) && (errno == EAGAIN))
                    return;
                set_error_message(c, resp, RS502);
                return;
            }

            resp->cgi.fcgi_type = s[1];
            resp->cgi.fcgiContentLen = ((unsigned char)s[4]<<8) | (unsigned char)s[5];
            resp->cgi.fcgiPaddingLen = (unsigned char)s[6];
            if (resp->cgi.fcgiContentLen == 0)
                return;

            switch (resp->cgi.fcgi_type)
            {
                case FCGI_STDOUT:
                    break;
                case FCGI_STDERR:
                    break;
                case FCGI_END_REQUEST:
                    break;
                default:
                    print_err(resp, "<%s:%d> Error fcgi type: %d\n", __func__, __LINE__, resp->cgi.fcgi_type);
                    set_error_message(c, resp, RS502);
                    return;
            }

            //return;
        }

        char buf[16384];
        int rd = (resp->cgi.fcgiContentLen > (int)c->h2->HTTP2_SendBufSize) ? c->h2->HTTP2_SendBufSize : resp->cgi.fcgiContentLen;
        int ret = read(fd, buf, rd);
        if (ret > 0)
        {
            resp->cgi.timer = 0;
            resp->cgi.fcgiContentLen -= ret;
            switch (resp->cgi.fcgi_type)
            {
                case FCGI_STDOUT:
                    if (resp->create_headers == false)
                    {
                        resp->buf.cat(buf, ret);
                        http2_get_cgi_headers(c, resp);
                    }
                    else
                    {
                        resp->buf.cat(buf, ret);
                    }
                    break;
                case FCGI_STDERR:
                    fwrite(buf, 1, ret, stderr);
                    fprintf(stderr, "\n");
                    break;
                case FCGI_END_REQUEST:
                    resp->cgi.end = true;
                    break;
            }
        }
        else
        {
            if (errno != EAGAIN)
            {
                print_err(resp, "<%s:%d> Error read() = %d\n", __func__, __LINE__, ret);
                set_error_message(c, resp, RS502);
            }
        }
    }
}
//======================================================================
int cgi_parse_headers(Connect* c, Stream *resp, bool lower_case)
{
    const int MAX_HEADER_LEN = 512;
    const char *p = resp->buf.ptr_remain();
    unsigned int size = resp->buf.size_remain();
    Param header;

    for (unsigned int i = 0; i < size; )
    {
        header.name = "";
        header.val = "";

        for ( ; i < size; )// name
        {
            if (i > MAX_HEADER_LEN)
            {
                print_err(resp, "<%s:%d> Error: size of header > %d bytes\n", __func__, __LINE__, i);
                return -1;
            }

            char ch = *(p++);
            ++i;
            if (ch == ':')
                break;
            else if (ch == '\r')
            {
                if (header.name.size())
                {
                    print_err(resp, "<%s:%d> Error\n", __func__, __LINE__);
                    return -1;
                }

                if (i < size)
                {
                    ch = *p++;
                    ++i;
                    if (ch == '\n')
                    {
                        resp->buf.set_offset(i);
                        if (resp->buf.size_remain() == 0)
                            resp->buf.init();
                        return resp->cgi.vPar.size();
                    }
                    else
                    {
                        print_err(resp, "<%s:%d> Error: \\n not found\n", __func__, __LINE__);
                        return -1;
                    }
                }
                return 0;
            }
            else if (ch == '\n')
            {
                if (header.name.size())
                {
                    print_err(resp, "<%s:%d> Error\n", __func__, __LINE__);
                    return -1;
                }

                resp->buf.set_offset(i);
                if (resp->buf.size_remain() == 0)
                    resp->buf.init();
                return resp->cgi.vPar.size();
            }
            else if (ch == 0)
            {
                print_err(resp, "<%s:%d> Error: character = 0\n", __func__, __LINE__);
                return -1;
            }
            else
                header.name += ch;
        }

        if (i == size)
            return 0;
        if (lower_case)
        {
            for (unsigned int n = 0; n < header.name.size(); ++n)
            {
                header.name[n] = tolower(header.name[n]);
            }
        }

        for ( ; i < size; )// val
        {
            if (i > MAX_HEADER_LEN)
            {
                print_err(resp, "<%s:%d> Error: size of header > %d bytes\n", __func__, __LINE__, i);
                return -1;
            }

            char ch = *(p++);
            ++i;

            if (ch == '\r')
                continue;
            else if (ch == '\n')
            {
                if ((header.name == "status") || (header.name == "Status"))
                    sscanf(header.val.c_str(), "%d", &resp->resp_status);
                resp->buf.set_offset(i);
                resp->cgi.vPar.push_back(header);
                size = resp->buf.size_remain();
                i = 0;
                break;
            }
            else if (ch == 0)
            {
                print_err(resp, "<%s:%d> Error: character = 0\n", __func__, __LINE__);
                return -1;
            }
            else if (ch == ' ')
            {
                if (header.val.size())
                    header.val += ch;
            }
            else
                header.val += ch;
        }

        if (i == size)
            return 0;
    }

    return 0;
}
//======================================================================
void EventHandlerClass::http2_get_cgi_headers(Connect* c, Stream *resp)
{
    int ret = cgi_parse_headers(c, resp, true);
    if (ret < 0)
    {
        print_err(resp, "<%s:%d> Error cgi_parse_headers() = -1\n", __func__, __LINE__);
        set_error_message(c, resp, RS502);
        return;
    }
    else if (ret == 0)
    {
        //print_err(resp, "<%s:%d> cgi_parse_headers() = 0\n", __func__, __LINE__);
        return;
    }

    set_frame_headers(resp);
    char str_status[32];
    snprintf(str_status, sizeof(str_status), "%d", resp->resp_status);
    add_header(resp, 8, str_status);
    add_header(resp, 54, conf->ServerSoftware.c_str());
    add_header(resp, 33, get_time().c_str());
    
    for (unsigned int i = 0; i < resp->cgi.vPar.size(); ++i)
    {
        Param header;
        header = resp->cgi.vPar[i];
        if (header.name == "status")
            continue;
        else if (header.name == "content-type")
            add_header(resp, 31, header.val.c_str());
        else if (header.name == "location")
            add_header(resp, 46, header.val.c_str());
        else
            add_header(resp, header.name.c_str(), header.val.c_str());
    }

    if (resp->resp_status == RS204)
    {
        add_header(resp, 28, "0");
        set_frame_flags(&resp->headers, FLAG_END_STREAM);
        resp->create_headers = true;
        resp->cgi.end = true;
        resp->buf.init();
        return;
    }
/*
    if (resp->buf.size_remain())
    {
        set_frame_data(resp, resp->buf.size_remain(), 0);
        resp->send_data.cat(resp->buf.ptr_remain(), resp->buf.size_remain());
    }
    resp->buf.init();*/
    resp->create_headers = true;
}
