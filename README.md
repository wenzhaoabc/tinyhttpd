# tinyhttpd

## 简介

tinyhttpd是一个简易的http服务器，支持CGI。其结构简单，但包含了网络请求处理的一般步骤，适合用于练习linux网络编程，tinyhttpd包含了Linux上进程的创建，管道的使用，socket编程的基本方法和http协议的基本结构。

tinyhttp主要包含以下函数

```c
void accept_request(int);   // 处理从套接字上监听到的一个 HTTP 请求
void bad_request(int);  // 返回错误请求
void cat(int, FILE *);  // 读取服务器上某个文件写到 socket 套接字
void cannot_execute(int);   // 处理发生在执行 cgi 程序时出现的错误
void error_die(const char *);   // 把错误信息写到 perror，终止程序
void execute_cgi(int, const char *, const char *, const char *);    // 运行cgi脚本，创建子进程执行脚本
int get_line(int, char *, int); // 读取一行HTTP报文
void headers(int, const char *);    // 返回HTTP响应头
void not_found(int);    // 找不到请求文件
void serve_file(int, const char *); // 调用 cat 把服务器文件内容返回给浏览器。
int startup(u_short *); // 开启http服务，包括绑定端口，监听，开启线程处理链接
void unimplemented(int);    // 返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持。
```

作者对tinyhttpd的源码进行了阅读练习，并添加了较为详细的注释

## 运行

```sh
# 编译
make httpd client

# 运行服务端
./httpd.out
```

## 参考

- https://github.com/cbsheng/tinyhttpd
- https://zhuanlan.zhihu.com/p/24941375
