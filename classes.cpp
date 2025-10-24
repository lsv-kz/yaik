#include "main.h"

//======================================================================
http2::http2()
{
    try_again = ack_recv = false;
    header_len = id = body_len = 0;
    init_window_size = 65535;
    connect_window_size = 0;
    max_frame_size = 0;
    cgi_window_update = 0;
    cgi_window_size = 0;

    max_streams = conf->MaxConcurrentStreams;
    num_streams = err = 0;
    work_stream = start_stream = end_stream = NULL;
    start_list_send_frame = end_list_send_frame = NULL;

    settings.cpy("\x00\x00\x12\x04\x00\x00\x00\x00\x00" // SETTINGS (type=0x4)
                "\x00\x01\x00\x00\x00\x00"              // SETTINGS_HEADER_TABLE_SIZE (0x1)  size of the dynamic table
                "\x00\x03\x00\x00\x00\x00"              // SETTINGS_MAX_CONCURRENT_STREAMS (0x3)
                "\x00\x04\x00\x00\x3e\x80", 9 + 18);    // SETTINGS_INITIAL_WINDOW_SIZE (0x4)

    settings.set_byte((conf->HeaderTableSize>>24) & 0xff, 11);
    settings.set_byte((conf->HeaderTableSize>>16) & 0xff, 12);
    settings.set_byte((conf->HeaderTableSize>>8) & 0xff, 13);
    settings.set_byte(conf->HeaderTableSize & 0xff, 14);

    settings.set_byte((conf->MaxConcurrentStreams>>24) & 0xff, 17);
    settings.set_byte((conf->MaxConcurrentStreams>>16) & 0xff, 18);
    settings.set_byte((conf->MaxConcurrentStreams>>8) & 0xff, 19);
    settings.set_byte(conf->MaxConcurrentStreams & 0xff, 20);
    //------------------------------------------------------------------
    if (conf->HeaderTableSize > 0)
    {
        dyn_tab = new(std::nothrow) DynamicTable(conf->HeaderTableSize, 62);
        if (!dyn_tab)
        {
            print_err("<%s:%d>Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
            exit(1);
        }
    }
    else
        dyn_tab = NULL;
}
//----------------------------------------------------------------------
http2::~http2()
{
    if (dyn_tab)
        delete dyn_tab;

    Stream *r = start_stream, *next = NULL;
    for ( ; r; r = next)
    {
        next = r->next;
        if (conf->PrintDebugMsg)
            print_err(r, "<%s:%d>~~~~~~~ Close Stream, id=%d \n", __func__, __LINE__, r->id);
        delete r;
    }

    FrameRedySend *fr = start_list_send_frame, *fr_next = NULL;
    for ( ; fr; fr = fr_next)
    {
        fr_next = fr->next;
        delete fr;
    }

    if (conf->PrintDebugMsg)
        print_err("<%s:%d> ~~~~ Close Connect\n", __func__, __LINE__);
    start_stream = end_stream = NULL;
}
//----------------------------------------------------------------------
Stream *http2::add(unsigned long numConn, unsigned long numReq)
{
    if (num_streams >= max_streams)
    {
        print_err("<%s:%d> Error: num streams: %d\n", __func__, __LINE__, num_streams);
        return NULL;
    }

    Stream *resp = NULL;
    resp = new(std::nothrow) Stream;
    if (!resp)
    {
        print_err("<%s:%d> Error: %s\n", __func__, __LINE__, strerror(errno));
        return NULL;
    }

    resp->numConn = numConn;
    resp->numReq = numReq;

    resp->type = type;
    resp->flags = flags;
    resp->id = id;

    if (init_window_size > 0)
    {
        resp->stream_window_size = init_window_size;
    }
    else
    {
        resp->stream_window_size = 65535;
    }

    int ret = parse(resp);
    if (ret)
    {
        delete resp;
        return NULL;
    }

    resp->prev = end_stream;
    resp->next = NULL;
    if (start_stream)
    {
        end_stream->next = resp;
        end_stream = resp;
    }
    else
        start_stream = end_stream = resp;
    ++num_streams;

    if (conf->PrintDebugMsg)
        print_err(resp, "<%s:%d> num streams: %d, id=%d\n", __func__, __LINE__, num_streams, id);
    return resp;
}
//----------------------------------------------------------------------
void http2::del_from_list(Stream *r)
{
    if (r->prev && r->next)
    {
        r->prev->next = r->next;
        r->next->prev = r->prev;
    }
    else if (r->prev && !r->next)
    {
        r->prev->next = r->next;
        end_stream = r->prev;
    }
    else if (!r->prev && r->next)
    {
        r->next->prev = r->prev;
        start_stream = r->next;
    }
    else if (!r->prev && !r->next)
        start_stream = end_stream = NULL;
    --num_streams;
}
//----------------------------------------------------------------------
int http2::close_stream(int id)
{
    Stream *r = start_stream, *next = NULL;
    for ( ; r; r = next)
    {
        next = r->next;
        if (r->id == id)
        {
            if (work_stream == r)
            {
                work_stream = next;
            }

            if (r->cgi.start)
            {
                if (conf->PrintDebugMsg)
                    print_err("<%s:%d>~~~~~~~ close cgi stream, id=%d \n", __func__, __LINE__, r->id);
                if (r->cgi_type <= PHPCGI)
                {
                    kill_chld(r->cgi.pid);
                }
            }

            if (conf->PrintDebugMsg)
                print_err("<%s:%d>~~~~~~~ Close Stream, id=%d \n", __func__, __LINE__, r->id);
            del_from_list(r);
            delete r;
            return id;
        }
    }

    return -1;
}
//----------------------------------------------------------------------
int http2::set_window_size(unsigned long num_conn, int id, long n)
{
    Stream *r = start_stream, *next = NULL;
    for ( ; r; r = next)
    {
        next = r->next;
        if (r->id == id)
        {
            r->stream_window_size += n;
            return id;
        }
    }

    return id;
}
//----------------------------------------------------------------------
Stream *http2::get(int id)
{
    Stream *r = start_stream, *next = NULL;
    for ( ; r; r = next)
    {
        next = r->next;
        if (r->id == id)
            return r;
    }

    return NULL;
}
//----------------------------------------------------------------------
Stream *http2::get()
{
    return start_stream;
}
//----------------------------------------------------------------------
int http2::size()
{
    return num_streams;
}
//----------------------------------------------------------------------
void http2::push_to_list(FrameRedySend *rf)
{
    rf->next = NULL;
    rf->prev = end_list_send_frame;
    if (start_list_send_frame)
    {
        end_list_send_frame->next = rf;
        end_list_send_frame = rf;
    }
    else
        start_list_send_frame = end_list_send_frame = rf;
}
//----------------------------------------------------------------------
void http2::del_from_list(FrameRedySend *rf)
{
    if (rf->prev && rf->next)
    {
        rf->prev->next = rf->next;
        rf->next->prev = rf->prev;
    }
    else if (rf->prev && !rf->next)
    {
        rf->prev->next = rf->next;
        end_list_send_frame = rf->prev;
    }
    else if (!rf->prev && rf->next)
    {
        rf->next->prev = rf->prev;
        start_list_send_frame = rf->next;
    }
    else if (!rf->prev && !rf->next)
        start_list_send_frame = end_list_send_frame = NULL;
    delete rf;
}
//----------------------------------------------------------------------
int http2::get_str(std::string& str, int *len)
{
    int ch;

    if ((ch = body.get_byte((*len)++)) < 0)
    {
        fprintf(stderr, "<%s:%d> Error body.get_byte()=%d\n", __func__, __LINE__, ch);
        return -1;
    }

    bool huffman = ch & 0x80;
    int val_len = ch & 0x7f;
    if (val_len == 0x7f)
    {
        val_len = bytes_to_int(val_len, 7, body.ptr(), len, body.size());
        if (val_len <= 0)
        {
            fprintf(stderr, "<%s:%d> Error bytes_to_int()=%d\n", __func__, __LINE__, val_len);
            return -1;
        }
    }

    if ((val_len + *len) > body.size())
    {
        fprintf(stderr, "<%s:%d> Error out of range [%d > %d]\n", __func__, __LINE__, val_len + *len, body.size());
        return -1;
    }

    if (huffman)
        huffman_decode(body.ptr() + *len, val_len, str);
    else
        str.assign(body.ptr() + *len, val_len);
    (*len) += val_len;
    return 0;
}
//----------------------------------------------------------------------
int http2::get_header(int ind, std::string& name, std::string& val, int *len)
{
    if (ind == 0x00)
    {
        if (get_str(name, len) < 0)
            return -1;
    }
    else
    {
        if (ind > 61)
        {
            Header *hd = dyn_tab->get(ind);
            if (hd)
                name = hd->name;
            else
            {
                name = "?";
                //return -1;
            }
        }
        else
            name = static_tab[ind][0];
    }

    if (get_str(val, len) < 0)
        return -1;
    return 0;
}
//----------------------------------------------------------------------
int http2::parse(Stream *r)
{
    int len = 0;
    int ch;
    if (flags & 0x08) // PADDED (0x8)
        ++len;
    if (flags & 0x20) // PRIORITY (0x20)
        len += 5;
    std::string name;
    name.reserve(32);
    std::string val;
    val.reserve(128);

    for ( ; len < body.size(); )
    {
        if ((ch = body.get_byte(len++)) < 0)
        {
            fprintf(stderr, "<%s:%d> Error ch=%d, 0x%X\n", __func__, __LINE__, ch, ch);
            return -1;
        }

        if (ch > 0x80)
        {// <0x81 ... 0xFF> ; static table: [0x81 ... 0x3D], dynamic table: [0x3E ...]
            int ind = bytes_to_int(ch & 0x7f, 7, body.ptr(), &len, body.size());
            if (ind > 61)
            {
                Header *hd = dyn_tab->get(ind);
                if (hd)
                {
                    name = hd->name;
                    val = hd->val;
                }
                else
                {
                    name = "?";
                    val = "?";
                    //return -1;
                }
            }
            else
            {
                name = static_tab[ind][0];
                val = static_tab[ind][1];
            }
        }
        else if ((ch >= 0x40) && (ch <= 0x7f))
        {// <0x40><len><name><len><val>, <0x41 ... 0x7F><index><len><val> ---> dyn_tab
            int ind = bytes_to_int(ch & 0x3f, 6, body.ptr(), &len, body.size());
            if (get_header(ind, name, val, &len) < 0)
                return -1;
            if (conf->PrintDebugMsg)
                fprintf(stderr, "<%s:%d> [%s: %s]\n", __func__, __LINE__, name.c_str(), val.c_str());
            if ((conf->HeaderTableSize > 0) && dyn_tab)
                dyn_tab->add(name.c_str(), val.c_str());
        }
        else if ((ch >= 0x00) && (ch <= 0x0f))
        {// <0x00><len><name><len><val>, <0x01 ... 0x0F><index><len><val>
            int ind = bytes_to_int(ch, 4, body.ptr(), &len, body.size());
            if (get_header(ind, name, val, &len) < 0)
                return -1;
        }
        else if ((ch >= 0x10) && (ch <= 0x1f))
        {// <0x10><len><name><len><val>, <0x11 ... 0x1F><index><len><val>
            int ind = bytes_to_int(ch & 0x0f, 4, body.ptr(), &len, body.size());
            if (get_header(ind, name, val, &len) < 0)
                return -1;
        }
        else if ((ch >= 0x20) && (ch <= 0x3f))
        {// Dynamic Table Size Update
            int size = bytes_to_int(ch & 0x1f, 5, body.ptr(), &len, body.size());
            fprintf(stderr, "[%lu/%lu] <%s:%d>--- Dynamic Table Size Update: %d ---\n", r->numConn, r->numReq, __func__, __LINE__, size);
            continue;
        }
        else
        {
            fprintf(stderr, "<%s:%d> !!! 0x%02X\n", __func__, __LINE__, ch);
            return -1;
        }

        if (conf->PrintDebugMsg)
            fprintf(stderr, "[%lu/%lu] <%s:%d> [%s: %s]\n", r->numConn, r->numReq, __func__, __LINE__, name.c_str(), val.c_str());

        if (name == ":method")
        {
            r->httpMethod = get_int_method(val.c_str());
            if (r->httpMethod == 0)
            {
                print_err("<%s:%d> Error http method: %s, id=%d \n", __func__, __LINE__, val.c_str(), r->id);
                return -1;
            }
        }
        else if (name == ":path")
            r->path = val;
        else if (name == "host")
            r->host = val;
        else if (name == "user-agent")
            r->user_agent = val;
        else if (name == "referer")
            r->referer = val;
        else if (name == "range")
        {
            if (conf->PrintDebugMsg)
                fprintf(stderr, "<%s:%d> [%s: %s]\n", __func__, __LINE__, name.c_str(), val.c_str());
            r->range = val;
        }
        else if (name == ":authority")
            r->host = val;
        else if (name == "content-length")
        {
            r->sReqContentLen = val;
            try
            {
                r->post_content_len = stoll(val, NULL, 10);
            }
            catch (...)
            {
                print_err("<%s:%d> Error stoll(\"%s\")\n", __func__, __LINE__, val.c_str());
                return -1;
            }
        }
        else if (name == "content-type")
        {
            r->sReqContentType = val;
        }
        else
        {
            if (conf->PrintDebugMsg)
                fprintf(stderr, "<%s:%d> [%s: %s]\n", __func__, __LINE__, name.c_str(), val.c_str());
        }
    }

    if ((conf->PrintDebugMsg) && dyn_tab)
        dyn_tab->print();
    return 0;
}
