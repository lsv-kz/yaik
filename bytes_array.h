#ifndef BYTESARRAY_H_
#define BYTESARRAY_H_

#include <iostream>
#include <cstring>
#include <unistd.h>
//======================================================================
class ByteArray
{
    char *buf;
    int buf_size;
    int buf_len;
    int offset;
    //------------------------------------------------------------------
    int resize(int size_new)
    {
        if ((buf_size >= size_new) ||(size_new <= 0))
            return -1;
        char *tmp_buf = new(std::nothrow) char [size_new];
        if (!tmp_buf)
        {
            fprintf(stderr, "<%s:%d> Error new char [%d]\n", __func__, __LINE__, size_new);
            return -1;
        }

        if (buf)
        {
            if (buf_len > 0)
                memcpy(tmp_buf, buf, buf_len + 1);
            delete [] buf;
        }

        buf = tmp_buf;
        buf_size = size_new;

        return 0;
    }

    ByteArray(const ByteArray&);
    ByteArray& operator=(const ByteArray&);

public:

    ByteArray()
    {
        buf = NULL;
        buf_size = buf_len = offset = 0;
    }
    //------------------------------------------------------------------
    ~ByteArray()
    {
        if (buf)
        {
            delete [] buf;
            buf = NULL;
        }
    }
    //------------------------------------------------------------------
    void init()
    {
        buf_len = offset = 0;
    }
    //------------------------------------------------------------------
    void reserve(unsigned int n)
    {
        resize(n);
    }
    //------------------------------------------------------------------
    int cpy(const char *b, int len)
    {
        if (len <= 0)
        {
            fprintf(stderr, "<%s:%d> len=%d\n", __func__, __LINE__, len);
            return -1;
        }

        if (buf_size <= len)
        {
            if (resize(len + 64))
                return -1;
        }

        memcpy(buf, b, len);
        buf_len = len;
        offset = 0;
        buf[buf_len] = 0;
        return 0;
    }
    //------------------------------------------------------------------
    int cat(const char *b, int len)
    {
        if (len <= 0)
        {
            fprintf(stderr, "<%s:%d> len=%d\n", __func__, __LINE__, len);
            return -1;
        }

        if (buf_size <= (buf_len + len))
        {
            if (resize(buf_len + len + 64))
                    return -1;
        }

        memcpy(buf + buf_len, b, len);
        buf_len += len;
        buf[buf_len] = 0;
        return 0;
    }
    //------------------------------------------------------------------
    int cat(const char b)
    {
        if (buf_size <= (buf_len + 1))
        {
            if (resize(buf_len + 64))
                    return -1;
        }

        buf[buf_len] = b;
        buf_len += 1;
        buf[buf_len] = 0;
        return 0;
    }
    //------------------------------------------------------------------
    int cpy_str(const char *s)
    {
        int len = (int)strlen(s);
        if (len)
            return cpy(s, len);
        else
        {
            buf_len = 0;
            offset = 0;
            return 0;
        }
    }
    //------------------------------------------------------------------
    int cat_str(const char *s)
    {
        int len = (int)strlen(s);
        if (len)
            return cat(s, len);
        else
        {
            return 0;
        }
    }
    //------------------------------------------------------------------
    const char *ptr()
    {
        if (buf)
        {
            buf[buf_len] = 0;
            return buf;
        }
        else
            return "";
    }
    //------------------------------------------------------------------
    const char *ptr_remain()
    {
        if (buf)
        {
            buf[buf_len] = 0;
            return buf + offset;
        }
        else
            return "";
    }
    //------------------------------------------------------------------
    char *data(int n)
    {
        if (n < 0)
            return NULL;
        if (buf_size <= n)
        {
            if (resize(n + 64))
                return NULL;
        }

        buf_len = 0;
        offset = 0;
        return buf;
    }
    //------------------------------------------------------------------
    int set_len(int n)
    {
        if ((n < 0) || (buf_len >= buf_size))
            return buf_len;
        buf_len = n;
        return buf_len;
    }
    //------------------------------------------------------------------
    int read_file(int fd, int len)
    {
        if (len < 0)
            return -1;
        if (buf_size <= len)
        {
            if (resize(len + 64))
                return -1;
        }

        offset = 0;
        int ret = read(fd, buf, len);
        if (ret < 0)
        {
            fprintf(stderr, "<%s:%d> Error read(): %s\n", __func__, __LINE__, strerror(errno));
            return -1;
        }

        buf_len = ret;
        return ret;
    }
    //------------------------------------------------------------------
    int get_byte(int i)
    {
        if ((i >= buf_len) || (i < 0))
            return -1;
        return (unsigned char)buf[i];
    }
    //------------------------------------------------------------------
    int set_byte(char ch, int i)
    {
        if ((i < 0) || (i >= buf_len) || (buf_size == 0))
            return -1;
        else
        {
            buf[i] = ch;
            return 0;
        }
    }
    //------------------------------------------------------------------
    int size() { return buf_len; }
    int size_remain() { return buf_len - offset; }
    int get_offset() { return offset; }

    int set_offset(int n)
    {
        if (((offset + n) < 0) || ((offset + n) > buf_len))
        {
            fprintf(stderr, "<%s:%d> Error new offset=%d, buf_len=%d\n", __func__, __LINE__, offset + n, buf_len);
            return -1;
        }
        else if (n == 0)
        {
            fprintf(stderr, "<%s:%d> n=%d\n", __func__, __LINE__, n);
            return offset;
        }

        offset += n;
        return offset;
    }
};

#endif
