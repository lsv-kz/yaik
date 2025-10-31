#include "main.h"

using namespace std;
//======================================================================
void init_openssl()
{
    SSL_library_init();
    OpenSSL_add_ssl_algorithms();
    //OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_crypto_strings();
}
//======================================================================
void cleanup_openssl()
{
    EVP_cleanup();
}
//======================================================================
SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    method = TLS_server_method();
    //method = TLSv1_2_server_method();
    return SSL_CTX_new(method);
}
//======================================================================
static int alpn_select_proto_cb(SSL *ssl, const unsigned char **out, unsigned char *outlen,
                                const unsigned char *in, unsigned int inlen, void *arg)
{
    if (conf->PrintDebugMsg)
        hex_print_stderr(__func__, __LINE__, in, inlen);
    const char *p = proto_alpn;
    unsigned int proto_alpn_len = sizeof(proto_alpn); 
    if (conf->SelectHTTP2)
    {
        p = proto_alpn_h2;
        proto_alpn_len = sizeof(proto_alpn_h2); 
    }
    for ( unsigned int i = 0; i < proto_alpn_len; i += (unsigned int)(p[i] + 1))
    {
        for (unsigned int j = 0; j < inlen; j += (unsigned int)(in[j] + 1))
        {
            if (in[j] != p[i])
                continue;
            if (memcmp(&in[j], &p[i], in[j] + 1) == 0)
            {
                *out = (unsigned char *)&in[j + 1];
                *outlen = in[j];
                return SSL_TLSEXT_ERR_OK;
            }
        }
    }

    return SSL_TLSEXT_ERR_NOACK;
}
//======================================================================
int configure_context(SSL_CTX *ctx)
{
    if (SSL_CTX_use_certificate_file(ctx, conf->Certificate.c_str(), SSL_FILETYPE_PEM) != 1)
    {
        fprintf(stderr, "<%s:%d> SSL_CTX_use_certificate_file failed: %s\n", __func__, __LINE__, conf->Certificate.c_str());
        return -1;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, conf->CertificateKey.c_str(), SSL_FILETYPE_PEM) != 1)
    {
        fprintf(stderr, "<%s:%d> SSL_CTX_use_PrivateKey_file failed: %s\n", __func__, __LINE__, conf->CertificateKey.c_str());
        return -1;
    }

    SSL_CTX_set_alpn_select_cb(ctx, alpn_select_proto_cb, NULL);
    return 0;
}
//======================================================================
SSL_CTX *Init_SSL(void)
{
    SSL_CTX *ctx;
    init_openssl();
    ctx = create_context();
    if (!ctx)
    {
        fprintf(stderr, "Unable to create SSL context\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (configure_context(ctx))
    {
        fprintf(stderr, "Error configure_context()\n");
        exit(EXIT_FAILURE);
    }
    else
        return ctx;
}
//======================================================================
const char *ssl_strerror(int err)
{
    switch (err)
    {
        case SSL_ERROR_NONE: // 0
            return "SSL_ERROR_NONE";
        case SSL_ERROR_SSL:  // 1
            return "SSL_ERROR_SSL";
        case SSL_ERROR_WANT_READ:  // 2
            return "SSL_ERROR_WANT_READ";
        case SSL_ERROR_WANT_WRITE:  // 3
            return "SSL_ERROR_WANT_WRITE";
        case SSL_ERROR_WANT_X509_LOOKUP:  // 4
            return "SSL_ERROR_WANT_X509_LOOKUP";
        case SSL_ERROR_SYSCALL:  // 5
            //print_err("SSL_ERROR_SYSCALL(%s)\n", strerror(errno));
            return "SSL_ERROR_SYSCALL";
        case SSL_ERROR_ZERO_RETURN:  // 6
            return "SSL_ERROR_ZERO_RETURN";
        case SSL_ERROR_WANT_CONNECT:  // 7
            return "SSL_ERROR_WANT_CONNECT";
        case SSL_ERROR_WANT_ACCEPT:  // 8
            return "SSL_ERROR_WANT_ACCEPT";
    }

    return "?";
}
//======================================================================
int ssl_read(Connect *con, char *buf, int len)
{
    ERR_clear_error();
    int ret = SSL_read(con->tls.ssl, buf, len);
    if (ret <= 0)
    {
        con->tls.err = SSL_get_error(con->tls.ssl, ret);
        if (con->tls.err == SSL_ERROR_ZERO_RETURN)
        {
            if (conf->PrintDebugMsg)
                print_err(con, "<%s:%d> Error SSL_read(): SSL_ERROR_ZERO_RETURN\n", __func__, __LINE__);
            return 0;
        }
        else if (con->tls.err == SSL_ERROR_WANT_READ)
        {
            con->tls.err = 0;
            return ERR_TRY_AGAIN;
        }
        else if (con->tls.err == SSL_ERROR_WANT_WRITE)
        {
            print_err(con, "<%s:%d> ??? Error SSL_read(): SSL_ERROR_WANT_WRITE\n", __func__, __LINE__);
            con->tls.err = 0;
            return ERR_TRY_AGAIN;
        }
        else
        {
            print_err(con, "<%s:%d> Error SSL_read(, , %d)=%d: %s\n", __func__, __LINE__, len, ret, ssl_strerror(con->tls.err));
            return -1;
        }
    }
    else
        return ret;
}
//======================================================================
int ssl_peek(Connect *con, char *buf, int len)
{
    ERR_clear_error();
    int ret = SSL_peek(con->tls.ssl, buf, len);
    if (ret <= 0)
    {
        con->tls.err = SSL_get_error(con->tls.ssl, ret);
        if (con->tls.err == SSL_ERROR_ZERO_RETURN)
        {
            if (conf->PrintDebugMsg)
                print_err(con, "<%s:%d> Error SSL_peek(): SSL_ERROR_ZERO_RETURN\n", __func__, __LINE__);
            return 0;
        }
        else if (con->tls.err == SSL_ERROR_WANT_READ)
        {
            con->tls.err = 0;
            return ERR_TRY_AGAIN;
        }
        else if (con->tls.err == SSL_ERROR_WANT_WRITE)
        {
            print_err(con, "<%s:%d> ??? Error SSL_peek(): SSL_ERROR_WANT_WRITE\n", __func__, __LINE__);
            con->tls.err = 0;
            return ERR_TRY_AGAIN;
        }
        else
        {
            print_err(con, "<%s:%d> Error SSL_peek(, , %d)=%d: %s\n", __func__, __LINE__, len, ret, ssl_strerror(con->tls.err));
            return -1;
        }
    }
    else
        return ret;
}
//======================================================================
int ssl_write(Connect *con, const char *buf, int len, int id)
{
    ERR_clear_error();
    if ((con == NULL) || (buf == NULL) || (len <= 0))
    {
        print_err("<%s:%d> ??? Error conn=%p, buf=%p, len=%d\n", __func__, __LINE__, con, buf, len);
        return -1;
    }

    int ret = SSL_write(con->tls.ssl, buf, len);
    if (ret <= 0)
    {
        con->tls.err = SSL_get_error(con->tls.ssl, ret);
        if (con->tls.err == SSL_ERROR_WANT_WRITE)
        {
            con->tls.err = 0;
            return ERR_TRY_AGAIN;
        }
        else if (con->tls.err == SSL_ERROR_WANT_READ)
        {
            print_err(con, "<%s:%d> ??? Error SSL_write(): SSL_ERROR_WANT_READ\n", __func__, __LINE__);
            con->tls.err = 0;
            return ERR_TRY_AGAIN;
        }
        print_err(con, "<%s:%d> Error SSL_write(, , %d)=%d: %s, errno=%d, id=%d\n", __func__, __LINE__,
                                    len, ret, ssl_strerror(con->tls.err), errno, id);
        return -1;
    }

    if (ret != len)
    {
        print_err(con, "<%s:%d> Error size(%d) != (SSL_write()=%d), id=%d\n", __func__, __LINE__, len, ret, id);
        return -1;
    }

    return ret;
}
//======================================================================
int ssl_accept(Connect *c)
{
    ERR_clear_error();
    int ret = SSL_accept(c->tls.ssl);
    if (ret < 1)
    {
        c->tls.err = SSL_get_error(c->tls.ssl, ret);
        if (conf->PrintDebugMsg)
            print_err(c, "<%s:%d> Error SSL_accept()=%d: %s\n", __func__, __LINE__, ret, ssl_strerror(c->tls.err));
        if (c->tls.err == SSL_ERROR_WANT_READ)
        {
            c->tls.poll_events = POLLIN;
        }
        else if (c->tls.err == SSL_ERROR_WANT_WRITE)
        {
            c->tls.poll_events = POLLOUT;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        const unsigned char *data = NULL;
        unsigned int datalen = 0;
        SSL_get0_alpn_selected(c->tls.ssl, &data, &datalen);
        if (data)
        {
            unsigned int proto_alpn_len = sizeof(proto_alpn);
            for ( unsigned int i = 0; i < proto_alpn_len; i += (unsigned int)(proto_alpn[i] + 1))
            {
                if (datalen != (unsigned int)proto_alpn[i])
                    continue;
                if (memcmp(data, &proto_alpn[i + 1], datalen) == 0)
                {
                    if (memcmp(data, "h2", datalen) == 0)
                    {
                        c->Protocol = P_HTTP2;
                    }
                    else if (memcmp(data, "http/1.1", datalen) == 0)
                    {
                        c->Protocol = P_HTTP1;
                    }
                    else
                    {
                        print_err(c, "<%s:%d> Protocol: ?\n", __func__, __LINE__);
                        return -1;
                    }
                    c->client_timer = 0;
                    return 1;
                }
            }

            //hex_print_stderr(__func__, __LINE__, data, datalen);
        }

        c->Protocol = P_HTTP1;
        return 1;
    }

    return 0;
}
