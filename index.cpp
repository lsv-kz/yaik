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
int create_index_html(Connect *r, vector<string>& list, int num_files, const char *dir_path, const char *uri, ByteArray *html)
{
    int n, i;
    struct stat st;
    int dir_path_len = strlen(dir_path);
    char *file_path = (char*)malloc(dir_path_len + NAME_MAX + 1);
    if (file_path == NULL)
    {
        print_err(r, "<%s:%d>  Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        return -RS500;
    }

    memcpy(file_path, dir_path, dir_path_len);
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
            "      margin-left:100px;\r\n"
            "      margin-right:50px;\r\n"
            "      background-color: rgb(122, 122, 82);\r\n"
            "      color: rgb(200,240,120);\r\n"
            "    }\r\n"
            "    a {\r\n"
            "      text-decoration: none;\r\n"
            "      color: rgb(255, 209, 26);\r\n"
            "    }\r\n"
            "    h3 {\r\n"
            "      color: rgb(200,240,120);\r\n"
            "    }\r\n"
            "  </style>\r\n"
            "  <link href=\"styles.css\" type=\"text/css\" rel=\"stylesheet\">\r\n"
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
    for (i = 0; i < num_files; i++)
    {
        char buf[1024];
        memcpy(file_path + dir_path_len, list[i].c_str(), list[i].size() + 1);
        n = lstat(file_path, &st);
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
    for (i = 0; i < num_files; i++)
    {
        char buf[1024];
        memcpy(file_path + dir_path_len, list[i].c_str(), list[i].size() + 1);
        n = lstat(file_path, &st);
        if ((n == -1) || !S_ISREG (st.st_mode))
            continue;
        else if (!strcmp(list[i].c_str(), "favicon.ico"))
            continue;

        if (!encode(list[i].c_str(), buf, sizeof(buf)))
        {
            print_err(r, "<%s:%d> Error: encode()\n", __func__, __LINE__);
            continue;
        }

        if (isimage(list[i].c_str()) && conf->ShowMediaFiles)
        {
            html->cat_str("   <tr><td><a href=\"");
            html->cat_str(buf);
            html->cat_str("\"><img src=\"");
            html->cat_str(buf);
            html->cat_str("\" width=\"100\"></a>");
            html->cat_str(list[i].c_str());
            html->cat_str("</td><td align=\"right\">");
            html->cat_int(st.st_size);
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
            html->cat_int(st.st_size);
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
            html->cat_int(st.st_size);
            html->cat_str(" bytes</td></tr>\r\n");
        }
        else
        {
            html->cat_str("   <tr><td><a href=\"");
            html->cat_str(buf);
            html->cat_str("\">");
            html->cat_str(list[i].c_str());
            html->cat_str("</a></td><td align=\"right\">");
            html->cat_int(st.st_size);
            html->cat_str(" bytes</td></tr>\r\n");
        }
    }
    //------------------------------------------------------------------
    html->cat_str("  </table>\r\n"
              "  <hr>\r\n  ");
    html->cat_time();
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
    free(file_path);
    return 0;
}
//======================================================================
int index_dir(Connect *r, const char *dir_path, const char *uri, ByteArray *html)
{
    DIR *dir;
    struct dirent *dirbuf;
    const int max_num_files = 1024;
    int num_files = 0;

    if (dir_path == NULL)
    {
        print_err(r, "<%s:%d>  Error dir_path = NULL\n", __func__, __LINE__);
        return -RS500;
    }

    vector<string> list;
    list.reserve(max_num_files);

    dir = opendir(dir_path);
    if (dir == NULL)
    {
        if (errno == EACCES)
            return -RS403;
        else
        {
            print_err(r, "<%s:%d>  Error opendir(\"%s\"): %s\n", __func__, __LINE__, dir_path, strerror(errno));
            return -RS500;
        }
    }

    while ((dirbuf = readdir(dir)))
    {
        if (dirbuf->d_name[0] == '.')
            continue;
        list.push_back(dirbuf->d_name);
        ++num_files;
    }

    closedir(dir);
    sort(list.begin(), list.end(), cmp);
    html->reserve(num_files * 100);
    return create_index_html(r, list, num_files, dir_path, uri, html);
}
