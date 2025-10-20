#include "main.h"

using namespace std;
//======================================================================
void set_frame(Stream *resp, char *s, int len, int type, HTTP2_FLAGS flags, int id)
{
    s[0] = (len>>16) & 0xff;
    s[1] = (len>>8) & 0xff;
    s[2] = len & 0xff;

    s[3] = (unsigned char)type;
    s[4] = (unsigned char)flags;

    s[5] = (id>>24) & 0x7f;
    s[6] = (id>>16) & 0xff;
    s[7] = (id>>8) & 0xff;
    s[8] = id & 0xff;
}
//======================================================================
void set_frame_headers(Stream *resp)
{
    int id = resp->id;
    resp->headers.cpy("\0\0\0\1\4\0\0\0\0", 9);
    resp->headers.set_byte((id>>24) & 0x7f, 5);
    resp->headers.set_byte((id>>16) & 0xff, 6);
    resp->headers.set_byte((id>>8) & 0xff, 7);
    resp->headers.set_byte(id & 0xff, 8);
    if (resp->numReq == 1)
        resp->headers.cat(0x20);
}
//======================================================================
void add_header(Stream *resp, int ind)
{
    if ((ind >= 8) && (ind <= 14))
        resp->resp_status = atoi(static_tab[ind][1]);
    resp->headers.cat(ind | 0x80);
    int len = resp->headers.size() - 9;
    resp->headers.set_byte((len>>16) & 0xff, 0);
    resp->headers.set_byte((len>>8) & 0xff, 1);
    resp->headers.set_byte(len & 0xff, 2);
}
//======================================================================
void add_header(Stream *resp, int ind, int mask, const char *val, bool huffman)
{
    if ((ind >= 8) && (ind <= 14))
        resp->resp_status = atoi(val);
    int len = (int)strlen(val);
    int prefix_len;
    switch (mask)
    {
        case 0x40:
            prefix_len = 6;
            break;
        case 0x10:
            prefix_len = 4;
            break;
        case 0x00:
            prefix_len = 4;
            break;
        default:
            prefix_len = 6;
            mask = 0x40;
    }

    int_to_bytes(resp->headers, ind, prefix_len, mask);

    if (huffman)
    {
        ByteArray buf;
        huffman_encode(val, buf);
        int_to_bytes(resp->headers, buf.size(), 7, 0x80);
        resp->headers.cat(buf.ptr(), buf.size());
    }
    else
    {
        int_to_bytes(resp->headers, len, 7, 0);
        resp->headers.cat(val, len);
    }
    len = resp->headers.size() - 9;
    resp->headers.set_byte((len>>16) & 0xff, 0);
    resp->headers.set_byte((len>>8) & 0xff, 1);
    resp->headers.set_byte(len & 0xff, 2);
}
//======================================================================
void add_header(Stream *resp, int ind, const char *val)
{
    add_header(resp, ind, hpack_mask, val, true);
}
//======================================================================
void set_frame_window_update(Stream *resp, int len)
{
    int id = resp->id;
    char s[] = "\x00\x00\x04\x08\x00\x00\x00\x00\x00"  // 0-8
               "\x00\x00\x00\x00";                     // 9-12

    resp->frame_win_update.cpy(s, 13);

    resp->frame_win_update.set_byte((len>>24) & 0x7f, 9);
    resp->frame_win_update.set_byte((len>>16) & 0xff, 10);
    resp->frame_win_update.set_byte((len>>8) & 0xff, 11);
    resp->frame_win_update.set_byte(len & 0xff, 12);

    resp->frame_win_update.set_byte((id>>24) & 0x7f, 5);
    resp->frame_win_update.set_byte((id>>16) & 0xff, 6);
    resp->frame_win_update.set_byte((id>>8) & 0xff, 7);
    resp->frame_win_update.set_byte(id & 0xff, 8);
}
//======================================================================
void set_frame_window_update(Connect *con, int len)
{
    con->h2->frame_win_update.cpy("\x00\x00\x04\x08\x00\x00\x00\x00\x00"  // 0-8
                               "\x00\x00\x00\x00", 13);              // 9-12

    con->h2->frame_win_update.set_byte((len>>24) & 0x7f, 9);
    con->h2->frame_win_update.set_byte((len>>16) & 0xff, 10);
    con->h2->frame_win_update.set_byte((len>>8) & 0xff, 11);
    con->h2->frame_win_update.set_byte(len & 0xff, 12);
}
//======================================================================
void set_frame_goaway(Connect *con, HTTP2_ERRORS error)
{
    char buf[] = "\x0\x0\x0\x0\x0\x0\x0\x0";
    con->h2->goaway.cpy("\x0\x0\x8\x7\x0\x0\x0\x0\x0", 9);

    buf[4] = (unsigned char)((error>>24) & 0xff);
    buf[5] = (unsigned char)((error>>16) & 0xff);
    buf[6] = (unsigned char)((error>>8) & 0xff);
    buf[7] = (unsigned char)(error & 0xff);

    con->h2->goaway.cat(buf, 8);
}
//======================================================================
int set_rst_stream(Connect *c, int id, HTTP2_ERRORS error)
{
    c->h2->close_stream(id);

    FrameRedySend *rf = NULL;
    rf = new(std::nothrow) FrameRedySend;
    if (!rf)
    {
        print_err(c, "<%s:%d> Error: %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    c->h2->push_to_list(rf);

    rf->id = id;

    rf->frame.cpy("\0\0\4\3\0\0\0\0\0"
                         "\0\0\0\0", 13);
    rf->frame.set_byte((id>>24) & 0x7f, 5);
    rf->frame.set_byte((id>>16) & 0xff, 6);
    rf->frame.set_byte((id>>8) & 0xff, 7);
    rf->frame.set_byte(id & 0xff, 8);

    rf->frame.set_byte((error>>24) & 0xff, 9);
    rf->frame.set_byte((error>>16) & 0xff, 10);
    rf->frame.set_byte((error>>8) & 0xff, 11);
    rf->frame.set_byte(error & 0xff, 12);

    return 0;
}
//======================================================================
void set_frame_data(Stream *resp, int len, int flag)
{
    int id = resp->id;
    resp->send_data.cpy("\0\0\0\0\0\0\0\0\0", 9);
    resp->send_data.set_byte(flag, 4);
    resp->send_data.set_byte((len>>16) & 0xff, 0);
    resp->send_data.set_byte((len>>8) & 0xff, 1);
    resp->send_data.set_byte(len & 0xff, 2);

    resp->send_data.set_byte((id>>24) & 0x7f, 5);
    resp->send_data.set_byte((id>>16) & 0xff, 6);
    resp->send_data.set_byte((id>>8) & 0xff, 7);
    resp->send_data.set_byte(id & 0xff, 8);
}
//======================================================================
int set_frame_data(Connect *con, Stream *resp)
{
    resp->send_data.init();
    long data_len = 0;
    long min_window_size = (con->h2->connect_window_size > resp->stream_window_size) ? resp->stream_window_size : con->h2->connect_window_size;
    if (min_window_size <= 0)
    {
        print_err(resp, "<%s:%d> !!! connect_window_size=%ld, stream_window_size=%ld, id=%d \n", __func__, __LINE__, con->h2->connect_window_size, resp->stream_window_size, resp->id);
        return 0;
    }

    if (resp->source_data == DYN_PAGE)
    {
        if (resp->send_headers == false)
            return 0;
        if (resp->httpMethod == M_HEAD)
        {
            resp->buf.init();
            resp->cgi.end = true;
            set_frame_data(resp, 0, FLAG_END_STREAM);
            return 0;
        }

        if (resp->buf.size_remain())
        {
            int len = resp->buf.size_remain();
            if (len > conf->HTTP2_DataBufSize)
                len = conf->HTTP2_DataBufSize;
            set_frame_data(resp, len, 0);
            resp->send_data.cat(resp->buf.ptr_remain(), len);
            resp->buf.set_offset(len);
            if (resp->buf.size_remain() == 0)
                resp->buf.init();
        }
        else
        {
            if (resp->cgi.end)
                set_frame_data(resp, 0, FLAG_END_STREAM);
            else
                return 0;
        }
    }
    else
    {
        if (resp->source_data == FROM_FILE)
        {
            if (resp->resp_content_len > conf->HTTP2_DataBufSize)
                data_len = conf->HTTP2_DataBufSize;
            else
                data_len = (int)resp->resp_content_len;

            char buf[16384];
            if (data_len > 0)
            {
                if (data_len > min_window_size)
                {
                    //print_err(resp, "<%s:%d> !!! data_len(%ld) > min_window_size(%ld), id=%d \n", __func__, __LINE__, data_len, min_window_size, resp->id);
                    data_len = min_window_size;
                }

                int ret = read(resp->fd, buf, data_len);
                if (ret <= 0)
                {
                    print_err(resp, "<%s:%d> Error read(fd=%d)=%d: %s, id=%d \n", __func__, __LINE__, resp->fd, ret, strerror(errno), resp->id);
                    close(resp->fd);
                    resp->fd = -1;
                    return -1;
                }

                data_len = ret;
            }

            resp->resp_content_len -= data_len;
            int flag = (resp->resp_content_len > 0) ? 0 : FLAG_END_STREAM;
            set_frame_data(resp, data_len, flag);
            if (data_len > 0)
                resp->send_data.cat(buf, data_len);
            if (resp->resp_content_len == 0)
            {
                close(resp->fd);
                resp->fd = -1;
            }
        }
        else if (resp->source_data == DIRECTORY)
        {
            if (resp->resp_content_len > conf->HTTP2_DataBufSize)
                data_len = conf->HTTP2_DataBufSize;
            else
                data_len = (int)resp->resp_content_len;

            if (data_len > min_window_size)
                data_len = min_window_size;

            if ((resp->buf.get_offset() + data_len) > resp->buf.size())
            {
                print_err(resp, "<%s:%d> Error\n", __func__, __LINE__);
                return -1;
            }

            resp->resp_content_len -= data_len;
            int flag = (resp->resp_content_len > 0) ? 0 : FLAG_END_STREAM;
            set_frame_data(resp, data_len, flag);
            resp->send_data.cat(resp->buf.ptr_remain(), data_len);
            resp->buf.set_offset(data_len);
        }
    }

    return 1;
}
//======================================================================
int set_response(Connect *con, Stream *resp)
{
    resp->send_bytes = 0;
    decode(resp->path.c_str(), resp->path.size(), resp->decode_path);

    int len = resp->decode_path.size();
    if (len >= resp->clean_decode_path_size)
    {
        if (resp->clean_decode_path)
        {
            delete [] resp->clean_decode_path;
            resp->clean_decode_path = NULL;
            resp->clean_decode_path_size = 0;
        }

        resp->clean_decode_path = new(nothrow) char [len + 1];
        if (resp->clean_decode_path == NULL)
        {
            print_err(resp, "<%s:%d> Error new char [%d]: %s\n", __func__, __LINE__, len + 1, strerror(errno));
            resp_500(resp);
            return 0;
        }

        resp->clean_decode_path_size = len + 1;
    }

    const char *p = strchr(resp->path.c_str(), '?');
    if (p)
        resp->query_string = p + 1;
    else
        resp->query_string = "";

    memcpy(resp->clean_decode_path, resp->decode_path.c_str(), len);
    resp->clean_decode_path[len] = 0;
    p = (const char*)memchr(resp->clean_decode_path, '?', len);
    if (p)
    {
        len = p - resp->clean_decode_path;
        resp->decode_query_string = p + 1;
        resp->clean_decode_path[len] = 0;
    }
    else
    {
        resp->decode_query_string = NULL;
        if (resp->query_string.size() > 0)
        {
            print_err(resp, "<%s:%d> Error ?\n", __func__, __LINE__);
            resp_500(resp);
            return 0;
        }
    }

    int err = clean_path(resp->clean_decode_path, len);
    if (err <= 0)
    {
        print_err(resp, "<%s:%d> Error: clean_path[%d], id=%d \n", __func__, __LINE__, err, resp->id);
        resp_400(resp);
        return 0;
    }

    if (resp->httpMethod == M_POST)
    {
        if (resp->sReqContentType.size() == 0)
        {
            print_err(resp, "<%s:%d> Content-Type \?\n", __func__, __LINE__);
            resp_400(resp);
            return 0;
        }

        if (resp->sReqContentLen.size() == 0)
        {
            print_err(resp, "<%s:%d> 411 Length Required\n", __func__, __LINE__);
            resp_411(resp);
            return 0;
        }

        if (resp->post_content_len >= conf->ClientMaxBodySize)
        {
            print_err(resp, "<%s:%d> 413 Request entity too large: %lld\n", __func__, __LINE__, resp->post_content_len);
            resp_413(resp);
            return 0;
        }
    }
    //-------------------------------
    string path = ".";
    if (!strncmp(resp->decode_path.c_str(), "/cgi-bin/", 9))
    {
        resp->source_data = DYN_PAGE;
        resp->cgi_type = CGI;
        resp->cgi.scriptName = resp->clean_decode_path;
    }
    else if (strstr(resp->decode_path.c_str(), ".php"))
    {
        resp->source_data = DYN_PAGE;
        resp->cgi.scriptName = resp->clean_decode_path;
        if (conf->UsePHP == "php-cgi")
            resp->cgi_type = PHPCGI;
        else if (conf->UsePHP == "php-fpm")
            resp->cgi_type = PHPFPM;
        else
        {
            resp_404(resp);
            return 0;
        }
    }
    else
    {
        path += resp->decode_path;
        resp->source_data = get_content_type(path.c_str());
    }

    if (resp->source_data == FROM_FILE)
    {
        // ----- file -----
        resp->file_size = (long long)file_size(path.c_str());
        if (resp->file_size < 0)
        {
            print_err(resp, "<%s:%d> Error file_size(%s)\n", __func__, __LINE__, path.c_str());
            resp_500(resp);
            return 0;
        }

        if (resp->range.size())
        {
            if (parse_range(resp->range.c_str(), resp->file_size, &resp->offset, &resp->resp_content_len))
            {
                print_err(resp, "<%s:%d> Error parse_range(%s)\n", __func__, __LINE__, resp->range.c_str());
                resp_400(resp);
                return 0;
            }
        }
        else
            resp->resp_content_len = resp->file_size;
        //----------- frame headers ----------------
        set_frame_headers(resp);
        if (resp->range.size())
            add_header(resp, 10);                                     // "206 Partial Content"
        else
            add_header(resp, 8);                                      // "200 OK"
        add_header(resp, 54, conf->ServerSoftware.c_str());           // "server"
        add_header(resp, 33, get_time().c_str());                     // "date"
        
        resp->resp_content_type = content_type(resp->path.c_str());
        if (resp->resp_content_type)
            add_header(resp, 31, resp->resp_content_type);            // "content-type"

        char s[128];
        snprintf(s, sizeof(s), "%lld", resp->resp_content_len);
        add_header(resp, 28, s);                                      // "content-length"
        add_header(resp, 18, "bytes");                                // "accept-ranges"
        add_header(resp, 24, "no-cache, no-store, must-revalidate");  // "cache-control"

        if ((resp->file_size == 0) || (resp->httpMethod == M_HEAD))
        {
            char flag = resp->headers.get_byte(4);
            resp->headers.set_byte(flag | FLAG_END_STREAM, 4);
            return 0;
        }

        if (resp->range.size())
        {
            char s[128];
            snprintf(s, sizeof(s), "bytes %lld-%lld/%lld", resp->offset, resp->offset + resp->resp_content_len - 1, resp->file_size);
            resp->file_size = resp->resp_content_len;
            add_header(resp, 30, s);                                  // "content-range"
        }

        resp->create_headers = true;
        resp->fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (resp->fd == -1)
        {
            print_err(resp, "<%s:%d> Error open(%s): %s\n", __func__, __LINE__, resp->decode_path.c_str(), strerror(errno));
            if (errno == EACCES)
                resp_403(resp);
            else if (errno == ENOENT)
                resp_404(resp);
            else
                resp_500(resp);
            return 0;
        }

        if (resp->offset > 0)
            lseek(resp->fd, resp->offset, SEEK_SET);
    }
    else if (resp->source_data == DIRECTORY)
    {
        if (resp->decode_path[resp->decode_path.size() - 1] != '/')
        {
            set_frame_headers(resp);
            add_header(resp, 8, "301");                               // "301 Moved Permanently"
            add_header(resp, 54, conf->ServerSoftware.c_str());       // "server"
            add_header(resp, 33, get_time().c_str());                 // "date"
            add_header(resp, 46, resp->path.append("/").c_str());     // "location"
            add_header(resp, 31, "text/plain");                       // "content-type"
            resp->create_headers = true;

            ByteArray smg;
            smg.cpy_str("301 Moved Permanently\n");
            smg.cat(resp->path.c_str(), resp->path.size());

            char s[128];
            snprintf(s, sizeof(s), "%d", smg.size());
            add_header(resp, 28, s);                                  // "content-length"

            set_frame_data(resp, smg.size(), FLAG_END_STREAM);
            resp->send_data.cat(smg.ptr(), smg.size());
            return 0;
        }

        int err = index_dir(con, path, resp->decode_path.c_str(), &resp->buf);
        if (err)
        {
            print_err(resp, "<%s:%d> Error index_dir(): %d\n", __func__, __LINE__, -err);
            resp_500(resp);
            return 0;
        }
        resp->resp_content_len = resp->buf.size();
        if (resp->httpMethod == M_HEAD)
            resp->buf.init();
        //------------- headers frame --------------
        set_frame_headers(resp);
        add_header(resp, 8);                                          // "200 OK"
        add_header(resp, 54, conf->ServerSoftware.c_str());           // "server"
        add_header(resp, 33, get_time().c_str());                     // "date"
        add_header(resp, 31, "text/html;charset=UTF-8");                            // "content-type"
        add_header(resp, 24, "no-cache, no-store, must-revalidate");  // "cache-control"
        resp->create_headers = true;

        if (resp->httpMethod == M_HEAD)
        {
            char flag = resp->headers.get_byte(4);
            resp->headers.set_byte(flag | FLAG_END_STREAM, 4);
        }
    }
    else if (resp->source_data == DYN_PAGE)
    {
        resp->send_data.init();
        resp->cgi_status = CGI_CREATE;
        resp->resp_status = RS200;
    }
    else
    {
        if (is_cgi(resp))
        {
            resp->send_data.init();
            resp->cgi_status = CGI_CREATE;
            resp->resp_status = RS200;
        }
        else
        {
            print_err(resp, "<%s:%d> Error: CONTENT_TYPE %s, create_headers=%d, send_headers=%d\n", __func__, __LINE__, path.c_str(), resp->create_headers, resp->send_headers);
            resp_404(resp);
        }
    }

    return 0;
}
//======================================================================
int EventHandlerClass::http2_connection(Connect *con)
{
    if (con->h2->con_status == http2::PREFACE_MESSAGE)
    {
        char buf[25];

        int ret = read_from_client(con, buf, sizeof(buf) - 1);
        if (ret == 24)
        {
            buf[ret] = 0;
            if (memcmp(buf, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24))
            {
                print_err(con, "<%s:%d> Error ---PREFACE_MESSAGE--- %s\n", __func__, __LINE__, buf);
                ssl_shutdown(con);
                return -1;
            }

            if (conf->PrintDebugMsg)
                hex_print_stderr(__func__, __LINE__, buf, 24);
            con->client_timer = 0;
            con->h2->con_status = http2::SEND_SETTINGS;
            con->h2->init();
            con->tls.poll_events = POLLOUT;
        }
        else
        {
            if (ret == ERR_TRY_AGAIN)
            {
                return 0;
            }

            print_err(con, "<%s:%d> Error read_from_client()=%d\n", __func__, __LINE__, ret);
            ssl_shutdown(con);
            return -1;
        }
        return 0;
    }
    else if (con->h2->con_status == http2::SSL_SHUTDOWN)
    {
        ERR_clear_error();
        char buf[256];
        int err = SSL_read(con->tls.ssl, buf, sizeof(buf));
        if (err <= 0)
        {
            con->tls.err = SSL_get_error(con->tls.ssl, err);
            if (con->tls.err == SSL_ERROR_WANT_READ)
            {
                con->tls.poll_events = POLLIN;
            }
            else if (con->tls.err == SSL_ERROR_WANT_WRITE)
            {
                con->tls.poll_events = POLLOUT;
            }
            else
            {
                print_err(con, "<%s:%d> SSL_SHUTDOWN: SSL_read() - %s\n", __func__, __LINE__, ssl_strerror(con->tls.err));
                close_connect(con);
                return -1;
            }
        }
        else
        {
            print_err(con, "<%s:%d> SSL_SHUTDOWN: SSL_read()=%d\n", __func__, __LINE__, err);
            con->client_timer = 0;
            con->tls.shutdown_timer = 0;
            if (conf->PrintDebugMsg)
                hex_print_stderr("recv SSL_SHUTDOWN", __LINE__, buf, err);
        }
        return 0;
    }
    else
    {
        print_err(con, "<%s:%d> !!! Error: type operation (%s)\n", __func__, __LINE__, con->h2->get_str_status());
        ssl_shutdown(con);
        return -1;
    }

    return 0;
}
//======================================================================
int EventHandlerClass::recv_frame(Connect *con)
{
    int ret = recv_frame_(con);
    if (ret <= 0)
    {
        if (ret == ERR_TRY_AGAIN)
            return 0;
        ssl_shutdown(con);
        return -1;
    }

    ret = parse_frame(con);
    if (ret < 0)
    {
        if (ret == ERR_TRY_AGAIN)
            return 0;
        ssl_shutdown(con);
        return -1;
    }

    return 0;
}
//======================================================================
int EventHandlerClass::recv_frame_(Connect *con)
{
    if (con->h2->header_len < 9)
    {
        if (con->h2->header_len == 0)
            con->h2->init();
        int ret = read_from_client(con, con->h2->header + con->h2->header_len, 9 - con->h2->header_len);
        if (ret <= 0)
        {
            print_err(con, "<%s:%d> Error read_from_client()=%d\n", __func__, __LINE__, ret);
            return ret;
        }

        con->h2->header_len += ret;
        if (con->h2->header_len == 9)
        {
            con->h2->body_len = ((unsigned char)con->h2->header[0]<<16) +
                ((unsigned char)con->h2->header[1]<<8) + (unsigned char)con->h2->header[2];
            con->h2->type = (FRAME_TYPE)con->h2->header[3];
            con->h2->flags = con->h2->header[4];
            con->h2->id = (((unsigned char)con->h2->header[5] & 0x7f)<<16) + ((unsigned char)con->h2->header[6]<<16) +
                ((unsigned char)con->h2->header[7]<<8) + (unsigned char)con->h2->header[8];
            if (conf->PrintDebugMsg)
                hex_print_stderr(__func__, __LINE__, con->h2->header, 9);
            if (con->h2->body_len > conf->HTTP2_DataBufSize)
            {
                print_err(con, "<%s:%d> Error frame size: %d\n", __func__, __LINE__, con->h2->body_len + 9);
                return -1;
            }
        }
        else
        {
            print_err(con, "<%s:%d> Error read frame header (%s)\n", __func__, __LINE__, con->h2->get_str_status());
            return -1;
        }
    }

    if (con->h2->body_len > 0)
    {
        char buf[16384];
        int len_rd = (int)sizeof(buf);
        if (con->h2->body_len < len_rd)
            len_rd = con->h2->body_len;
        int ret = read_from_client(con, buf, len_rd);
        if (ret <= 0)
        {
            if (ret == ERR_TRY_AGAIN)
                print_err(con, "<%s:%d> Error (SSL_ERROR_WANT_READ) read frame %s id=%d \n", __func__, __LINE__, get_str_frame_type(con->h2->type), con->h2->id);
            else
                print_err(con, "<%s:%d> Error read frame %s id=%d \n", __func__, __LINE__, get_str_frame_type(con->h2->type), con->h2->id);
            return ret;
        }

        con->h2->body.cat(buf, ret);
        con->h2->body_len -= ret;
        if (con->h2->body_len == 0)
            con->h2->header_len = 0;
        else if (con->h2->body_len > 0)
            return ERR_TRY_AGAIN;
    }
    else if (con->h2->body_len == 0)
    {
        con->h2->header_len = 0;
    }

    return 1;
}
//======================================================================
int EventHandlerClass::parse_frame(Connect *con)
{
    if (con->h2->type == SETTINGS)
    {
        con->client_timer = 0;
        if (conf->PrintDebugMsg)
            hex_print_stderr("recv SETTINGS", __LINE__, con->h2->body.ptr(), con->h2->body.size());
        if (con->h2->body.size())
        {
            for (int i = 0; i < (con->h2->body.size()/6); ++i)
            {
                int ind = i * 6;
                if (con->h2->body.get_byte(ind + 1) == 1)
                {
                    long n = (unsigned char)con->h2->body.get_byte(ind + 5);
                    n += ((unsigned char)con->h2->body.get_byte(ind + 4)<<8);
                    n += ((unsigned char)con->h2->body.get_byte(ind + 3)<<16);
                    n += ((unsigned char)con->h2->body.get_byte(ind + 2)<<24);
                    if (conf->PrintDebugMsg)
                        print_err(con, "<%s:%d> SETTINGS_HEADER_TABLE_SIZE [%ld] id=%d \n", __func__, __LINE__, n, 0);
                }
                else if (con->h2->body.get_byte(ind + 1) == 4)
                {
                    con->h2->init_window_size = (unsigned char)con->h2->body.get_byte(ind + 5);
                    con->h2->init_window_size += ((unsigned char)con->h2->body.get_byte(ind + 4)<<8);
                    con->h2->init_window_size += ((unsigned char)con->h2->body.get_byte(ind + 3)<<16);
                    con->h2->init_window_size += ((unsigned char)con->h2->body.get_byte(ind + 2)<<24);
                    if (conf->PrintDebugMsg)
                        print_err(con, "<%s:%d> SETTINGS_INITIAL_WINDOW_SIZE [%ld] id=%d \n", __func__, __LINE__, con->h2->init_window_size, 0);
                }
                else if (con->h2->body.get_byte(ind + 1) == 5)
                {
                    int n = (unsigned char)con->h2->body.get_byte(ind + 5);
                    n += ((unsigned char)con->h2->body.get_byte(ind + 4)<<8);
                    n += ((unsigned char)con->h2->body.get_byte(ind + 3)<<16);
                    n += ((unsigned char)con->h2->body.get_byte(ind + 2)<<24);
                    if (n < conf->HTTP2_DataBufSize)
                    {
                        setDataBufSize(n);
                    }
                    if (conf->PrintDebugMsg)
                        print_err(con, "<%s:%d> SETTINGS_MAX_FRAME_SIZE [%ld], conf->HTTP2_DataBufSize=%d, id=0 \n", __func__, __LINE__, n, conf->HTTP2_DataBufSize);
                }
            }

            con->h2->connect_window_size += con->h2->init_window_size;
        }

        if (con->h2->header[4] == FLAG_ACK)
        {
            if (conf->PrintDebugMsg)
                print_err(con, "recv SETTINGS flag=ACK\n");
            con->h2->ack_recv = true;
            con->h2->con_status = http2::PROCESSING_REQUESTS;
        }
        else
        {
            con->h2->settings.cpy("\x00\x00\x00\x04\x01\x00\x00\x00\x00", 9);
            con->h2->con_status = http2::SEND_SETTINGS;
        }
    }
    else if (con->h2->type == DATA)
    {
        con->client_timer = 0;
        Stream *resp = con->h2->get(con->h2->id);
        if (!resp)
        {
            print_err(con, "<%s:%d> recv DATA: Error list.get(id=%d), h2.body.size()=%d, flag=%d \n", __func__, __LINE__,
                            con->h2->id, con->h2->body.size(), (int)con->h2->header[4]);
            return 0;
        }

        if ((con->h2->body.size() == 0) && (con->h2->header[4] == FLAG_END_STREAM))
        {
            if ((resp->cgi_type <= PHPCGI) || (resp->cgi_type == SCGI))
            {
                resp->cgi_status = CGI_STDOUT;
            }

            if (resp->cgi.to_script > 0)
                close(resp->cgi.to_script);
            resp->cgi.to_script = -1;
            return 0;
        }

        int body_len = con->h2->body.size();
        const char *p_buf = NULL;
        if (con->h2->header[4] & FLAG_PADDED)
        {
            unsigned int padd = (unsigned char)con->h2->body.get_byte(0);
            body_len -= padd;
            p_buf = con->h2->body.ptr() + 1;
        }
        else
            p_buf = con->h2->body.ptr();
        if (conf->PrintDebugMsg)
        {
            if (body_len < 100)
                print_err(resp, "<%s:%d> recv DATA %d, con.cgi_window_size=%ld, stream.cgi_windows_size=%ld, id=%d \n", __func__, __LINE__, body_len, con->h2->cgi_window_size, resp->cgi.windows_size, resp->id);
        }

        resp->post_content_len -= body_len;
        con->h2->cgi_window_size -= body_len;
        resp->cgi.windows_size -= body_len;

        con->h2->cgi_window_update += body_len;
        resp->cgi.window_update += body_len;

        if ((resp->cgi_type <= PHPCGI) || (resp->cgi_type == SCGI))
        {
            resp->post_data.cat(p_buf, body_len);
        }
        else if ((resp->cgi_type == FASTCGI) || (resp->cgi_type == PHPFPM))
        {
            char s[8];
            fcgi_set_header(s, FCGI_STDIN, body_len);
            resp->post_data.cat(s, 8);
            if (body_len)
            {
                resp->post_data.cat(p_buf, body_len);
                if (con->h2->header[4] & FLAG_END_STREAM)
                    resp->post_data.cat("\1\5\0\1\0\0\0\0", 8);
            }

            if (resp->cgi.fcgiContentLen)
            {
                print_err(resp, "<%s:%d> !!! resp->cgi.fcgiContentLen=%d(%lld), id=%d \n", __func__, __LINE__,
                        resp->cgi.fcgiContentLen, resp->post_content_len, resp->id);
            }
            resp->cgi.fcgiContentLen += body_len;
        }

        if (resp->post_content_len < 0)
        {
            print_err(resp, "<%s:%d> !!! Error: cont_length=%lld, body_len=%d, size=%d, id=%d \n", __func__, __LINE__,
                        resp->post_content_len, body_len, resp->post_data.size(), resp->id);
            resp_500(resp);
            return 0;
        }
    }
    else if (con->h2->type == HEADERS)
    {
        if (conf->PrintDebugMsg)
            hex_print_stderr(__func__, __LINE__, con->h2->body.ptr(), con->h2->body.size());
        con->client_timer = 0;
        con->numReq++;
        Stream *resp = con->h2->add(con->numConn, con->numReq);
        if (resp == NULL)
        {
            print_err(con, "<%s:%d> Error id=%d \n", __func__, __LINE__, con->h2->id);
            set_rst_stream(con, con->h2->id, CANCEL);
            return 0;
        }

        if (con->h2->flags & FLAG_END_HEADERS)
        {
            set_response(con, resp);
            if (conf->PrintDebugMsg)
                print_err(resp, "\"%s\" new request headers.size=%d, id=%d \n", resp->decode_path.c_str(), resp->headers.size(), resp->id);
        }
        else
        {
            // frame CONTINUATION not support
            print_err(resp, "<%s:%d> Error: frame CONTINUATION not support, id=%d \n", __func__, __LINE__, con->h2->id);
            set_response(con, resp);
            print_err(resp, "\"%s\" new request headers.size=%d, id=%d \n", resp->decode_path.c_str(), resp->headers.size(), con->h2->id);
            resp_431(resp);
            return 0;
        }
    }
    else if (con->h2->type == CONTINUATION)
    {
        // frame CONTINUATION not support
        print_err(con, "<%s:%d> Error: frame CONTINUATION not support, id=%d \n", __func__, __LINE__, con->h2->id);
        set_rst_stream(con, con->h2->id, CANCEL);
        return 0;
    }
    else if (con->h2->type == GOAWAY)
    {
        if (conf->PrintDebugMsg)
        {
            print_err(con, "recv GOAWAY [%s]\n", get_str_error(con->h2->body.get_byte(7)));
        }

        return -1;
    }
    else if (con->h2->type == RST_STREAM)
    {
        con->client_timer = 0;
        print_err(con, "recv RST_STREAM [%s] id=%d \n", get_str_error(con->h2->body.get_byte(3)), con->h2->id);
        if (con->h2->id == 0)
        {
            set_frame_goaway(con, PROTOCOL_ERROR);
            return 0;
        }

        Stream *resp = con->h2->get(con->h2->id);
        if (resp)
        {
            if (resp->send_data.size() == 0)
            {
                print_log(con, resp);
                con->h2->close_stream(resp->id);
            }
            else
                resp->rst_stream = true;
        }
        else
            print_err(con, "<%s:%d> RST_STREAM Error stream id=%d does not exist\n", __func__, __LINE__, con->h2->id);
    }
    else if (con->h2->type == PING)
    {
        if (conf->PrintDebugMsg)
            hex_print_stderr("recv PING", __LINE__, con->h2->body.ptr(), con->h2->body.size());
        con->client_timer = 0;
        print_err(con, "recv PING\n");
        con->h2->ping.cpy("\x0\x0\x8\x6\x1\x0\x0\x0\x0", 9);
        con->h2->ping.cat(con->h2->body.ptr(), con->h2->body.size());
    }
    else if (con->h2->type == WINDOW_UPDATE)
    {
        con->client_timer = 0;
        long n = 0;
        n += (con->h2->body.get_byte(3));
        n += (con->h2->body.get_byte(2)<<8);
        n += (con->h2->body.get_byte(1)<<16);
        n += (con->h2->body.get_byte(0)<<24);

        if (con->h2->id == 0)
        {
            con->h2->connect_window_size += n;
            if (conf->PrintDebugMsg)
                print_err(con, "<%s:%d> WINDOW_UPDATE %ld[%ld] id=%d \n", __func__, __LINE__, n, con->h2->connect_window_size, con->h2->id);
        }
        else
        {
            con->h2->set_window_size(con->numConn, con->h2->id, n);
            if (conf->PrintDebugMsg)
                print_err(con, "<%s:%d> WINDOW_UPDATE %ld id=%d \n", __func__, __LINE__, n, con->h2->id);
        }
    }
    else if (con->h2->type == PRIORITY)
    {
        con->client_timer = 0;
        if (conf->PrintDebugMsg)
            print_err(con, "<%s:%d> PRIORITY id=%d \n", __func__, __LINE__, con->h2->id);
    }
    else
    {
        con->client_timer = 0;
        //if (conf->PrintDebugMsg)
            print_err(con, "<%s:%d> frame type: %s, id=%d \n", __func__, __LINE__, get_str_frame_type(con->h2->type), con->h2->id);
    }

    return 0;
}
//======================================================================
void EventHandlerClass::send_frames(Connect *con)
{
    int ret = send_frames_(con);
    if (ret < 0)
    {
        if (ret == ERR_TRY_AGAIN)
        {
            con->h2->try_again = true;
            return;
        }
        ssl_shutdown(con);
            return;
    }

    con->h2->try_again = false;
}
//======================================================================
int EventHandlerClass::send_frames_(Connect *con)
{
    if (con->h2->con_status == http2::SEND_SETTINGS)
    {
        if (con->h2->settings.size() == 0)
        {
            print_err(con, "<%s:%d> !!! SEND_SETTINGS Error: settings.size() = 0\n", __func__, __LINE__);
            return -1;
        }

        return send_frame_settings(con);
    }
    else if (con->h2->con_status == http2::PROCESSING_REQUESTS)
    {
        if (con->h2->goaway.size())
        {
            return send_frame_goawey(con);
        }

        if (con->h2->ping.size())
        {
            return send_frame_ping(con);
        }

        if (con->h2->start_list_send_frame)
        {
            return send_frame_rststream(con);
        }

        if (con->h2->frame_win_update.size() || (con->h2->cgi_window_update > 0))
        {
            if (con->h2->cgi_window_update > 32000)
            {
                print_err(con, "<%s:%d> ??? con->h2->server_window_size(%ld) > 32000\n", __func__, __LINE__, con->h2->cgi_window_update);
            }

            if (con->h2->frame_win_update.size() == 0)
                set_frame_window_update(con, con->h2->cgi_window_update);
            int ret = send_window_update(con);
            if (ret < 0)
                return ret;
        }
        //--------------------------------------------------------------
        int n = con->h2->size();
        if (n > conf->MaxConcurrentStreams)
        {
            print_err(con, "<%s:%d> ??? h2.size()=%d\n", __func__, __LINE__, n);
            return -1;
        }
        else if (n == 0)
            return 0;

        if (con->h2->work_stream == NULL)
            return 0;
        Stream *resp = con->h2->work_stream;
        if (resp == NULL)
        {
            if ((resp = con->h2->start_stream) == NULL)
            {
                print_err(con, "<%s:%d> ??? resp == NULL\n", __func__, __LINE__);
                return 0;
            }
        }

        for ( ; resp; resp = con->h2->work_stream)
        {
            if (resp->frame_win_update.size() || (resp->cgi.window_update > 0))
            {
                if (resp->cgi.window_update > 32000)
                {
                    if (conf->PrintDebugMsg)
                        print_err(resp, "<%s:%d> ??? resp->cgi.window_update(%ld) > 32000\n", __func__, __LINE__, resp->cgi.window_update);
                }

                if (resp->frame_win_update.size() == 0)
                    set_frame_window_update(resp, resp->cgi.window_update);
                int ret = send_window_update(con, resp);
                if (ret < 0)
                    return ret;
            }

            if (resp->headers.size())
            {
                int ret = send_frame_headers(con, resp);
                if (ret < 0)
                    return ret;
            }

            if (resp->send_headers && (!resp->send_end_stream))
            {
                int ret = send_frame_data(con, resp);
                if (ret < 0)
                    return ret;
            }

            if (con->h2->work_stream)
                con->h2->work_stream = con->h2->work_stream->next;
        }
    }
    else
    {
        print_err(con, "<%s:%d> !!! Error: connections status (%s)\n", __func__, __LINE__, con->h2->get_str_status());
        return -1;
    }

    return 0;
}
//======================================================================
int EventHandlerClass::send_frame_headers(Connect *con, Stream *resp)
{
    if (resp->headers.size() && (!resp->send_headers))
    {
        int ret = write_to_client(con, resp->headers.ptr_remain(), resp->headers.size_remain(), resp->id);
        if (ret < 0)
        {
            if (ret == ERR_TRY_AGAIN)
            {
                //print_err(resp, "<%s:%d> Error send frame HEADERS: SSL_ERROR_WANT_WRITE, id=%d \n", __func__, __LINE__, resp->id);
            }
            else
                print_err(resp, "<%s:%d> Error send frame HEADERS: %d, id=%d \n", __func__, __LINE__, ret, resp->id);
            return ret;
        }

        con->client_timer = 0;
        resp->headers.set_offset(ret);
        if (resp->headers.size_remain())
            return ERR_TRY_AGAIN;
        resp->send_headers = true;
        if (resp->headers.get_byte(4) & FLAG_END_STREAM)
        {
            if (conf->PrintDebugMsg)
            {
                print_err(resp, "<%s:%d>... send frame HEADERS, END_STREAM, [%s] send %lld bytes ... id=%d \n", 
                        __func__, __LINE__, resp->clean_decode_path, resp->send_bytes, resp->id);
            }

            resp->send_end_stream = true;
            print_log(con, resp);
            con->h2->close_stream(resp->id);
        }
        else
        {
            if (conf->PrintDebugMsg)
            {
                print_err(resp, "<%s:%d> send frame HEADERS: %d, id=%d \n", __func__, __LINE__, ret, resp->id);
                hex_print_stderr(__func__, __LINE__, resp->headers.ptr(), resp->headers.size());
            }
            resp->headers.init();
        }

        return 0;
    }
    else
    {
        print_err(resp, "<%s:%d> !!! Error id=%d \n", __func__, __LINE__, resp->id);
        return -1;
    }
}
//=============================================================================================================================
int EventHandlerClass::send_frame_data(Connect *con, Stream *resp)
{
    if (resp->send_data.size() == 0)
    {
        if (resp->rst_stream)
        {
            if (resp->source_data == DYN_PAGE)
                set_rst_stream(con, resp->id, CANCEL);
            else
            {
                print_log(con, resp);
                con->h2->close_stream(resp->id);
            }

            return 0;
        }
        else
        {
            if (con->h2->connect_window_size <= 0)
            {
                //print_err(resp, "<%s:%d> !!! connect_window_size(%ld) <= 0, id=%d \n", __func__, __LINE__, con->h2->connect_window_size, resp->id);
                return 0;
            }

            if (resp->stream_window_size <= 0)
            {
                //print_err(resp, "<%s:%d> !!! stream_window_size(%ld) <= 0, %ld, id=%d \n", __func__, __LINE__, resp->stream_window_size, con->h2->connect_window_size, resp->id);
                return 0;
            }

            int ret = set_frame_data(con, resp);
            if (ret < 0)
                return ret;
            else if (ret == 0)
                return 0;
        }
    }

    int ret = write_to_client(con, resp->send_data.ptr_remain(), resp->send_data.size_remain(), resp->id);
    if (ret < 0)
    {
        if (ret == ERR_TRY_AGAIN)
        {
            //print_err(resp, "<%s:%d> Error send frame DATA: %d, %d, id=%d \n", __func__, __LINE__,
            //                                    ret, resp->send_data.size(), resp->id);
        }
        else
        {
            print_err(resp, "<%s:%d> Error send frame DATA: %d, %d, send_bytes=%lld, id=%d \n", __func__, __LINE__,
                                                ret, resp->send_data.size(), resp->send_bytes, resp->id);
            resp->send_data.init();
        }

        return ret;
    }

    con->client_timer = 0;
    resp->send_data.set_offset(ret);
    if (resp->send_data.size_remain())
        return ERR_TRY_AGAIN;
    else
    {
        resp->send_bytes += (resp->send_data.size() - 9);
        resp->stream_window_size -= (resp->send_data.size() - 9);
        con->h2->connect_window_size -= (resp->send_data.size() - 9);
    }

    if (conf->PrintDebugMsg)
    {
        print_err(resp, "<%s:%d> send frame DATA: %d, send_bytes=%lld, id=%d \n", __func__, __LINE__,
                                                ret, resp->send_bytes, resp->id);
    }

    if ((resp->send_data.get_byte(4) & FLAG_END_STREAM) || resp->rst_stream)
    {
        if (conf->PrintDebugMsg)
        {
            print_err(resp, "<%s:%d>... send frame DATA, END_STREAM, [%s] send %lld bytes, data.size=%d ... id=%d \n", 
                        __func__, __LINE__, resp->clean_decode_path, resp->send_bytes, resp->send_data.size(), resp->id);
        }

        resp->send_end_stream = true;
        print_log(con, resp);
        con->h2->close_stream(resp->id);
        return 0;
    }
    else
        resp->send_data.init();

    return 0;
}
//======================================================================
int EventHandlerClass::send_frame_settings(Connect *con)
{
    if (con->h2->settings.size() == 0)
    {
        print_err(con, "<%s:%d> !!! SEND_SETTINGS Error: settings.size() = 0\n", __func__, __LINE__);
        return -1;
    }

    int ret = write_to_client(con, con->h2->settings.ptr_remain(), con->h2->settings.size_remain(), 0);
    if (ret < 0)
    {
        print_err(con, "<%s:%d> Error send frame SETTINGS\n", __func__, __LINE__);
        if (ret != ERR_TRY_AGAIN)
            con->h2->settings.init();
        return ret;
    }

    if (conf->PrintDebugMsg)
        hex_print_stderr(__func__, __LINE__, con->h2->settings.ptr(), con->h2->settings.size());

    con->client_timer = 0;
    con->h2->settings.set_offset(ret);
    if (con->h2->settings.size_remain())
        return ERR_TRY_AGAIN;
    con->h2->settings.init();
    if (con->h2->ack_recv)
        con->h2->con_status = http2::PROCESSING_REQUESTS;
    else
        con->h2->con_status = http2::RECV_SETTINGS;
    return 0;
}
//======================================================================
int EventHandlerClass::send_frame_ping(Connect *con)
{
    int ret = write_to_client(con, con->h2->ping.ptr_remain(), con->h2->ping.size_remain(), 0);
    if (ret < 0)
    {
        print_err(con, "<%s:%d> Error send frame PING, id=0 \n", __func__, __LINE__);
        if (ret != ERR_TRY_AGAIN)
            con->h2->ping.init();
        return ret;
    }

    if (conf->PrintDebugMsg)
        hex_print_stderr(__func__, __LINE__, con->h2->ping.ptr_remain(), con->h2->ping.size_remain());
    con->h2->ping.set_offset(ret);
    if (con->h2->ping.size_remain())
        return ERR_TRY_AGAIN;
    con->h2->ping.init();
    return 0;
}
//======================================================================
int EventHandlerClass::send_frame_goawey(Connect *con)
{
    int ret = write_to_client(con, con->h2->goaway.ptr_remain(), con->h2->goaway.size_remain(), 0);
    if (ret < 0)
    {
        print_err(con, "<%s:%d> Error send frame GOAWAY, id=0 \n", __func__, __LINE__);
        if (ret != ERR_TRY_AGAIN)
            con->h2->goaway.init();
        return ret;
    }

    if (conf->PrintDebugMsg)
        hex_print_stderr(__func__, __LINE__, con->h2->goaway.ptr_remain(), con->h2->goaway.size_remain());
    con->client_timer = 0;
    con->h2->goaway.set_offset(ret);
    if (con->h2->goaway.size_remain())
        return ERR_TRY_AGAIN;
    con->h2->goaway.init();
    return -1;
}
//======================================================================
int EventHandlerClass::send_frame_rststream(Connect *con)
{
    FrameRedySend *rf = con->h2->start_list_send_frame;
    if (!rf)
    {
        return 0;
    }

    int ret = write_to_client(con, rf->frame.ptr_remain(), rf->frame.size_remain(), 0);
    if (ret < 0)
    {
        print_err(con, "<%s:%d> Error send frame GOAWAY, id=0 \n", __func__, __LINE__);
        if (ret != ERR_TRY_AGAIN)
            con->h2->del_from_list(rf);
        return ret;
    }

    if (conf->PrintDebugMsg)
        hex_print_stderr(__func__, __LINE__, rf->frame.ptr_remain(), rf->frame.size_remain());
    con->client_timer = 0;
    rf->frame.set_offset(ret);
    if (rf->frame.size_remain())
        return ERR_TRY_AGAIN;
    con->h2->del_from_list(rf);
    return 0;
}
//======================================================================
int EventHandlerClass::send_window_update(Connect *con)
{
    int ret = 0;
    if ((ret = write_to_client(con, con->h2->frame_win_update.ptr_remain(), con->h2->frame_win_update.size_remain(), 0)) <= 0)
    {
        print_err(con, "<%s:%d> Error send frame WINDOW_UPDATE: %d, %d, id=0 \n", __func__, __LINE__, ret, con->h2->frame_win_update.size_remain());
        return ret;
    }

    con->client_timer = 0;
    con->h2->frame_win_update.set_offset(ret);
    if (con->h2->frame_win_update.size_remain())
        return ERR_TRY_AGAIN;
    con->h2->cgi_window_size += con->h2->cgi_window_update;
    con->h2->cgi_window_update = 0;
    con->h2->frame_win_update.init();
    return 0;
}
//======================================================================
int EventHandlerClass::send_window_update(Connect *con, Stream *resp)
{
    int ret = 0;
    if ((ret = write_to_client(con, resp->frame_win_update.ptr_remain(), resp->frame_win_update.size_remain(), resp->id)) <= 0)
    {
        print_err(resp, "<%s:%d> Error send frame WINDOW_UPDATE: %d, %d, id=%d \n", __func__, __LINE__, ret, resp->frame_win_update.size_remain(), resp->id);
        return ret;
    }

    con->client_timer = 0;
    resp->frame_win_update.set_offset(ret);
    if (resp->frame_win_update.size_remain())
        return ERR_TRY_AGAIN;
    resp->cgi.windows_size += resp->cgi.window_update;
    resp->cgi.window_update = 0;
    resp->frame_win_update.init();
    return 0;
}
//======================================================================
const char *static_tab[][2] = {
                         {"", ""},
                         {":authority", ""},
                         {":method", "GET"},
                         {":method", "POST"},
                         {":path", "/"},
                         {":path", "/index.html"},
                         {":scheme", "http"},
                         {":scheme", "https"},
                         {":status", "200"},
                         {":status", "204"},
                         {":status", "206"},
                         {":status", "304"},
                         {":status", "400"},
                         {":status", "404"},
                         {":status", "500"},
                         {"accept-charset", ""},
                         {"accept-encoding", "gzip, deflate"},
                         {"accept-language", ""},
                         {"accept-ranges", ""},
                         {"accept", ""},
                         {"access-control-allow-origin", ""},
                         {"age", ""},
                         {"allow", ""},
                         {"authorization", ""},
                         {"cache-control", ""},
                         {"content-disposition", ""},
                         {"content-encoding", ""},
                         {"content-language", ""},
                         {"content-length", ""},
                         {"content-location", ""},
                         {"content-range", ""},
                         {"content-type", ""},
                         {"cookie", ""},
                         {"date", ""},
                         {"etag", ""},
                         {"expect", ""},
                         {"expires", ""},
                         {"from", ""},
                         {"host", ""},
                         {"if-match", ""},
                         {"if-modified-since", ""},
                         {"if-none-match", ""},
                         {"if-range", ""},
                         {"if-unmodified-since", ""},
                         {"last-modified", ""},
                         {"link", ""},
                         {"location", ""},
                         {"max-forwards", ""},
                         {"proxy-authenticate", ""},
                         {"proxy-authorization", ""},
                         {"range", ""},
                         {"referer", ""},
                         {"refresh", ""},
                         {"retry-after", ""},
                         {"server", ""},
                         {"set-cookie", ""},
                         {"strict-transport-security", ""},
                         {"transfer-encoding", ""},
                         {"user-agent", ""},
                         {"vary", ""},
                         {"via", ""},
                         {"www-authenticate", ""},
                         {NULL, NULL}};
