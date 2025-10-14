#include "main.h"

using namespace std;
//======================================================================
static void fcgi_set_param(Connect *c)
{
    c->h1->resp.cgi.buf_param.reserve(4096);
    c->h1->resp.cgi.buf_param.cpy("\0\0\0\0\0\0\0\0", 8);

    for ( ; c->h1->resp.cgi.i_param < c->h1->resp.cgi.size_par; ++c->h1->resp.cgi.i_param)
    {
        int len_name = c->h1->resp.cgi.vPar[c->h1->resp.cgi.i_param].name.size();
        int len_val = c->h1->resp.cgi.vPar[c->h1->resp.cgi.i_param].val.size();

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

        c->h1->resp.cgi.buf_param.cat(s, i);
        c->h1->resp.cgi.buf_param.cat(c->h1->resp.cgi.vPar[c->h1->resp.cgi.i_param].name.c_str(), len_name);
        if (len_val > 0)
        {
            c->h1->resp.cgi.buf_param.cat(c->h1->resp.cgi.vPar[c->h1->resp.cgi.i_param].val.c_str(), len_val);
        }
    }

    fcgi_set_header(&c->h1->resp.cgi.buf_param, FCGI_PARAMS);
}
//======================================================================
int fcgi_create_params(Connect *c)
{
    int i = 0;
    Param param;
    c->h1->resp.cgi.vPar.clear();
    if (c->h1->resp.cgi.vPar.capacity() < 50)
        c->h1->resp.cgi.vPar.reserve(50);

    if (c->h1->resp.cgi_type == PHPFPM)
    {
        param.name = "REDIRECT_STATUS";
        param.val = "true";
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

    param.name = "GATEWAY_INTERFACE";
    param.val = "CGI/1.1";
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

    if (c->h1->resp.host.size())
    {
        param.name = "HTTP_HOST";
        param.val = c->h1->resp.host;
        c->h1->resp.cgi.vPar.push_back(param);
        ++i;
    }

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

    if (c->h1->resp.cgi_type == PHPFPM)
    {
        param.name = "SCRIPT_FILENAME";
        param.val = conf->DocumentRoot + c->h1->resp.clean_decode_path;
        c->h1->resp.cgi.vPar.push_back(param);
        ++i;
    }

    if (c->h1->resp.httpMethod == M_POST)
    {
        param.name = "CONTENT_TYPE";
        if (c->h1->resp.sReqContentType.size())
            param.val = c->h1->resp.sReqContentType;
        else
            param.val = "";
        c->h1->resp.cgi.vPar.push_back(param);
        ++i;

        param.name = "CONTENT_LENGTH";
        if (c->h1->resp.sReqContentLen.size())
            param.val = c->h1->resp.sReqContentLen;
        else
            param.val = "0";
        c->h1->resp.cgi.vPar.push_back(param);
        ++i;
    }

    param.name = "QUERY_STRING";
    if (c->h1->resp.query_string.size())
        param.val = c->h1->resp.query_string;
    else
        param.val = "";
    c->h1->resp.cgi.vPar.push_back(param);
    ++i;

    if (i != (int)c->h1->resp.cgi.vPar.size())
    {
        print_err(c, "<%s:%d> Error: create fcgi param list %d/%d\n", __func__, __LINE__, i, (int)c->h1->resp.cgi.vPar.size());
        return -RS500;
    }

    c->h1->resp.cgi.size_par = i;
    c->h1->resp.cgi.i_param = 0;
    c->h1->resp.cgi.timer = 0;

    return 0;
}
//======================================================================
int fcgi_create_connect(Connect *c)
{
    if ((c->h1->resp.cgi_type != PHPFPM) && (c->h1->resp.cgi_type != FASTCGI))
    {
        print_err(c, "<%s:%d> ? req->scriptType=%d\n", __func__, __LINE__, c->h1->resp.cgi_type);
        return -1;
    }

    if (c->h1->resp.cgi_type == PHPFPM)
        c->h1->resp.cgi.socket = &conf->PathPHP;

    c->h1->resp.cgi.fd = create_fcgi_socket(c->h1->resp.cgi.socket->c_str());
    if (c->h1->resp.cgi.fd < 0)
    {
        print_err(c, "<%s:%d> Error connect to fcgi\n", __func__, __LINE__);
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

    c->h1->resp.cgi.buf_param.cpy(s, 16);
    c->h1->resp.cgi_status = FASTCGI_BEGIN;

    int ret = fcgi_create_params(c);
    if (ret < 0)
        return -1;
    return 0;
}
//======================================================================
void EventHandlerClass::fcgi_worker(Connect *c, struct pollfd *poll_fd)
{
    int revents = poll_fd->revents;
    int fd = poll_fd->fd;

    if (c->h1->resp.cgi_status == FASTCGI_BEGIN)
    {
        if (revents == POLLOUT)
        {
            int ret = write_to_fcgi(c->h1->resp.cgi.fd, c->h1->resp.cgi.buf_param.ptr_remain(), c->h1->resp.cgi.buf_param.size_remain());
            if (ret < 0)
            {
                if (ret != ERR_TRY_AGAIN)
                {
                    print_err(c, "<%s:%d> Error write_to_fcgi()=%d\n", __func__, __LINE__, ret);
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
                c->h1->resp.cgi_status = FASTCGI_PARAMS;
            }
        }
        else if (revents != 0)
        {
            print_err(c, "<%s:%d> Error 0x%02X(0x%02X), fd=%d\n", __func__, __LINE__, 
                        poll_fd->revents, poll_fd->events, poll_fd->fd);
            c->err = -RS502;
            http1_end_request(c);
        }
    }
    else if (c->h1->resp.cgi_status == FASTCGI_PARAMS)
    {
        if (revents == POLLOUT)
        {
            if (c->h1->resp.cgi.buf_param.size_remain() == 0)
            {
                fcgi_set_param(c);
            }

            int ret = write_to_fcgi(c->h1->resp.cgi.fd, c->h1->resp.cgi.buf_param.ptr_remain(), c->h1->resp.cgi.buf_param.size_remain());
            if (ret < 0)
            {
                if (ret != ERR_TRY_AGAIN)
                {
                    print_err(c, "<%s:%d> Error write_to_fcgi()=%d\n", __func__, __LINE__, ret);
                    c->err = -RS502;
                    http1_end_request(c);
                }
                return;
            }

            c->h1->resp.cgi.timer = 0;
            c->h1->resp.cgi.buf_param.set_offset(ret);
            if (c->h1->resp.cgi.buf_param.size_remain() == 0)
            {
                if (c->h1->resp.cgi.buf_param.size() == 8)
                {
                    c->h1->resp.cgi_status = CGI_STDIN;
                    c->h1->con_status = http1::READ_POSTDATA;
                    if ((c->h1->resp.post_content_len <= 0) && (c->h1->resp.post_data.size() == 0))
                    {
                        c->h1->resp.post_data.cpy("\0\0\0\0\0\0\0\0", 8);
                        fcgi_set_header(&c->h1->resp.post_data, FCGI_STDIN);
                    }
                }

                c->h1->resp.cgi.buf_param.init();
            }
        }
        else
        {
            print_err(c, "<%s:%d> FASTCGI_PARAMS Error revents=0x%02X: %s\n", __func__, __LINE__, revents);
            c->err = -RS502;
            http1_end_request(c);
        }
    }
    else if (c->h1->resp.cgi_status == CGI_STDIN)
    {
        if (revents != POLLOUT)
        {
            print_err(c, "<%s:%d> FASTCGI_STDIN Error revents=0x%02X: %s\n", __func__, __LINE__, revents);
            c->err = -RS502;
            http1_end_request(c);
            return;
        }

        int ret = write(fd, c->h1->resp.post_data.ptr_remain(), c->h1->resp.post_data.size_remain());
        if (ret <= 0)
        {
            if (errno != EAGAIN)
            {
                print_err(c, "<%s:%d> Error write()=%d: %s\n", __func__, __LINE__, ret, strerror(errno));
                c->h1->resp.post_data.init();
                c->err = -RS502;
                http1_end_request(c);
            }
            return;
        }

        if (ret != c->h1->resp.post_data.size_remain())
        {
            print_err(c, "<%s:%d> !!! Error write()=%d(%d)\n", __func__, __LINE__, ret, c->h1->resp.post_data.size_remain());
        }
        c->h1->resp.post_data.set_offset(ret);
        c->h1->resp.cgi.timer = 0;
        if (c->h1->resp.post_data.size_remain() == 0)
        {
            c->h1->resp.post_data.init();
            if (c->h1->resp.post_content_len <= 0)
            {
                c->h1->resp.cgi_status = CGI_STDOUT;
                c->h1->con_status = http1::SEND_RESP_HEADERS;
                c->h1->resp.cgi.fcgiContentLen = 0;
                c->h1->resp.cgi.fcgiPaddingLen = 0;
            }
        }
    }
    else if (c->h1->resp.cgi_status == CGI_STDOUT)
    {
        if (revents != POLLIN)
        {
            print_err(c, "<%s:%d> FASTCGI_STDOUT Error revents=0x%02X: %s\n", __func__, __LINE__, revents);
            c->err = -RS502;
            http1_end_request(c);
            return;
        }

        if (c->h1->resp.cgi.fcgiContentLen == 0)
        {
            if (c->h1->resp.cgi.fcgiPaddingLen > 0)
            {
                char s[256];
                if (c->h1->resp.cgi.fcgiPaddingLen > (int)sizeof(s))
                {
                    print_err(c, "<%s:%d> Error fcgiPaddingLen=%d\n", __func__, __LINE__, c->h1->resp.cgi.fcgiPaddingLen);
                    c->h1->resp.post_data.init();
                    c->err = -RS502;
                    http1_end_request(c);
                    return;
                }

                int ret = read(fd, s, c->h1->resp.cgi.fcgiPaddingLen);
                if (ret <= 0)
                {
                    print_err(c, "<%s:%d> Error read()=%d\n", __func__, __LINE__, ret);
                    c->h1->resp.post_data.init();
                    c->err = -RS502;
                    http1_end_request(c);
                    return;
                }

                c->h1->resp.cgi.fcgiPaddingLen -= ret;
                return;
            }

            c->h1->resp.send_data.init();
            char s[8];
            int ret = read(fd, s, 8);
            if (ret != 8)
            {
                if ((ret == -1) && (errno == EAGAIN))
                    return;
                print_err(c, "<%s:%d> Error read()=%d\n", __func__, __LINE__, ret);
                c->err = -RS502;
                http1_end_request(c);
                return;
            }

            c->h1->resp.cgi.fcgi_type = s[1];
            c->h1->resp.cgi.fcgiContentLen = ((unsigned char)s[4]<<8) | (unsigned char)s[5];
            c->h1->resp.cgi.fcgiPaddingLen = (unsigned char)s[6];
            if ((c->h1->resp.cgi.fcgiContentLen > 65535) || (c->h1->resp.cgi.fcgiPaddingLen > 255))
            {
                print_err(c, "<%s:%d> Error fcgiContentLen=%d, fcgiPaddingLen=%d\n", __func__, __LINE__, c->h1->resp.cgi.fcgiContentLen, c->h1->resp.cgi.fcgiPaddingLen);
                hex_print_stderr(__func__, __LINE__, s, 8);
                c->err = -1;
                http1_end_request(c);
                return;
            }

            switch (s[1])
            {
                case FCGI_STDOUT:
                    break;
                case FCGI_STDERR:
                    break;
                case FCGI_END_REQUEST:
                    c->h1->resp.cgi.end = true;
                    if (c->h1->mode_send == CHUNK)
                    {
                        char s[] = "0\r\n\r\n";
                        c->h1->resp.send_data.cpy(s, 5);
                    }
                    else
                    {
                        c->h1->resp.send_data.init();
                        http1_end_request(c);
                    }

                    break;
                default:
                    print_err(c, "<%s:%d> Error fcgi type %d\n", __func__, __LINE__, (int)s[1]);
                    hex_print_stderr(__func__, __LINE__, s, 8);
                    c->err = -RS502;
                    http1_end_request(c);
            }

            return;
        }

        char buf[65535]; // 16384
        int rd = (c->h1->resp.cgi.fcgiContentLen > (int)sizeof(buf)) ? sizeof(buf) : c->h1->resp.cgi.fcgiContentLen;
        int ret = read(fd, buf, rd);
        if (ret > 0)
        {
            c->h1->resp.cgi.fcgiContentLen -= ret;
            switch (c->h1->resp.cgi.fcgi_type)
            {
                case FCGI_STDOUT:
                    if ((c->h1->con_status == http1::SEND_RESP_HEADERS) && 
                        (c->h1->resp.create_headers == false)
                    )
                    {
                        c->h1->resp.send_data.cat(buf, ret);
                        fcgi_get_headers(c);
                    }
                    else
                    {
                        if (c->h1->mode_send == CHUNK)
                        {
                            c->h1->resp.send_data.cpy("01234567", 8);
                            c->h1->resp.send_data.cat(buf, ret);
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
                        {
                            c->h1->resp.send_data.cat(buf, ret);
                        }
                    }
                    break;
                case FCGI_STDERR:
                    fwrite(buf, 1, ret, stderr);
                    fprintf(stderr, "\n");
                    break;
                case FCGI_END_REQUEST:
                    {
                        c->h1->resp.cgi.end = true;
                        if (c->h1->mode_send == CHUNK)
                        {
                            char s[] = "0\r\n\r\n";
                            c->h1->resp.send_data.cat_str(s);
                        }

                        if (c->h1->resp.send_data.size() == 0)
                        {
                            http1_end_request(c);
                        }
                    }
                    break;
            }
        }
        else
        {
            if (errno != EAGAIN)
            {
                print_err(c, "<%s:%d> Error read()=%d: %s\n", __func__, __LINE__, ret, strerror(errno));
                c->err = -RS502;
                http1_end_request(c);
            }
        }
    }
}
//======================================================================
void EventHandlerClass::fcgi_get_headers(Connect *c)
{
    const char *p1 = c->h1->resp.send_data.ptr(), *p = NULL;
    for (int i = 0; i < c->h1->resp.send_data.size(); ++i)
    {
        if (*(p1++) == '\n')
        {
            if (*(p1) == '\r')
            {
                p1++;
                if (*(p1) == '\n')
                {
                    p1++;
                    p = p1;
                    break;
                }
            }
            else if (*(p1) == '\n')
            {
                p1++;
                p = p1;
                break;
            }
        }
    }

    if (p)
    {
        const char *p3 = NULL;
        if ((p3 = strstr_case(c->h1->resp.send_data.ptr(), "Status:")))
        {
            sscanf(p3 + 7, "%d", &c->h1->resp.resp_status);
            if (c->h1->resp.resp_status == RS204)
            {
                c->h1->mode_send = NO_CHUNK;
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

        if ((p3 = strstr_case(c->h1->resp.send_data.ptr(), "Content-Type:")))
        {
            char cont_type[64] = "NO";
            int j = 0;
            for (int i = 0; i < 64; ++i)
            {
                char ch = *(p3 + 13 + i);
                if ((ch == ' ') && (j == 0))
                    continue;
                else if ((ch == '\r') || (ch == '\n'))
                    break;
                else
                    cont_type[j++] = ch;
            }

            cont_type[j] = 0;
            c->h1->hdrs << "Content-Type: " << cont_type << "\r\n";
            if (conf->PrintDebugMsg)
                print_err(c, "<%s:%d> Content-Type: %s\n", __func__, __LINE__, cont_type);
            if ((p - c->h1->resp.send_data.ptr()) == c->h1->resp.send_data.size())
            {
                c->h1->resp.send_data.init();
            }
            else
            {
                if (c->h1->mode_send == CHUNK)
                {
                    c->h1->resp.send_data.set_offset(p - c->h1->resp.send_data.ptr() - 8);
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
                    c->h1->resp.send_data.set_offset(p - c->h1->resp.send_data.ptr());
            }
            
            c->h1->resp.resp_status = RS200;
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
        else
        {
            print_err(c, "<%s:%d> Error \"Content-Type\" not found\n", __func__, __LINE__);
            c->err = -RS502;
            http1_end_request(c);
        }
    }
}
