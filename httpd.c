/*
 * httpd 简易服务器
 */
// 网络字节序和主机字节序转换，IPv4,IPv6地址的转换
#include <arpa/inet.h>
#include <ctype.h>
// 网络编程头文件,定义相关数据结构，常量，函数
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
// 处理文件状态信息的函数和数据结构
#include <sys/stat.h>
#include <sys/types.h>
// 进程处理
#include <sys/wait.h>
// 系统调用声明
#include <unistd.h>

// 是否是空格，头文件中函数冲突，故设此宏定义
#define ISspace(x) isspace((int)(x))

// 服务器的标识
#define SERVER_STRING "Server: httpd/0.1.0\r\n"

/**
 * 向标准错误输出输出错误信息，终止程序
 * @param sc 错误信息
 */
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

/**
 * 启动端口监听，port为0则自动分配端口
 *
 * @param port 监听端口
 */
int startup(unsigned short *port)
{
    int httpd = 0;

    struct sockaddr_in name;
    /**
     * 创建套接字连接，
     * @param domain 指定使用的协议族，AF_INET(IPv4)，AF_INET6(IPv6)，AF_UNIX(本地通信)
     * @param type 指定套接字类型 SOCK_STREAM 流式套接字，如TCP；SOCK_DGRAM 数据报套接字，如UDP
     * @param protocol 指定具体的传输协议，TCP/UDP等，可设为0，表示使用默认协议
     */
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
    {
        error_die("socket");
    }

    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;                // 协议族设为IPv4
    name.sin_port = htons(*port);             // 端口号转为网络字节序表示的16位整数
    name.sin_addr.s_addr = htonl(INADDR_ANY); // sin_addr为IPv4地址，即为通信目标主机的IP地址

    /**
     * 将套接字绑定到特定的地址和端口
     * @param fd 要绑定的套接字的文件描述符
     * @param addr 指定要绑定的地址和端口信息
     * @param len addr结构体的长度
     */
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
    {
        error_die("bind");
    }

    // 在调用bind后端口仍为0，手动获取系统分配的端口
    if (*port == 0)
    {
        int namelen = sizeof(name);
        /**
         * getsockname用于获取与指定套接字关联的本地地址(IP和端口号)
         * @param fd 套接字的文件描述符
         * @param addr 存储获取到的本地地址信息的结构体
         * @param len addr结构体的长度
         */
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
        {
            error_die("getsockname");
        }
        *port = ntohs(name.sin_port); // ntohs网络字节序转为16位无符号整数
    }

    /**
     * 将已绑定的套接字转为监听套接字，使其可以接受连接请求
     * @param fd 监听套接字的文件描述符
     * @param n 允许在套接字队列中排队等待的最大连接请求数
     */
    if (listen(httpd, 5) < 0)
    {
        error_die("listen");
    }

    return httpd;
}

/**
 *  从套接字中读取一行,该行以"\r","\n"或"\r\n"结尾
 * @param sock 套接字的文件描述符
 * @param buf 暂存数据的缓冲区
 * @param size 缓冲区大小
 */
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        /**
         *  ssize_t recv(int fd, void *buf, size_t n, int flags)
         *  从fd中读取n个字节到缓冲区buf中,返回读取到的字节数，出错则返回-1
         *  flags为标志位，为0为默认情况，此时阻塞等待数据到达
         */
        n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK); // MSG_PEEK 表示不将数据从接收缓冲区中移除
                if ((n > 0) && (c == '\n'))
                {
                    recv(sock, &c, 1, 0);
                }
                else
                {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }
        else
        {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return (i);
}

/**
 * 未实现相应请求方法时返回的数据
 * @param client 与客户端通信的套接字文件描述符
 */
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.</P>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**
 *  未找到对应文件
 *
 * @param client 与客户端通信的套接字文件描述符
 */
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.</P>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**
 *  将文件的基本信息封装进HTTP的头部
 *
 * @param client 套接字文件描述符
 * @param filename 文件名
 */
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**
 * 错误请求处理
 *
 * @param client 客户端通信套接字文件描述符
 */
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**
 * 无法执行cgi脚本
 *
 * @param client 套接字文件描述符
 */
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**
 * 读取文件内容到响应的body中
 *
 * @param client 套接字文件描述符
 * @param resource 文件指针
 */
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**
 *  向客户端发送文件
 *
 * @param client 套接字文件描述符
 * @param filename 文件名
 */
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    // 读取并忽略该HTTP请求后的所有内容
    buf[0] = 'A';
    buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))
    {
        numchars = get_line(client, buf, sizeof(buf));
    }

    resource = fopen(filename, "r");
    if (resource == NULL)
    {
        not_found(client);
    }
    else
    {
        // 将文件的基本信息封装进response的头部
        headers(client, filename);
        // 将文件的内容读出作为response的body
        cat(client, resource);
    }

    fclose(resource);
}

/**
 *  执行CGI脚本，设置适当的环境变量
 *
 * @param client 客户端通信套接字
 * @param path 请求路径
 * @param method 请求方法
 * @param query_string 查询参数
 */
void execute_cgi(int client, const char *path, const char *method, const char *query_string)
{
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A';
    buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)
    {
        // 对于GET请求，将请求内容全部读出并忽略
        while ((numchars > 0) && (strcmp("\n", buf)))
        {
            numchars = get_line(client, buf, sizeof(buf));
        }
    }
    else
    {
        numchars = get_line(client, buf, sizeof(buf));
        // 该while循环用于获取content-length,header中的其它字段忽略
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length") == 0)
            {
                content_length = atoi(&buf[16]);
            }
            numchars = get_line(client, buf, sizeof(buf));
        }

        if (content_length == -1)
        {
            bad_request(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    // 创建两个管道，用于两个进程间通信，无名管道的读端和写端
    if (pipe(cgi_output) < 0)
    {
        cannot_execute(client);
        return;
    }

    if (pipe(cgi_input) < 0)
    {
        cannot_execute(client);
        return;
    }

    // 创建一个子进程执行CGI脚本
    if ((pid = fork()) < 0)
    {
        cannot_execute(client);
        return;
    }

    // 子进程执行脚本
    if (pid == 0)
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        // 子进程的标准输出重定向到管道cgi_output的写端
        dup2(cgi_output[1], 1);
        // 子进程的标准输入重定向到管道cgi_input的读端
        dup2(cgi_input[0], 0);
        // 关闭 cgi_ouput 管道的读端与cgi_input 管道的写端
        close(cgi_output[0]);
        close(cgi_input[1]);

        // 构造一个环境变量
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        // 将该环境变量添加到子进程的运行环境中
        putenv(meth_env);

        if (strcasecmp(method, "GET") == 0)
        {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else
        {
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }

        /**
         * 在子进程中执行path指定的程序
         * int execl(const char *path, const char *arg, ...)
         *
         * @param path 可执行文件路径
         * @param arg 可执行文件名
         * @param ... 传递给可执行文件的参数
         */
        execl(path, path, NULL);
        exit(0);
    }
    else
    {
        // 父进程中执行的代码
        close(cgi_output[1]);
        close(cgi_input[0]);

        // 对于POST方法，读取body里面的内容，并写入到cgi_input管道的写入端让子进程去读
        if (strcasecmp(method, "POST") == 0)
        {
            for (int i = 0; i < content_length; i++)
            {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        }

        // 从cgi_output管道读取子进程的输出，并发送到客户端
        while (read(cgi_output[0], &c, 1) > 0)
        {
            send(client, &c, 1, 0);
        }

        close(cgi_input[1]);
        close(cgi_output[0]);

        // 等待子进程退出
        waitpid(pid, &status, 0);
    }
}

/**
 * 处理接收到的请求;
 * HTTP请求格式 首行+请求头+空行+正文;
 * 首行 = 请求方法+空格+URL+空格+版本号; eg: POST /index.html?a=1 HTTP/1.1
 *
 * @param client 与客户端通信的套接字文件描述符
 */
void accept_request(int client)
{
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;
    char *query_string = NULL;

    // 读取HTTP请求的首行数据
    numchars = get_line(client, buf, sizeof(buf));
    printf("first line:%s\n", buf);

    // 获取请求方法
    i = j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';

    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        // 请求方法不是GET和POST，直接返回错误信息
        unimplemented(client);
        return;
    }

    if (strcasecmp(method, "POST") == 0)
    {
        cgi = 0;
    }

    // 跳过所有空格
    while (ISspace(buf[j]) && (j < sizeof(buf)))
    {
        j++;
    }
    // 读取首行的URL，放到缓冲区url中
    i = 0;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && j < sizeof(buf))
    {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    // 对于GET请求解析URL参数，获取?之后的键值对暂存在query_string中
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
        {
            query_string++;
        }
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "/home/wen/workspace/acwj%s",
            url); // htdocs即HyperText Online Document System，常用作网站根目录的目录名称
    if (path[strlen(path) - 1] == '/')
    {
        strcat(path, "index.html");
    }

    printf("method:%s\nurl:%s\nquery_string:%s\n\n", method, url, query_string);

    // 获取请求的文件，如果文件不存在则返回Not Found
    /**
     * int stat(const char *file, struct stat *buf)
     * stat()函数用于获取文件的状态信息，file文件文件路径，buf为暂存文件信息的缓冲区
     * 成功获取文件信息则返回0，出现错误则返回-1
     */
    if (stat(path, &st) == -1)
    {
        // 读取该请求后续的所有内容hearder+body并忽略
        while ((numchars > 0) && strcmp("\n", buf))
        {
            numchars = get_line(client, buf, sizeof(buf));
        }
        not_found(client);
    }
    else
    {
        // st.st_mode 与 __S_IFMT 按位与判断文件类型
        if ((st.st_mode & __S_IFMT) == __S_IFDIR)
        {
            // 该路径为目录，则再拼接"/indec.html"
            strcat(path, "/index.html");
        }

        if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
        {
            // 该文件为可执行文件，置cgi为1
        }

        if (!cgi)
        {
            // 不需要cgi，直接返回文件
            serve_file(client, path);
        }
        else
        {
            execute_cgi(client, path, method, query_string);
        }
    }
    close(client);
}

int main(int argc, char *argv[])
{
    printf("Hello httpd!\n");
    int server_sock = -1;
    int client_sock = -1;
    unsigned short port = 4000;
    // IPv4 套接字地址结构
    struct sockaddr_in client_name;
    int client_name_len = sizeof(client_name);

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        /**
         *  接受客户端请求，创建新的套接字用于与客户端通信，返回新的套接字的文件描述符
         *  @param fd 正在监听的套接字的文件描述符
         *  @param addr struct sockaddr结构体，存储客户端地址信息
         *  @param len addr的长度
         */
        client_sock = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);

        if (client_sock == -1)
        {
            error_die("accept");
        }
        accept_request(client_sock);
    }

    close(server_sock);
    return (0);
}
