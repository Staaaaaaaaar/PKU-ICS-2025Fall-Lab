# ICS - Proxy Lab | 论一个代理的自我修养

在 Proxy Lab 中，我们将编写一个简单的 HTTP 代理，用于将客户端的请求转发到网页服务器，读取服务器的响应，然后将响应转发给客户端。Lab 分为三个部分：

- 在第一部分，我们要设置代理以接受传入的连接，读取并解析请求，将请求转发到 Web 服务器，读取服务器的响应，并将这些响应转发给相应的客户端。
- 在第二部分，我们要升级代理以处理多个并发连接。
- 在第三部分，我们要为代理添加缓存功能。

## 预备知识

### Makefile 基础

Makefile 是自动化构建系统的核心工具，它通过定义规则和依赖关系，高效地管理项目编译过程。

**基本语法**：

```makefile
# 变量定义
# 变量名 = 值
CC = gcc
CFLAGS = -g -Wall

# 目标: 依赖文件
# 	命令（注意：命令前必须使用Tab缩进）
csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c
```

**常用规则**：

- `all`：默认目标，通常依赖于主要可执行文件
- `clean`：清理编译生成的文件
- 编译模式：`$(CC) $(CFLAGS) -c 源文件`

### 网络编程基础

- **Socket API**：网络通信的基础接口，包括`socket()`、`bind()`、`listen()`、`accept()`、`connect()`等
- **HTTP 协议**：理解请求/响应格式、头部字段、状态码
- **I/O 模型**：掌握 RIO 函数（如`rio_readlineb`）
- **地址转换**：`getaddrinfo`/`getnameinfo` 用于主机名与 IP 地址的转换

### 并发编程基础

- **多线程模型**：使用 `pthread_create` 创建线程，`pthread_detach` 设置分离状态
- **共享资源**：使用信号量用于协调多线程访问共享资源
- **读者-写者问题**：经典同步模式，适用于缓存等读多写少的场景
- **线程安全**：理解可重入函数，避免在信号处理程序中调用非异步信号安全函数

## 前置准备

### 环境

参照 `writeup`，我们需要安装一些包来支持本地测试和调试。

```bash
sudo apt update
sudo apt install net-tools curl
sudo apt install python3-selenium chromium-chromedriver
sudo snap install chromium
```

### TINY Web Server

Proxy Lab 为我们提供了一个简易的 Web 服务器供调试和本地评测使用。

使用方式为在 `./tiny` 目录下编译并运行 `tiny`：

```bash
cd ./tiny && make clean && make && ./tiny <port>
```

服务器会解析 HTTP 请求并提供相应内容，我们可以使用 `curl` 工具向这个服务器发送请求来查看其响应内容。例如，在确保服务器运行的情况下在终端输入：

```bash
curl -v http://localhost:<port>/home.html
```

注意使用服务端对应的端口号，会得到下面文本：

```text
* Host localhost:12345 was resolved.
* IPv6: ::1
* IPv4: 127.0.0.1
*   Trying [::1]:12345...
* connect to ::1 port 12345 from ::1 port 42792 failed: Connection refused
*   Trying 127.0.0.1:12345...
* Connected to localhost (127.0.0.1) port 12345
> GET /home.html HTTP/1.1
> Host: localhost:12345
> User-Agent: curl/8.5.0
> Accept: */*
>
* HTTP 1.0, assume close after body
< HTTP/1.0 200 OK
< Server: Tiny Web Server
< Connection: close
< Content-length: 120
< Vary: *
< Cache-Control: no-cache, no-store, must-revalidate
< Content-type: text/html
<
<html>
<head><title>test</title></head>
<body>
<img align="middle" src="godzilla.gif">
Dave O'Hallaron
</body>
</html>
* Closing connection
```

### csapp.h

Lab 的工作目录中包含了 `csapp.h` 和 `csapp.c`。鉴于网络编程的完整实现比较繁琐，我们可以使用其中的一些封装函数来简化代码并提高可读性。

但请注意：

- `csapp.h` 中的错误处理包装函数可能并不适合在代理中使用，因为它们默认的处理方式往往是终止进程，这不是一个需要长期稳定运行的代理所希望的。
- 如果认为需要，你也可以自行修改相关函数的实现。

### 代理测试

除了运行 `driver.sh` 进行本地评测外，我们还可以自行对代理进行一些基本功能的测试。

`curl` 是一个强大的命令行工具，可以用来发送各种 HTTP 请求，并将响应内容显示在终端。我们可以使用它来测试代理服务器是否正确地转发请求和响应。

在两个终端中分别运行 TINY Web Server 和代理：

```bash
cd ./tiny && ./tiny 1234
```

```bash
./proxy 12345
```

使用 `curl` 经代理向 TINY Web Server 发送请求：

```bash
curl -v --proxy http://localhost:12345 http://localhost:1234/home.html
```

也可以向其他服务端发送请求：

```bash
curl -v --proxy http://localhost:12345 http://www.baidu.com
```

## Part 1: Basic Proxy

### 主循环

在我们的代码里，主循环负责初始化代理服务器，监听指定端口，接受客户端连接，并为每个连接调用请求处理函数，我们可以在 `main` 函数中实现主循环。

```c
int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    Signal(SIGPIPE, SIG_IGN);

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }
    return 0;
}
```

**实现思路：**

- **命令行参数检查**：确保用户提供了端口号参数
- **忽略 SIGPIPE 信号**：防止因写入已关闭的 socket 而导致进程意外退出
- **创建监听套接字**：使用 `Open_listenfd()` 函数创建并绑定到指定端口
- **主循环**：无限循环接受客户端连接，每次接受连接后：

  - 获取客户端信息（主机名和端口）

  - 打印连接信息用于调试

  - 调用`doit()`函数处理请求

  - 处理完成后关闭连接

### 请求处理

`doit` 函数是代理的核心，负责读取客户端请求，解析请求，连接目标服务器，转发请求，并将响应返回给客户端。

```c
void doit(int fd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
    char http_header[MAXLINE];
    rio_t rio;
    int serverfd;
    ssize_t n;

    Rio_readinitb(&rio, fd);
    if (rio_readlineb(&rio, buf, MAXLINE) <= 0) {
        return;
    }
    /* Reject overlong request lines to avoid buffer misuse */
    if (strlen(buf) >= MAXLINE - 1 && buf[MAXLINE - 2] != '\n') {
        const char *body = "URI too long\n";
        char resp[MAXLINE];
        int resp_len = snprintf(resp, sizeof(resp),
                                 "HTTP/1.0 414 Request-URI Too Long\r\n"
                                 "Connection: close\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: %zu\r\n\r\n%s",
                                strlen(body), body);
        if (resp_len > 0) {
            rio_writen(fd, resp, resp_len);
        }
        return;
    }
    printf("Request:\n");
    printf("%s", buf);
    if (sscanf(buf, "%s %s %s", method, uri, version) != 3) {
        return;
    }

    if (strcasecmp(method, "GET")) {
        printf("Proxy does not implement the method");
        return;
    }

    parse_uri(uri, hostname, path, port);

    build_http_header(http_header, hostname, path, port, &rio);

    serverfd = open_clientfd(hostname, port);
    if (serverfd < 0) {
        printf("Connection failed\n");
        return;
    }

    if (rio_writen(serverfd, http_header, strlen(http_header)) != strlen(http_header)) {
        Close(serverfd);
        return;
    }

    while ((n = read(serverfd, buf, MAXLINE)) != 0) {
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == ECONNRESET) break;
            break;
        }

        if (rio_writen(fd, buf, n) != n) {
             if (errno == EPIPE) break;
            break;
        }
    }
    Close(serverfd);
}
```

**实现思路：**

- **初始化读缓冲区**：使用 `rio_readinitb` 初始化一个带缓冲的 I/O 流

- **读取请求行**：读取 HTTP 请求的第一行，形如

  ```text
  GET http://www.cmu.edu/hub/index.html HTTP/1.1
  ```

- **请求行验证**：

  - 检查请求行长度，防止缓冲区溢出（主要针对超长 URI 的测试用例）
  - 解析请求行，提取方法、URI 和 HTTP 版本
  - 验证请求方法是否为 GET（本代理仅支持 GET 方法）

- **URI 解析**：调用 `parse_uri` 函数解析 URI，提取目标主机名、路径和端口号

- **构建 HTTP 头**：调用 `build_http_header` 构建发送给目标服务器的 HTTP 请求头

- **连接目标服务器**：使用 `open_clientfd` 建立到目标服务器的连接

- **转发请求**：将构建好的 HTTP 请求头发送到目标服务器

- **转发响应**：循环读取目标服务器的响应，并将其转发给客户端（注意谨慎处理各种错误，以保证代理持续运行）

### URI 解析

`parse_uri` 函数负责解析客户端请求中的 URI，提取主机名、路径和端口号。

```c
void parse_uri(char *uri, char *hostname, char *path, char *port)
{
    char *ptr;

    if (strstr(uri, "http://")) {
        ptr = uri + 7;
    } else {
        ptr = uri;
    }

    char *port_ptr = strstr(ptr, ":");
    char *path_ptr = strstr(ptr, "/");

    if (path_ptr == NULL) {
        strcpy(path, "/");
    } else {
        strcpy(path, path_ptr);
        *path_ptr = '\0';
    }

    if (port_ptr != NULL) {
        *port_ptr = '\0';
        strcpy(port, port_ptr + 1);
        strcpy(hostname, ptr);
    } else {
        strcpy(port, "80");
        strcpy(hostname, ptr);
    }
}
```

**实现思路：**

- **处理 URI 格式**：支持两种格式的 URI：

  - 完整格式：`http://hostname:port/path`

  - 简化格式：`hostname/path`

- **提取路径**：
  - 查找第一个 `/` 字符，将其作为路径的开始
  - 如果没有找到 `/`，则使用默认路径 `/`
  - 将路径部分复制到 `path` 参数中
- **提取端口和主机名**：

  - 查找 `:` 字符，判断是否有自定义端口
  - 如果有自定义端口，将其分离并复制到 `port` 参数
  - 如果没有指定端口，使用默认 HTTP 端口 80
  - 剩余部分作为主机名

### HTTP 请求头构建

`build_http_header` 函数负责构建发送给目标服务器的 HTTP 请求头。

```c
void build_http_header(char *http_header, char *hostname, char *path, char *port, rio_t *client_rio)
{
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

    sprintf(request_hdr, "GET %s HTTP/1.0\r\n", path);

    other_hdr[0] = '\0';
    host_hdr[0] = '\0';

    while(rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        if(strcmp(buf, "\r\n") == 0) break;

        if(!strncasecmp(buf, "Host", 4)) {
            strcpy(host_hdr, buf);
            continue;
        }

        if(!strncasecmp(buf, "Connection", 10) ||
           !strncasecmp(buf, "Proxy-Connection", 16) ||
           !strncasecmp(buf, "User-Agent", 10)) {
            continue;
        }

        strcat(other_hdr, buf);
    }

    if(strlen(host_hdr) == 0) {
        sprintf(host_hdr, "Host: %s\r\n", hostname);
    }

    sprintf(http_header, "%s%s%s%s%s%s\r\n",
            request_hdr,
            host_hdr,
            "Connection: close\r\n",
            "Proxy-Connection: close\r\n",
            user_agent_hdr,
            other_hdr);

    return;
}
```

**实现思路：**

- **构建请求行**：使用 GET 方法和提取的路径构建请求行，使用 HTTP/1.0 协议
- **处理客户端请求头**：
  - 逐行读取客户端发送的所有请求头
  - 特殊处理 `Host` 头：保留客户端提供的 `Host` 头，如果客户端没有提供 `Host` 头，使用从 URI 中提取的主机名构建
  - 设置 `Connection: close`，确保服务器在响应后关闭连接
  - 设置 `Proxy-Connection: close`，与旧版代理兼容
  - 设置统一的 `User-Agent` 头，伪装成标准浏览器
  - 保存其他所有头字段
- **组合最终请求头**：将所有部分组合成完整的 HTTP 请求头

## Part 2: Concurrency

### 多线程架构设计

在多线程架构中，主循环负责接受客户端连接，但不再直接处理请求，而是为每个新连接创建一个独立的工作线程。这样，主线程可以立即返回继续接受新的连接，而不必等待当前请求处理完成。

```c
int main(int argc, char **argv)
{
    int listenfd, *connfdp;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    Signal(SIGPIPE, SIG_IGN);

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
    return 0;
}
```

**实现思路：**

- **动态内存分配**：为**避免竞争**，为每个连接描述符动态分配内存，确保每个线程有自己独立的连接描述符副本
- **线程创建**：使用 `Pthread_create` 为每个新连接创建一个工作线程，将连接描述符指针作为参数传递给线程函数 `thread`
- **非阻塞接受连接**：主线程在创建线程后立即返回继续接受新连接，不需要等待请求处理完成
- **资源管理**：线程负责在处理完成后释放连接描述符和相关内存资源

### 线程函数实现

线程函数是每个工作线程的入口点，负责处理单个客户端连接的完整生命周期。

```c
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}
```

**实现思路：**

- **参数解引用**：从传入的参数指针中获取连接描述符
- **线程分离**：调用 `Pthread_detach(pthread_self())` 将线程设置为分离状态，这样线程终止时系统会自动回收其资源，无需主线程调用 `pthread_join`
- **内存释放**：立即释放动态分配的内存，避免内存泄漏
- **请求处理**：调用 `doit` 函数处理客户端请求
- **资源清理**：处理完成后关闭连接描述符，释放文件描述符资源

## Part 3: Cache

### 缓存数据结构设计

我们可以设计一个简单的缓存系统，使用固定大小的块来存储 HTTP 响应内容，在缓存头文件 `cache.h` 中声明相关结构和函数。

```c
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_CACHE 10

typedef struct
{
    char obj[MAX_OBJECT_SIZE];
    char uri[MAXLINE];
    int lru_cnt;
    int isEmpty;
    int size;
} Block;

typedef struct
{
    Block data[MAX_CACHE];
    int num;
} Cache;

void cache_init();
int cache_find(char *url, char *buf, int *size);
void cache_add(char *url, char *buf, int size);
```

- **缓存块定义**：每个`Block`包含：
  - `obj`：存储响应内容的缓冲区
  - `uri`：对应的请求 URI（作为缓存键）
  - `lru_cnt`：最近最少使用计数器，用于缓存替换策略
  - `isEmpty`：标记块是否为空
  - `size`：存储内容的实际大小
- **缓存结构**：`Cache` 结构包含一个固定大小的块数组和当前已使用的块数量
- **缓存参数**：
  - `MAX_CACHE_SIZE`：整个缓存的最大大小（约 1MB）
  - `MAX_OBJECT_SIZE`：单个对象的最大大小（100KB）
  - `MAX_CACHE`：缓存中最多可存储的对象数量（10 个）

### 读者-写者模型

由于多个线程可能同时访问缓存，因此我们需要确保线程安全。这里采用了经典的第一类读者-写者模型。

#### 初始化

```c
static Cache cache;
static int read_cnt;
static sem_t mutex, w;

void cache_init() {
    cache.num = 0;
    read_cnt = 0;
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0, 1);
    for (int i = 0; i < MAX_CACHE; i++) {
        cache.data[i].isEmpty = 1;
        cache.data[i].lru_cnt = 0;
    }
}
```

#### 线程安全语句封装

将第一类读者-写者模型所需的**加锁**和**解锁**语句封装，使后续代码更直观。

```c
static void reader_lock() {
    P(&mutex);
    read_cnt++;
    if (read_cnt == 1) {
        P(&w);
    }
    V(&mutex);
}

static void reader_unlock() {
    P(&mutex);
    read_cnt--;
    if (read_cnt == 0) {
        V(&w);
    }
    V(&mutex);
}

static void writer_lock() {
    P(&w);
}

static void writer_unlock() {
    V(&w);
}
```

- **信号量设计**：
  - `mutex`：保护对`read_cnt`的访问
  - `w`：写者锁，确保写操作的互斥性
- **读者优先策略**：
  - 第一个读者获取写锁，最后一个读者释放写锁
  - 允许多个读者同时读取
- **写者独占**：写者必须获取独占锁，确保在修改缓存时没有其他线程可以访问

#### 缓存查找

```c
int cache_find(char *url, char *buf, int *size) {
    int found = 0;
    reader_lock();
    for (int i = 0; i < MAX_CACHE; i++) {
        if (!cache.data[i].isEmpty && strcmp(cache.data[i].uri, url) == 0) {
            memcpy(buf, cache.data[i].obj, cache.data[i].size);
            *size = cache.data[i].size;
            found = 1;
            break;
        }
    }
    reader_unlock();

    if (found) {
        // Update LRU - this is a write operation on metadata
        writer_lock();
        for (int i = 0; i < MAX_CACHE; i++) {
            if (!cache.data[i].isEmpty && strcmp(cache.data[i].uri, url) == 0) {
                cache.data[i].lru_cnt = 0;
                for(int j = 0; j < MAX_CACHE; j++) {
                    if(j != i && !cache.data[j].isEmpty) {
                        cache.data[j].lru_cnt++;
                    }
                }
                break;
            }
        }
        writer_unlock();
        return 1;
    }

    return 0;
}
```

**实现思路：**

- **读锁保护**：在查找过程中使用读锁，允许多个线程同时查询缓存
- **LRU 更新**：找到缓存项后，需要更新 LRU 计数器：

  - 获取写锁（因为要修改缓存元数据）

  - 将找到的项的 LRU 计数器重置为 0（表示最近使用）

  - 增加其他所有非空项的 LRU 计数器

#### 缓存添加

```c
void cache_add(char *url, char *buf, int size) {
    if (size > MAX_OBJECT_SIZE) return;

    writer_lock();

    int victim = -1;
    int max_lru = -1;

    if (cache.num < MAX_CACHE) {
        for (int i = 0; i < MAX_CACHE; i++) {
            if (cache.data[i].isEmpty) {
                victim = i;
                break;
            }
        }
    }

    if (victim == -1) {
        for (int i = 0; i < MAX_CACHE; i++) {
            if (!cache.data[i].isEmpty && cache.data[i].lru_cnt > max_lru) {
                max_lru = cache.data[i].lru_cnt;
                victim = i;
            }
        }
    }

    if (victim != -1) {
        if (cache.data[victim].isEmpty) {
            cache.num++;
        }

        strcpy(cache.data[victim].uri, url);
        memcpy(cache.data[victim].obj, buf, size);
        cache.data[victim].size = size;
        cache.data[victim].isEmpty = 0;
        cache.data[victim].lru_cnt = 0;

        for(int j = 0; j < MAX_CACHE; j++) {
            if(j != victim && !cache.data[j].isEmpty) {
                cache.data[j].lru_cnt++;
            }
        }
    }

    writer_unlock();
}
```

**实现思路：**

- **大小限制**：首先检查对象大小是否超过单个对象的最大限制
- **写锁保护**：整个添加过程在写锁保护下进行，确保操作的原子性
- **缓存替换策略**：

  - 如果缓存未满，优先使用空闲块

  - 如果缓存已满，选择 LRU 计数器最大的块作为替换目标

- **LRU 更新**：

  - 新添加的项 LRU 计数器设为 0（最近使用）
  - 其他所有非空项的 LRU 计数器增加 1

- **元数据更新**：更新 URI、内容、大小和空闲状态

### 代理中集成缓存功能

#### 更新 main 函数

在 `main` 函数的开头部分输出化缓存。

```c
int main(int argc, char **argv)
{
    // ... 其他初始化代码

    Signal(SIGPIPE, SIG_IGN);
    cache_init();  // 初始化缓存系统

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        // ... 接受连接和创建线程的代码
    }
    return 0;
}
```

#### 更新 doit 函数

```c
void doit(int fd)
{
    // ... 变量声明
    char *cache_buf = Malloc(MAX_OBJECT_SIZE);
    int obj_size = 0; /* accumulate full object size for caching */
    char url_key[MAXLINE];

    Rio_readinitb(&rio, fd);
    // ... 读取请求行和验证

    strncpy(url_key, uri, MAXLINE - 1);
    url_key[MAXLINE - 1] = '\0';

    // ... 验证GET方法

    /* 检查缓存 - 如果找到，直接返回缓存内容 */
    if (cache_find(url_key, cache_buf, &obj_size)) {
        rio_writen(fd, cache_buf, obj_size);
        printf("Served from cache\n");
        Free(cache_buf);
        return;
    }

    // ... 解析URI、构建请求头、连接服务器的代码

    /* 读取服务器响应并转发给客户端，同时累积内容到缓存缓冲区 */
    while ((n = read(serverfd, buf, MAXLINE)) != 0) {
        // ... 错误处理

        if (rio_writen(fd, buf, n) != n) {
            // ... 错误处理
        }

        /* 累积整个对象到缓存缓冲区，直到达到大小限制 */
        if (obj_size >= 0 && obj_size + n <= MAX_OBJECT_SIZE) {
            memcpy(cache_buf + obj_size, buf, n);
            obj_size += n;
        } else {
            obj_size = -1; /* 标记为太大无法缓存 */
        }
    }

    /* 如果对象大小合适，添加到缓存 */
    if (obj_size >= 0) {
        cache_add(url_key, cache_buf, obj_size);
    }

    Close(serverfd);
    Free(cache_buf);
}
```

- **缓存缓冲区**：为每个请求分配一个缓存缓冲区，用于累积响应内容
- **缓存查找**：
  - 在连接目标服务器前，先检查缓存中是否有对应 URI 的内容
  - 如果找到，直接将缓存内容写回客户端，跳过后续处理
- **缓存累积**：
  - 在转发服务器响应给客户端的同时，将内容累积到缓存缓冲区
  - 检查累积大小，如果超过限制，标记为不可缓存
- **缓存添加**：
  - 请求处理完成后，如果对象大小合适，将其添加到缓存
  - 使用原始请求 URI 作为缓存键，确保相同请求可以命中缓存

### 修改 Makefile

由于我们引入了缓存模块，因此需要修改 `Makefile` 文件。

```makefile
#
# Makefile for Proxy Lab
#
# You may modify this file any way you like (except for the handin
# rule). Autolab will execute the command "make" on your specific
# Makefile to build your proxy from sources.
#
CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

cache.o: cache.c cache.h csapp.h
	$(CC) $(CFLAGS) -c cache.c

proxy.o: proxy.c csapp.h cache.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o cache.o
	$(CC) $(CFLAGS) proxy.o csapp.o cache.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you should then
# hand in to Autolab. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar czvf proxylab-handin.tar.gz proxylab-handout)

clean:
	rm -rf *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz .proxy .noproxy
```

---

[更适合北大宝宝体质的 Proxy Lab 踩坑记](https://arthals.ink/blog/proxy-lab)

[CSAPP | Lab9-Proxy Lab 深入解析](https://zhuanlan.zhihu.com/p/497982541)

[CSAPP - Proxylab](https://lonelyuan.github.io/p/csapp-proxylab)
