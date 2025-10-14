#include "main.h"

using namespace std;

static Config c;
const Config* const conf = &c;
//======================================================================
int check_path(string& path)
{
    struct stat st;

    int ret = stat(path.c_str(), &st);
    if (ret == -1)
    {
        fprintf(stderr, "<%s:%d> Error stat(%s): %s\n", __func__, __LINE__, path.c_str(), strerror(errno));
        char buf[2048];
        char *cwd = getcwd(buf, sizeof(buf));
        if (cwd)
            fprintf(stderr, "<%s:%d> cwd: %s\n", __func__, __LINE__, cwd);
        return -1;
    }

    if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "<%s:%d> [%s] is not directory\n", __func__, __LINE__, path.c_str());
        return -1;
    }

    char path_[PATH_MAX] = "";
    if (!realpath(path.c_str(), path_))
    {
        fprintf(stderr, "<%s:%d> Error realpath(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    path = path_;

    return 0;
}
//======================================================================
void create_conf_file(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        fprintf(stderr, "<%s> Error create conf file (%s): %s\n", __func__, path, strerror(errno));
        exit(1);
    }

    fprintf(f, "PrintDebugMsg        off       # on, off\n\n");

    fprintf(f, "SecureConnect        on\n");
    fprintf(f, "SelectHTTP2          on\n");

    fprintf(f, "ServerSoftware       ?\n");
    fprintf(f, "ServerAddr           0.0.0.0\n");
    fprintf(f, "ServerPort           ?\n\n");

    fprintf(f, "Certificate          ?\n");
    fprintf(f, "CertificateKey       ?\n");
    fprintf(f, "DocumentRoot         ?\n");
    fprintf(f, "ScriptPath           ?\n");
    fprintf(f, "LogPath              ?\n");
    fprintf(f, "PidFilePath          ?\n\n");

    fprintf(f, "####### UsePHP: off, php-fpm, php-cgi #######\n");
    fprintf(f, "UsePHP     php-fpm\n");
    fprintf(f, "PathPHP    127.0.0.1:9000  # [php-fpm: 127.0.0.1:9000 (/var/run/php-fpm.sock)]\n\n");

    fprintf(f, "ListenBacklog        4096\n");
    fprintf(f, "TcpNoDelay           on\n\n");

    fprintf(f, "HeaderTableSize       0\n");
    fprintf(f, "MaxConcurrentStreams  128\n\n");

    fprintf(f, "MaxAcceptConnections  5000\n\n");
    
    fprintf(f, "MaxRequestsPerClient  1000\n\n");

    fprintf(f, "HTTP1_DataBufSize          16384\n");
    fprintf(f, "HTTP2_DataBufSize          16384\n\n");

    fprintf(f, "Timeout              35  # seconds\n");
    fprintf(f, "TimeoutKeepAlive     180 # seconds\n");
    fprintf(f, "TimeoutPoll          10  # milliseconds\n\n");
    fprintf(f, "TimeoutCGI           15  # seconds\n");
    fprintf(f, "MaxCgiProc           15\n\n");

    fprintf(f, "ClientMaxBodySize    10000000\n\n");

    fprintf(f, "ShowMediaFiles       off\n\n");

    fprintf(f, "User                 root\n");
    fprintf(f, "Group                www-data\n");

    fclose(f);
}
//======================================================================
static int line_ = 1, line_inc = 0;
static char bracket = 0;
//----------------------------------------------------------------------
int getLine(FILE *f, String &ss)
{
    ss.clear();
    if (bracket)
    {
        ss << bracket;
        bracket = 0;
        return 1;
    }

    ss = "";
    int ch, len = 0, numWords = 0, wr = 1, wrSpace = 0;

    if (line_inc)
    {
        ++line_;
        line_inc = 0;
    }

    while (((ch = getc(f)) != EOF))
    {
        if (ch == '\n')
        {
            if (len)
            {
                line_inc = 1;
                return ++numWords;
            }
            else
            {
                ++line_;
                wr = 1;
                ss = "";
                wrSpace = 0;
                continue;
            }
        }
        else if (wr == 0)
            continue;
        else if ((ch == ' ') || (ch == '\t'))
        {
            if (len)
                wrSpace = 1;
        }
        else if (ch == '#')
            wr = 0;
        else if ((ch == '{') || (ch == '}'))
        {
            if (len)
                bracket = (char)ch;
            else
            {
                ss << (char)ch;
                ++len;
            }
            return ++numWords;
        }
        else if (ch != '\r')
        {
            if (wrSpace)
            {
                ss << " ";
                ++len;
                ++numWords;
                wrSpace = 0;
            }

            ss << (char)ch;
            ++len;
        }
    }

    if (len)
        return ++numWords;
    return -1;
}
//======================================================================
int is_number(const char *s)
{
    if (!s)
        return 0;
    int n = isdigit((int)*(s++));
    while (*s && n)
        n = isdigit((int)*(s++));
    return n;
}
//======================================================================
int find_bracket(FILE *f, char c)
{
    if (bracket)
    {
        if (c != bracket)
        {
            bracket = 0;
            return 0;
        }

        bracket = 0;
        return 1;
    }

    int ch, grid = 0;
    if (line_inc)
    {
        ++line_;
        line_inc = 0;
    }

    while (((ch = getc(f)) != EOF))
    {
        if (ch == '#')
        {
            grid = 1;
        }
        else if (ch == '\n')
        {
            grid = 0;
            ++line_;
        }
        else if ((ch == '{') && (grid == 0))
            return 1;
        else if ((ch != ' ') && (ch != '\t') && (grid == 0))
            return 0;
    }

    return 0;
}
//======================================================================
void create_fcgi_list(fcgi_list_addr **l, const string &s1, const string &s2, CGI_TYPE type)
{
    if (l == NULL)
    {
        fprintf(stderr, "<%s:%d> Error pointer = NULL\n", __func__, __LINE__);
        exit(errno);
    }

    fcgi_list_addr *t;
    try
    {
        t = new fcgi_list_addr;
    }
    catch (...)
    {
        fprintf(stderr, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        exit(errno);
    }

    t->script_name = s1;
    t->addr = s2;
    t->type = type;
    t->next = *l;
    *l = t;
}
//======================================================================
int read_conf_file(FILE *fconf)
{
    String ss;
    c.SecureConnect = true;
    c.ctx = NULL;

    int n;
    while ((n = getLine(fconf, ss)) > 0)
    {
        if (n == 2)
        {
            String s1, s2;
            ss >> s1;
            ss >> s2;

            if (s1 == "PrintDebugMsg")
            {
                if (!strcmp_case(s2.c_str(), "on"))
                    c.PrintDebugMsg = true;
                else if (!strcmp_case(s2.c_str(), "off"))
                    c.PrintDebugMsg = false;
                else
                {
                    fprintf(stderr, "<%s:%d> Error config file line <%d> \"%s\": [on | off]\n", 
                            __func__, __LINE__, line_, ss.c_str());
                    return -1;
                }
            }
            else if (s1 == "SecureConnect")
            {
                if (!strcmp_case(s2.c_str(), "on"))
                    c.SecureConnect = true;
                else if (!strcmp_case(s2.c_str(), "off"))
                    c.SecureConnect = false;
                else
                {
                    fprintf(stderr, "<%s:%d> Error config file line <%d> \"%s\": [on | off]\n", 
                            __func__, __LINE__, line_, ss.c_str());
                    return -1;
                }
            }
            else if (s1 == "SelectHTTP2")
            {
                if (!strcmp_case(s2.c_str(), "on"))
                    c.SelectHTTP2 = true;
                else if (!strcmp_case(s2.c_str(), "off"))
                    c.SelectHTTP2 = false;
                else
                {
                    fprintf(stderr, "<%s:%d> Error config file line <%d> \"%s\": [on | off]\n", 
                            __func__, __LINE__, line_, ss.c_str());
                    return -1;
                }
            }
            else if (s1 == "ServerAddr")
                s2 >> c.ServerAddr;
            else if (s1 == "ServerPort")
                s2 >> c.ServerPort;
            else if (s1 == "ServerSoftware")
                s2 >> c.ServerSoftware;
            else if ((s1 == "HeaderTableSize") && is_number(s2.c_str()))
                s2 >> c.HeaderTableSize;
            else if ((s1 == "MaxConcurrentStreams") && is_number(s2.c_str()))
                s2 >> c.MaxConcurrentStreams;
            else if (s1 == "TcpNoDelay")
            {
                if (!strcmp_case(s2.c_str(), "on"))
                    c.TcpNoDelay = true;
                else if (!strcmp_case(s2.c_str(), "off"))
                    c.TcpNoDelay = false;
                else
                {
                    fprintf(stderr, "<%s:%d> Error config file line <%d> \"%s\": [on | off]\n", 
                            __func__, __LINE__, line_, ss.c_str());
                    return -1;
                }
            }
            else if ((s1 == "ListenBacklog") && is_number(s2.c_str()))
                s2 >> c.ListenBacklog;
            else if ((s1 == "HTTP1_DataBufSize") && is_number(s2.c_str()))
            {
                s2 >> c.HTTP1_DataBufSize;
            }
            else if ((s1 == "HTTP2_DataBufSize") && is_number(s2.c_str()))
            {
                s2 >> c.HTTP2_DataBufSize;
                if ((c.HTTP2_DataBufSize <= 0) || (c.HTTP2_DataBufSize > 16384))
                {
                    fprintf(stderr, "<%s:%d> Error read config file: HTTP2_DataBufSize > 16384, [%s], line <%d>\n", __func__, __LINE__, ss.c_str(), line_);
                    return -1;
                }

                if (c.HTTP2_DataBufSize > 16375)
                    c.HTTP2_DataBufSize = 16375;
            }
            else if ((s1 == "MaxAcceptConnections") && is_number(s2.c_str()))
                s2 >> c.MaxAcceptConnections;
            else if ((s1 == "MaxRequestsPerClient") && is_number(s2.c_str()))
                s2 >> c.MaxRequestsPerClient;
            else if ((s1 == "TimeoutPoll") && is_number(s2.c_str()))
                s2 >> c.TimeoutPoll;
            else if (s1 == "Certificate")
                s2 >> c.Certificate;
            else if (s1 == "CertificateKey")
                s2 >> c.CertificateKey;
            else if (s1 == "DocumentRoot")
                s2 >> c.DocumentRoot;
            else if (s1 == "ScriptPath")
                s2 >> c.ScriptPath;
            else if (s1 == "LogPath")
                s2 >> c.LogPath;
            else if (s1 == "PidFilePath")
                s2 >> c.PidFilePath;
            else if ((s1 == "MaxCgiProc") && is_number(s2.c_str()))
                s2 >> c.MaxCgiProc;
            else if ((s1 == "Timeout") && is_number(s2.c_str()))
                s2 >> c.Timeout;
            else if ((s1 == "TimeoutKeepAlive") && is_number(s2.c_str()))
                s2 >> c.TimeoutKeepAlive;
            else if ((s1 == "TimeoutCGI") && is_number(s2.c_str()))
                s2 >> c.TimeoutCGI;
            else if (s1 == "UsePHP")
            {
                if ((s2 == "off") || (s2 == "php-fpm") || (s2 == "php-cgi"))
                    s2 >> c.UsePHP;
                else
                {
                    fprintf(stderr, "<%s:%d> Error read config file: [%s], line <%d>\n", __func__, __LINE__, ss.c_str(), line_);
                    return -1;
                }
            }
            else if (s1 == "PathPHP")
                s2 >> c.PathPHP;
            else if (s1 == "ShowMediaFiles")
            {
                if (!strcmp_case(s2.c_str(), "on"))
                    c.ShowMediaFiles = true;
                else if (!strcmp_case(s2.c_str(), "off"))
                    c.ShowMediaFiles = false;
                else
                {
                    fprintf(stderr, "<%s:%d> Error config file line <%d> \"%s\": [on | off]\n", 
                            __func__, __LINE__, line_, ss.c_str());
                    return -1;
                }
            }
            else if ((s1 == "ClientMaxBodySize") && is_number(s2.c_str()))
                s2 >> c.ClientMaxBodySize;
            else if (s1 == "User")
                s2 >> c.user;
            else if (s1 == "Group")
                s2 >> c.group;
            else
            {
                fprintf(stderr, "<%s:%d> Error read config file: [%s], line <%d>\n", __func__, __LINE__, ss.c_str(), line_);
                return -1;
            }
        }
        else if (n == 1)
        {
            if (ss == "ServerSoftware")
                c.ServerSoftware = "";
            else if (ss == "fastcgi")
            {
                if (find_bracket(fconf, '{') == 0)
                {
                    fprintf(stderr, "<%s:%d> Error not found \"{\", line <%d>\n", __func__, __LINE__, line_);
                    return -1;
                }

                while (getLine(fconf, ss) == 2)
                {
                    string s1, s2;
                    ss >> s1;
                    ss >> s2;

                    create_fcgi_list(&c.fcgi_list, s1, s2, FASTCGI);
                }

                if (ss != "}")
                {
                    fprintf(stderr, "<%s:%d> Error not found \"}\", line <%d>\n", __func__, __LINE__, line_);
                    return -1;
                }
            }
            else if (ss == "scgi")
            {
                if (find_bracket(fconf, '{') == 0)
                {
                    fprintf(stderr, "<%s:%d> Error not found \"{\", line <%d>\n", __func__, __LINE__, line_);
                    return -1;
                }

                while (getLine(fconf, ss) == 2)
                {
                    string s1, s2;
                    ss >> s1;
                    ss >> s2;

                    create_fcgi_list(&c.fcgi_list, s1, s2, SCGI);
                }

                if (ss != "}")
                {
                    fprintf(stderr, "<%s:%d> Error not found \"}\", line <%d>\n", __func__, __LINE__, line_);
                    return -1;
                }
            }
            else
            {
                fprintf(stderr, "<%s:%d> Error read config file: [%s] line <%d>\n", __func__, __LINE__, ss.c_str(), line_);
                return -1;
            }
        }
        else
        {
            fprintf(stderr, "<%s:%d> Error read config file: [%s], line <%d>\n", __func__, __LINE__, ss.c_str(), line_);
            return -1;
        }
    }

    if (!feof(fconf))
    {
        fprintf(stderr, "<%s:%d> Error read config file\n", __func__, __LINE__);
        return -1;
    }

    fclose(fconf);
    //------------------------------------------------------------------
    if (check_path(c.LogPath) == -1)
    {
        fprintf(stderr, "!!! Error LogPath [%s]\n", conf->LogPath.c_str());
        return -1;
    }
    //------------------------------------------------------------------
    if (check_path(c.DocumentRoot) == -1)
    {
        fprintf(stderr, "!!! Error DocumentRoot [%s]\n", conf->DocumentRoot.c_str());
        return -1;
    }
    //------------------------------------------------------------------
    if (check_path(c.ScriptPath) == -1)
    {
        c.ScriptPath = "";
        fprintf(stderr, "!!! Error ScriptPath [%s]\n", conf->ScriptPath.c_str());
    }
    //------------------------------------------------------------------
    if (check_path(c.PidFilePath) == -1)
    {
        fprintf(stderr, "!!! Error PidFilePath [%s]\n", conf->PidFilePath.c_str());
        return -1;
    }
    //------------------------------------------------------------------
    if (!conf->SecureConnect && conf->SelectHTTP2)
    {
        fprintf(stderr, "!!! conf->SecureConnect=off and SelectHTTP2=on is incompatible\n");
        return -1;
    }
    //------------------------------------------------------------------
    if (conf->MaxCgiProc == 0)
    {
        fprintf(stderr, "<%s:%d> Error: MaxCgiProc=0\n", __func__, __LINE__);
        return -1;
    }
    //------------------------------------------------------------------
    if (conf->MaxAcceptConnections <= 0)
    {
        fprintf(stderr, "<%s:%d> Error config file: MaxAcceptConnections=%d\n", __func__, __LINE__, conf->MaxAcceptConnections);
        return -1;
    }

    const int fd_std = 3, fd_logs = 2, fd_sock = 1;
    long min_open_fd = fd_std + fd_logs + fd_sock;
    int max_fd = min_open_fd + conf->MaxAcceptConnections + conf->MaxAcceptConnections * conf->MaxConcurrentStreams + conf->MaxCgiProc * 2;
    n = set_max_fd(max_fd);
    if (n == -1)
        return -1;
    else if (n < max_fd)
    {
        fprintf(stderr, "<%s:%d> Error config file: max_open_fd=%d, max_fd=%d\n", __func__, __LINE__, n, max_fd);
        return -1;
    }
    //------------------------------------------------------------------
    if (conf->SecureConnect)
    {
        if (!(c.ctx = Init_SSL()))
        {
            fprintf(stderr, "<%s:%d> Error Init_SSL()\n", __func__, __LINE__);
            return -1;
        }
    }

    return 0;
}
//======================================================================
int read_conf_file(const char *path_conf)
{
    FILE *fconf = fopen(path_conf, "r");
    if (!fconf)
    {
        if (errno == ENOENT)
        {
            fprintf(stderr, " Error: config file %s not found\n", path_conf);
            char s[8];
            printf(" Create config file? [y/n]: ");
            fflush(stdout);
            fgets(s, sizeof(s), stdin);
            if ((s[0] == 'y') || (s[0] == 'Y'))
            {
                create_conf_file(path_conf);
                fprintf(stderr, " Correct the configuration file %s\n", path_conf);
            }
        }
        else
            fprintf(stderr, "<%s:%d> Error fopen(%s): %s\n", __func__, __LINE__, path_conf, strerror(errno));
        return -1;
    }

    return read_conf_file(fconf);
}
//======================================================================
int set_uid()
{
    uid_t uid = getuid();
    if (uid == 0)
    {
        char *p;
        c.server_uid = strtol(c.user.c_str(), &p, 0);
        if (*p == '\0')
        {
            struct passwd *passwdbuf = getpwuid(c.server_uid);
            if (!passwdbuf)
            {
                fprintf(stderr, "<%s:%d> Error getpwuid(%u): %s\n", __func__, __LINE__, c.server_uid, strerror(errno));
                return -1;
            }
        }
        else
        {
            struct passwd *passwdbuf = getpwnam(c.user.c_str());
            if (!passwdbuf)
            {
                fprintf(stderr, "<%s:%d> Error getpwnam(%s): %s\n", __func__, __LINE__, c.user.c_str(), strerror(errno));
                return -1;
            }
            c.server_uid = passwdbuf->pw_uid;
        }

        c.server_gid = strtol(c.group.c_str(), &p, 0);
        if (*p == '\0')
        {
            struct group *groupbuf = getgrgid(c.server_gid);
            if (!groupbuf)
            {
                fprintf(stderr, "<%s:%d> Error getgrgid(%u): %s\n", __func__, __LINE__, c.server_gid, strerror(errno));
                return -1;
            }
        }
        else
        {
            struct group *groupbuf = getgrnam(c.group.c_str());
            if (!groupbuf)
            {
                fprintf(stderr, "<%s:%d> Error getgrnam(%s): %s\n", __func__, __LINE__, c.group.c_str(), strerror(errno));
                return -1;
            }
            c.server_gid = groupbuf->gr_gid;
        }
        //--------------------------------------------------------------
        if (c.server_uid != uid)
        {
            if (setuid(c.server_uid) == -1)
            {
                fprintf(stderr, "<%s:%d> Error setuid(%u): %s\n", __func__, __LINE__, c.server_uid, strerror(errno));
                return -1;
            }
        }
    }
    else
    {
        c.server_uid = getuid();
        c.server_gid = getgid();
    }

    return 0;
}
//======================================================================
int set_max_fd(int max_open_fd)
{
    struct rlimit lim;
    if (getrlimit(RLIMIT_NOFILE, &lim) == -1)
    {
        fprintf(stderr, "<%s:%d> Error getrlimit(RLIMIT_NOFILE): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }
    else
    {
        if (max_open_fd > (long)lim.rlim_cur)
        {
            if (max_open_fd > (long)lim.rlim_max)
                lim.rlim_cur = lim.rlim_max;
            else
                lim.rlim_cur = max_open_fd;

            if (setrlimit(RLIMIT_NOFILE, &lim) == -1)
                fprintf(stderr, "<%s:%d> Error setrlimit(RLIMIT_NOFILE): %s\n", __func__, __LINE__, strerror(errno));
            max_open_fd = sysconf(_SC_OPEN_MAX);
            if (max_open_fd < 0)
            {
                fprintf(stderr, "<%s:%d> Error sysconf(_SC_OPEN_MAX): %s\n", __func__, __LINE__, strerror(errno));
                return -1;
            }
            else
                fprintf(stdout, "<%s:%d> sysconf(_SC_OPEN_MAX): %d\n", __func__, __LINE__, max_open_fd);
        }
    }

    return max_open_fd;
}
//======================================================================
void free_fcgi_list()
{
    c.free_fcgi_list();
}
//======================================================================
void setDataBufSize(int n)
{
    c.HTTP2_DataBufSize = n;
}
