#ifndef BYTESARRAY_H_
#define BYTESARRAY_H_

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>

extern const unsigned int array_reserve;
//======================================================================
class ByteArray
{
    char int_buf[21];
    char *buf;
    unsigned int buf_size;
    unsigned int buf_len;
    unsigned int offset;
    int err;
    //------------------------------------------------------------------
    char *ll_to_string(long long ll)
    {
        if (err)
            return NULL;
        int cnt = 20, minus = (ll < 0) ? 1 : 0;
        const char *get_char = "9876543210123456789";
    
        int_buf[cnt] = 0;
        while (cnt > 0)
        {
            int_buf[--cnt] = get_char[9 + ll % 10];
            ll /= 10;
            if (ll == 0)
                break;
        }
    
        if ((minus) && (cnt > 0))
            int_buf[--cnt] = '-';
    
        return int_buf + cnt;
    }
    //------------------------------------------------------------------
    ByteArray(const ByteArray&);
    ByteArray& operator=(const ByteArray&);

public:

    ByteArray()
    {
        buf = NULL;
        buf_size = buf_len = 0;
        offset = 0;
        err = 0;
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
        buf_len = 0;
        offset = 0;
        err = 0;
    }
    //------------------------------------------------------------------
    int reserve(unsigned int size_new)
    {
        if (err)
            return -1;
        if ((buf_size >= size_new) || (size_new == 0))
            return buf_size;
        size_new += array_reserve;
        char *tmp_buf = new(std::nothrow) char [size_new];
        if (!tmp_buf)
        {
            fprintf(stderr, "<%s:%d> Error new char [%d]\n", __func__, __LINE__, size_new);
            err = -1;
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
    //------------------------------------------------------------------
    int cpy(const char *b, unsigned int len)
    {
        if ((b == NULL) || (len == 0) || err)
        {
            fprintf(stderr, "<%s:%d> %p, len=%d, error=%d\n", __func__, __LINE__, b, len, err);
            return -1;
        }

        if (buf_size <= len)
        {
            if (reserve(len + 1))
                return -1;
        }

        memcpy(buf, b, len);
        buf_len = len;
        offset = 0;
        buf[buf_len] = 0;
        return 0;
    }
    //------------------------------------------------------------------
    int cat(const char *b, unsigned int len)
    {
        if ((b == NULL) || (len == 0) || err)
        {
            fprintf(stderr, "<%s:%d> %p, len=%d, error=%d\n", __func__, __LINE__, b, len, err);
            return -1;
        }

        if (buf_size <= (buf_len + len))
        {
            if (reserve(buf_len + len + 1))
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
        if (err)
            return -1;
        if (buf_size <= (buf_len + 1))
        {
            if (reserve(buf_len + 2))
                return -1;
        }

        buf[buf_len] = b;
        buf_len += 1;
        buf[buf_len] = 0;
        return 0;
    }
    //------------------------------------------------------------------
    int cpy_int(long long ll) { return cpy_str(ll_to_string(ll)); }
    //------------------------------------------------------------------
    int cat_int(long long ll) { return cat_str(ll_to_string(ll)); }
    //------------------------------------------------------------------
    int cpy_str(const char *s)
    {
        if ((s == NULL) || err)
            return -1;
        unsigned int len = strlen(s);
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
        if ((s == NULL) || err)
            return -1;
        unsigned int len = strlen(s);
        if (len)
            return cat(s, len);
        else
            return 0;
    }
    //------------------------------------------------------------------
    void cat_time()
    {
        if (err)
            return;
        struct tm t;
        char s[40];
        time_t now = time(NULL);
    
        gmtime_r(&now, &t);
        unsigned int len = strftime(s, sizeof(s), "%a, %d %b %Y %H:%M:%S %Z", &t);
        cat(s, len);
    }
    //------------------------------------------------------------------
    void cat_logtime()
    {
        if (err)
            return;
        struct tm t;
        char s[40];
        time_t now = time(NULL);
    
        localtime_r(&now, &t);
        unsigned int len = strftime(s, sizeof(s), "%d/%b/%Y:%H:%M:%S %Z", &t);
        cat(s, len);
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
    int set_len(unsigned int n)
    {
        if (n >= buf_size)
            return buf_len;
        buf_len = n;
        return buf_len;
    }
    //------------------------------------------------------------------
/*    int read_file(int fd, unsigned int len)
    {
        if ((len == 0) || err)
            return -1;
        if (buf_size <= len)
        {
            if (reserve(len + 1))
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
    }*/
    //------------------------------------------------------------------
    int get_byte(unsigned int i)
    {
        if ((i >= buf_len) || err)
            return -1;
        return (unsigned char)buf[i];
    }
    //------------------------------------------------------------------
    int set_byte(char ch, unsigned int i)
    {
        if ((i >= buf_len) || (buf_size == 0) || err)
            return -1;
        else
        {
            buf[i] = ch;
            return 0;
        }
    }
    //------------------------------------------------------------------
    unsigned int size() { return buf_len; }
    unsigned int capacity() { return buf_size; }
    unsigned int size_remain() { return buf_len - offset; }
    unsigned int get_offset() { return offset; }
    int error() { return err; }

    unsigned int set_offset(unsigned int n)
    {
        if ((offset + n) > buf_len)
        {
            fprintf(stderr, "<%s:%d> Error new offset=%u, buf_len=%u\n", __func__, __LINE__, offset + n, buf_len);
            return 0;
        }
        else if (n == 0)
        {
            fprintf(stderr, "<%s:%d> n=%u\n", __func__, __LINE__, n);
            return offset;
        }

        offset += n;
        return offset;
    }
};

#endif
