#include "main.h"
#include <algorithm>

using namespace std;
//======================================================================
int isimage(const char *name)
{
    const char *p;

    if (!(p = strrchr(name, '.')))
        return 0;

    if (!strlcmp_case(p, ".gif", 4))
        return 1;
    else if (!strlcmp_case(p, ".png", 4))
        return 1;
    else if (!strlcmp_case(p, ".svg", 4))
        return 1;
    else if (!strlcmp_case(p, ".jpeg", 5) || !strlcmp_case(p, ".jpg", 4))
        return 1;
    return 0;
}
//======================================================================
int isaudio(const char *name)
{
    const char *p;

    if (!(p = strrchr(name, '.')))
        return 0;

    if (!strlcmp_case(p, ".wav", 4))
        return 1;
    else if (!strlcmp_case(p, ".mp3", 4))
        return 1;
    else if (!strlcmp_case(p, ".ogg", 4))
        return 1;
    return 0;
}
//======================================================================
int isvideo(const char *name)
{
    const char *p;

    if (!(p = strrchr(name, '.')))
        return 0;
    if (!strlcmp_case(p, ".mp4", 4))
        return 1;
    else if (!strlcmp_case(p, ".webm", 4))
        return 1;
    else if (!strlcmp_case(p, ".ogv", 4))
        return 1;
    return 0;
}
//======================================================================
bool cmp(const string &a, const string &b)
{
    unsigned int n1, n2;
    bool i;

    if (((n1 = atoi(a.c_str())) > 0) && ((n2 = atoi(b.c_str())) > 0))
    {
        if (n1 < n2)
            i = 1;
        else if (n1 == n2)
            i = a < b;
        else
            i = 0;
    }
    else
        i = a < b;

    return i;
}
//======================================================================
void create_index_html(Connect *r, vector<string>& list, int numFiles, string& path, const char *uri, ByteArray *html)
{
    const int len_path = path.size();
    int n, i;
    long long size;
    struct stat st;

    //------------------------------------------------------------------
    html->cpy_str("<!DOCTYPE HTML>\r\n"
            "<html>\r\n"
            " <head>\r\n"
            "  <meta charset=\"UTF-8\">\r\n"
            "  <title>Index of ");
    html->cat_str(uri);
    html->cat_str("</title>\r\n"
            "  <style>\r\n"
            "    body {\r\n"
            "     margin-left:100px; margin-right:50px;\r\n"
            "    }\r\n"
            "  </style>\r\n"
            "  <link href=\"/styles.css\" type=\"text/css\" rel=\"stylesheet\">\r\n"
            " </head>\r\n"
            " <body id=\"top\">\r\n"
            "  <h3>Index of ");
    html->cat_str(uri);
    html->cat_str("</h3>\r\n"
            "  <table border=\"0\" width=\"100\%\">\r\n"
            "   <tr><td><h3>Directories</h3></td></tr>\r\n");
    //------------------------------------------------------------------
     if (!strcmp(uri, "/"))
        html->cat_str("   <tr><td></td></tr>\r\n");
    else
        html->cat_str("   <tr><td><a href=\"../\">Parent Directory/</a></td></tr>\r\n");
    //-------------------------- Directories ---------------------------
    for (i = 0; (i < numFiles); i++)
    {
        char buf[1024];
        path += list[i];
        n = lstat(path.c_str(), &st);
        path.resize(len_path);
        if ((n == -1) || !S_ISDIR (st.st_mode))
            continue;

        if (!encode(list[i].c_str(), buf, sizeof(buf)))
        {
            print_err(r, "<%s:%d> Error: encode()\n", __func__, __LINE__);
            continue;
        }

        html->cat_str("   <tr><td><a href=\"");
        html->cat_str(buf);
        html->cat_str("/\">");
        html->cat_str(list[i].c_str());
        html->cat_str("/</a></td></tr>\r\n");
    }
    //------------------------------------------------------------------
    html->cat_str("  </table>\r\n   <hr>\r\n  <table border=\"0\" width=\"100\%\">\r\n"
                "   <tr><td><h3>Files</h3></td><td></td></tr>\r\n");
    //---------------------------- Files -------------------------------
    for (i = 0; i < numFiles; i++)
    {
        char buf[1024];
        path += list[i];
        n = lstat(path.c_str(), &st);
        path.resize(len_path);
        if ((n == -1) || !S_ISREG (st.st_mode))
            continue;
        else if (!strcmp(list[i].c_str(), "favicon.ico"))
            continue;

        if (!encode(list[i].c_str(), buf, sizeof(buf)))
        {
            print_err(r, "<%s:%d> Error: encode()\n", __func__, __LINE__);
            continue;
        }

        size = (long long)st.st_size;
        char size_s[32];
        snprintf(size_s, sizeof(size_s), "%lld", size);

        if (isimage(list[i].c_str()) && conf->ShowMediaFiles)
        {
            html->cat_str("   <tr><td><a href=\"");
            html->cat_str(buf);
            html->cat_str("\"><img src=\"");
            html->cat_str(buf);
            html->cat_str("\" width=\"100\"></a>");
            html->cat_str(list[i].c_str());
            html->cat_str("</td><td align=\"right\">");
            html->cat_str(size_s);
            html->cat_str(" bytes</td></tr>\r\n");
        }
        else if (isaudio(list[i].c_str()) && conf->ShowMediaFiles)
        {
            html->cat_str("   <tr><td><audio preload=\"none\" controls src=\"");
            html->cat_str(buf);
            html->cat_str("\"></audio><a href=\"");
            html->cat_str(buf);
            html->cat_str("\">");
            html->cat_str(list[i].c_str());
            html->cat_str("</a></td><td align=\"right\">");
            html->cat_str(size_s);
            html->cat_str(" bytes</td></tr>\r\n");
        }
        else if (isvideo(list[i].c_str()) && conf->ShowMediaFiles)
        {
            html->cat_str("   <tr><td><video width=\"320\" preload=\"none\" controls src=\"");
            //html->cat_str("   <tr><td><video preload=\"none\" controls src=\"");
            html->cat_str(buf);
            html->cat_str("\"></video><a href=\"");
            html->cat_str(buf);
            html->cat_str("\">");
            html->cat_str(list[i].c_str());
            html->cat_str("</a></td><td align=\"right\">");
            html->cat_str(size_s);
            html->cat_str(" bytes</td></tr>\r\n");
        }
        else
        {
            html->cat_str("   <tr><td><a href=\"");
            html->cat_str(buf);
            html->cat_str("\">");
            html->cat_str(list[i].c_str());
            html->cat_str("</a></td><td align=\"right\">");
            html->cat_str(size_s);
            html->cat_str(" bytes</td></tr>\r\n");
        }
    }
    //------------------------------------------------------------------
    html->cat_str("  </table>\r\n"
              "  <hr>\r\n  ");
    html->cat_str(get_time().c_str());
    html->cat_str("\r\n"
              "  <a href=\"#top\" style=\"display:block;\r\n"
              "         position:fixed;\r\n"
              "         bottom:30px;\r\n"
              "         left:10px;\r\n"
              "         width:50px;\r\n"
              "         height:40px;\r\n"
              "         font-size:60px;\r\n"
              "         background:gray;\r\n"
              "         border-radius:10px;\r\n"
              "         color:black;\r\n"
              "         opacity: 0.7\">^</a>\r\n"
              " </body>\r\n"
              "</html>");
}
//======================================================================
int index_dir(Connect *r, string& path, const char *uri, ByteArray *html)
{
    DIR *dir;
    struct dirent *dirbuf;
    const int maxNumFiles = 1024;
    int numFiles = 0;

    vector<string> list;
    list.reserve(maxNumFiles);

    path += '/';

    dir = opendir(path.c_str());
    if (dir == NULL)
    {
        if (errno == EACCES)
            return -RS403;
        else
        {
            print_err(r, "<%s:%d>  Error opendir(\"%s\"): %s\n", __func__, __LINE__, path.c_str(), strerror(errno));
            return -RS500;
        }
    }

    while ((dirbuf = readdir(dir)))
    {
        if (numFiles >= maxNumFiles )
        {
            print_err(r, "<%s:%d> number of files per directory >= %d\n", __func__, __LINE__, numFiles);
            break;
        }

        if (dirbuf->d_name[0] == '.')
            continue;
        list.push_back(dirbuf->d_name);
        ++numFiles;
    }

    closedir(dir);
    sort(list.begin(), list.end(), cmp);
    create_index_html(r, list, numFiles, path, uri, html);

    return 0;
}
