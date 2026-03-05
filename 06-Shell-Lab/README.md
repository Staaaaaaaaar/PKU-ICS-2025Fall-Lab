# ICS - Shell Lab | 信号屏蔽中，勿扰！

在 Shell Lab 中，我们将编写一个简单的 Linux shell 程序，来实现简单的作业控制和 I/O 重定向。

具体而言，我们将主要实现以下函数：

- `eval`：解析命令行的主程序。
- `sigchld_handler`：捕获 SIGCHLD 信号。
- `sigint_handler`：捕获 SIGINT (Ctrl-C) 信号。
- `sigtstp_handler`：捕获 SIGTSTP (Ctrl-Z) 信号。

## 预备知识

在深入 `tsh.c` 的实现细节之前，理解 Shell Lab 所依赖的 **进程控制** 和 **信号处理** 是至关重要的。

### 进程控制

#### fork、execve 和 waitpid

- **`fork()`**: 用于创建一个新的子进程。子进程几乎是父进程的**精确副本**。
- **`execve()`**: 用于在当前进程的上下文中加载并运行一个新的程序。成功调用后，它会用新的程序替换掉当前进程的整个可执行映像、数据和堆栈，但**保持 PID 不变**。
- **`waitpid()`**: 父进程用于等待子进程终止或停止。在 `tsh` 中，我们使用带有 `WNOHANG` 和 `WUNTRACED` 选项的 `waitpid` 来异步回收僵尸进程和捕获停止的子进程。

#### 进程组控制

- **进程组 (Process Group)**: 一组相关的进程的集合，由一个进程组 ID (PGID) 标识。作业控制的本质是内核将信号发送给整个进程组，而不是单个进程。
- **`setpgid(pid, pgid)`**: 设置进程 `pid` 的进程组 ID 为 `pgid`。在 `tsh` 中，子进程会调用 `setpgid(0, 0)` 将自己放入一个新的、以其自身 PID 为 PGID 的进程组中。这确保了每个作业都是一个独立的进程组。

### 信号处理

信号是进程间通信的一种方式，用于通知进程发生了某种事件。

#### 关键信号及其作用

- **`SIGCHLD`**: 当子进程停止或终止时，发送给父进程。
- **`SIGINT`**: 默认行为是终止进程。当用户输入 Ctrl+C 时，发送给**前台进程组**。
- **`SIGTSTP`**: 默认行为是停止进程。当用户输入 Ctrl+Z 时，发送给**前台进程组**。

#### 信号屏蔽与同步

- **`sigprocmask()`**: 用于阻塞（添加到屏蔽集）、解除阻塞（从屏蔽集中移除）或设置进程的信号屏蔽字。
- **`sigsuspend()`**: **原子性**地将进程的信号屏蔽字替换为 `set`，然后挂起进程，直到捕获到一个信号。在信号处理程序返回后，它恢复原始的信号屏蔽字。这是实现前台作业**等待**的关键机制。

### 异步信号安全

信号处理程序是异步执行的，这意味着它们可能在程序执行的任何时刻中断主程序。在信号处理程序中，我们只能调用**异步信号安全**的函数。

### I/O 重定向

- **文件描述符 (FD)**: 内核用来标识打开的文件或 I/O 设备的非负整数。
  - `STDIN_FILENO` (0): 标准输入
  - `STDOUT_FILENO` (1): 标准输出
  - `STDERR_FILENO` (2): 标准错误
- **`open()`**: 打开或创建一个文件，并返回一个新的文件描述符。
- **`dup2(oldfd, newfd)`**: 是实现 I/O 重定向的核心。它会强制将 `newfd` 重定向到 `oldfd` 所指向的同一文件表项。

## 前置准备

### 包装函数

为了简化错误处理，我们可以使用一些错误处理包装函数。这些包装函数会在调用失败时打印错误信息并终止程序。
它们一般形如：

```c
pid_t Fork(void)
{
    pid_t pid;

    if ((pid = fork()) < 0)
	unix_error("Fork error");
    return pid;
}
```

其他的包装函数可以参考 `csapp.h` 和 `csapp.c` 文件（可从 [CS:APP 资源页面](http://csapp.cs.cmu.edu/3e/code.html) 下载）。

### 代码概览

在实现 Shell Lab 之前，建议先浏览一下 `tsh.c` 文件，了解代码结构和各个函数的作用。`tsh.c` 文件包含了 Shell Lab 的主要代码框架，我们将在此基础上进行实现。

#### 结构体

`tsh.c` 定义了两个关键的结构体用于管理作业（jobs）和解析命令行。

##### job_t

该结构体用于表示一个作业（进程或进程组）。

```c
struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t job_list[MAXJOBS]; /* The job list */
```

- **`pid`**: 作业的进程 ID（我们只考察单进程作业）。
- **`jid`**: 作业的 ID，从 1 开始分配。
- **`state`**: 作业的状态，可以是 `FG`（前台）、`BG`（后台）、`ST`（停止）或 `UNDEF`。
- **`cmdline`**: 启动该作业的命令行字符串。
- `job_list[MAXJOBS]` 是一个全局数组，用于存储当前所有的作业。

##### cmdline_tokens

该结构体用于存储解析后的命令行参数和重定向信息。

```c
struct cmdline_tokens {
    int argc;               /* Number of arguments */
    char *argv[MAXARGS];    /* The arguments list */
    char *infile;           /* The input file */
    char *outfile;          /* The output file */
    enum builtins_t {       /* Indicates if argv[0] is a builtin command */
        BUILTIN_NONE,
        BUILTIN_QUIT,
        BUILTIN_JOBS,
        BUILTIN_BG,
        BUILTIN_FG,
        BUILTIN_KILL,
        BUILTIN_NOHUP} builtins;
};
```

- **`argc`**: 参数的数量。
- **`argv`**: 参数列表（类似于 `main` 函数的 `argv`）。
- **`infile`**: 输入重定向文件名（如果存在）。
- **`outfile`**: 输出重定向文件名（如果存在）。
- **`builtins`**: 一个枚举值，指示命令行是否是一个内置命令（如 `quit`, `jobs`, `bg`, `fg` 等）。

#### 主要函数

`tsh.c` 中的主要函数包括 `main` 和 `eval`，它们分别负责初始化 Shell 和评估命令行。

##### main

`main` 函数是 Shell 的入口点，它负责初始化、安装信号处理程序并执行主循环。

```c
int main(int argc, char **argv)
{
    // ... (命令行参数解析和错误重定向)

    /* Install the signal handlers */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(job_list);

    /* Execute the shell's read/eval loop */
    while (1) {

        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) {
            /* End of file (ctrl-d) */
            // ... (退出逻辑)
            exit(0);
        }

        /* Remove the trailing newline */
        cmdline[strlen(cmdline)-1] = '\0';

        /* Evaluate the command line */
        eval(cmdline);

        fflush(stdout);
        fflush(stdout);
    }

    exit(0); /* control never reaches here */
}
```

- **初始化**: 调用 `dup2(1, 2)` 将标准错误重定向到标准输出。
- **关联信号回调函数**: 调用 `Signal` 包装函数安装 `SIGINT`、`SIGTSTP` 和 `SIGCHLD` 的处理程序，以及忽略 `SIGTTIN` 和 `SIGTTOU`。
- **作业列表初始化**: 调用 `initjobs` 清空作业列表。
- **主循环**: 循环读取用户输入的命令行，调用 `eval` 函数评估和执行命令。

##### eval

`eval` 函数是 Shell 的核心评估逻辑，它解析命令行并决定如何执行命令。

```c
void eval(char *cmdline)
{
    int bg;              /* should the job run in bg or fg? */
    struct cmdline_tokens tok;

    /* Parse command line */
    bg = parseline(cmdline, &tok);

    if (bg == -1) /* parsing error */
        return;
    if (tok.argv[0] == NULL) /* ignore empty lines */
        return;

    // TODO: 需要在此处实现

    return;
}
```

- 调用 `parseline` 解析命令行，并确定是否为后台作业（`bg`）。
- 根据 `tok.builtins` 字段判断是否为内置命令。
- 如果不是内置命令，则需要实现 `fork` 子进程、设置进程组 ID、处理 I/O 重定向、`execve` 执行程序，并将作业添加到作业列表中（`addjob`）。
- 如果是前台作业，需要等待子进程终止或停止。

#### 信号处理函数

信号处理函数用于捕获和响应特定的信号。

##### sigchld_handler

当子进程停止或终止时，内核会发送 `SIGCHLD` 信号给父进程（shell）。

```c
void
sigchld_handler(int sig)
{
    // TODO: 需要在此处实现
    return;
}
```

##### sigint_handler

当用户按下 **Ctrl+C** 时，内核会发送 `SIGINT` 信号给前台进程组。Shell 捕获此信号后，需要将其转发给前台作业。

```c
void
sigint_handler(int sig)
{
    // TODO: 需要在此处实现
    return;
}
```

##### sigtstp_handler

当用户按下 **Ctrl+Z** 时，内核会发送 `SIGTSTP` 信号给前台进程组。Shell 捕获此信号后，需要将其转发给前台作业。

```c
void
sigtstp_handler(int sig)
{
    // TODO: 需要在此处实现
    return;
}
```

#### 辅助函数

`tsh.c` 中还包含了一些已实现的辅助函数，用于解析命令行和管理作业列表。

##### parseline

解析用户输入的命令行字符串，填充 `cmdline_tokens` 结构体。

```c
int
parseline(const char *cmdline, struct cmdline_tokens *tok)
{
    // ... (详细的解析逻辑)
    return is_bg; // 1 for BG, 0 for FG, -1 for error
}
```

- **功能**: 将命令行分割成参数（`argv`），识别输入/输出重定向文件（`infile`/`outfile`），识别内置命令，并判断是否为后台作业（`&`）。
- **返回值**: `1` 表示后台作业（`BG`），`0` 表示前台作业（`FG`），`-1` 表示解析错误。

##### 作业列表管理函数

这些函数用于维护全局作业列表 `job_list`。

| 函数名      | 描述                                         |
| :---------- | :------------------------------------------- |
| `clearjob`  | 清空指定 `job_t` 结构体的内容。              |
| `initjobs`  | 初始化整个作业列表。                         |
| `maxjid`    | 返回当前最大的作业 ID。                      |
| `addjob`    | 向作业列表添加一个新作业，并分配一个 JID。   |
| `deletejob` | 根据 PID 从作业列表中删除一个作业。          |
| `fgpid`     | 返回当前前台作业的 PID，如果没有则返回 0。   |
| `getjobpid` | 根据 PID 查找作业，返回指向 `job_t` 的指针。 |
| `getjobjid` | 根据 JID 查找作业，返回指向 `job_t` 的指针。 |
| `pid2jid`   | 将 PID 映射到 JID。                          |
| `listjobs`  | 打印作业列表到指定的输出文件描述符。         |

##### 其他函数

除此之外，`tsh.c` 中还提供了一些其他辅助函数，用于错误处理和信号包装。我们也可以添加额外的错误处理包装函数。

## 前台作业

前台作业 (Foreground Job, FG) 是 Shell 最基本的执行模式。当用户输入一个不以 `&` 符号结尾的命令时，`tsh` 会将其作为前台作业启动，并暂停 Shell 的执行，直到该作业终止或被停止。其核心逻辑体现在 `eval` 函数中。

### 阻塞信号

在 `tsh` 创建新作业时，**竞争条件** 是一个需要避免的关键问题。具体来说，如果在父进程将子进程添加到作业列表之前，子进程就结束了，并向父进程发送了 `SIGCHLD` 信号，那么信号处理函数 (`sigchld_handler`) 将无法识别该 PID，导致作业信息丢失。

为了防止这种竞争，父进程在 `fork` 之前，必须先阻塞可能影响作业列表的关键信号，这里主要是 `SIGCHLD`，`SIGINT` 和 `SIGTSTP`。
    
```c
// tsh.c - eval 函数片段
pid_t pid;
sigset_t mask, prev;

Sigemptyset(&mask);
Sigaddset(&mask, SIGCHLD);
Sigaddset(&mask, SIGINT);
Sigaddset(&mask, SIGTSTP);
/* Block SIGCHLD */
Sigprocmask(SIG_BLOCK, &mask, &prev);

if ((pid = Fork()) == 0) {
    // ... 子进程逻辑
} else {
    addjob(job_list, pid, bg ? BG : FG, cmdline);
    Sigprocmask(SIG_SETMASK, &prev, NULL);
}

addjob(job_list, pid, bg ? BG : FG, cmdline);
Sigprocmask(SIG_SETMASK, &prev, NULL);

// ... 等待逻辑
```

在调用 `Fork()` 之前，Shell 阻塞信号，确保子进程创建后，只有在父进程安全地将该作业信息添加到全局列表 (`addjob`) 之后，才会解除信号阻塞，使得 `SIGCHLD` 可以被处理。

### 子进程逻辑

子进程在执行新程序之前，需要完成两个重要的设置：

1.  **恢复信号屏蔽**: `fork` 会继承父进程的信号屏蔽字。子进程必须恢复到之前的信号屏蔽字 (`prev`)，以便能够响应信号。同时，它需要将 `SIGINT` 和 `SIGTSTP` 的处理程序设置为默认 (`SIG_DFL`)，确保这些信号能够终止或停止自身。

2.  **设置进程组 ID**: 为了实现作业控制，每个新创建的前台或后台作业都必须拥有一个**唯一的进程组 ID (GID)**。这是为了防止用户在键盘上输入 Ctrl+C 或 Ctrl+Z 时，内核将信号错误地发送给 Shell 进程。

    - 通过调用 `Setpgid(0, 0)`，子进程将自己的进程组 ID 设置为自己的 PID。

```c
// tsh.c - eval 函数片段 (子进程逻辑)
if ((pid = Fork()) == 0) {
    Sigprocmask(SIG_SETMASK, &prev, NULL);
    Signal(SIGINT, SIG_DFL);
    Signal(SIGTSTP, SIG_DFL);

    Setpgid(0, 0);

    Execve(tok.argv[0], tok.argv, environ);
}
```

### 等待机制

当父进程（Shell）启动了一个前台作业后，它必须等待该作业结束。

为了避免浪费 CPU 资源，Shell 使用 `Sigsuspend` 函数来挂起自身，等待 `SIGCHLD` 信号的到来。（详情可参考 CSAPP 8.5.7）

`Sigsuspend` 会在接收到**任何信号**后返回。所以，不能简单地在 `Sigsuspend` 之后就退出等待，因为其他不相关的信号（如定时器信号等）也可能导致进程被唤醒。因此，需要使用以下 `while` 循环来确保只有当前台作业彻底执行结束后，父进程才会继续运行：

```c
// tsh.c - eval 函数片段
if (!bg) {
    while(pid == fgpid(job_list))
        Sigsuspend(&prev);
}
```

## 后台作业

后台作业（Background Job, BG）是 `tsh` 的另一种执行模式，由用户在命令行末尾添加 `&` 符号指定。与前台作业不同，Shell 在启动后台作业后不会等待其完成。这使得用户可以立即在 Shell 中输入和执行其他命令，实现了并发操作。

后台作业的实现相对简单，当 `bg` 为真时，父进程立即打印出新作业的 JID、PID 和完整的命令行，然后 `eval` 函数返回，Shell 继续主循环，等待用户输入下一个命令。

```c
// tsh.c - eval 函数片段
if (!bg) {
    while(pid == fgpid(job_list))
        Sigsuspend(&prev);
} else {
    printf("[%d] (%d) %s\n", pid2jid(pid), pid, cmdline);
}
```

## 内置命令

内置命令（Builtin Commands）不需要 `fork` 子进程运行，而是直接在 `tsh` 进程中调用相应的函数逻辑。`tsh` 主要实现了 `quit`、`jobs`、`bg`、`fg` 和 `kill` 五个内置命令。

我们可以将内置命令的处理包装成 `buildin_cmd` 函数，并在 `eval` 函数中调用。

```c
// tsh.c - eval 函数片段

if (!buildin_cmd(&tok, cmdline)){
    // ... 前台/后台处理逻辑
}
```

### quit

`quit` 命令是最简单的内置命令，用于优雅地终止 Shell 进程（当然，也可以通过 Ctrl+D 退出）。

```c
// tsh.c - buildin_cmd 片段
    case BUILTIN_QUIT:
        exit(0); // 退出 Shell
        return 1;
```

### jobs

`jobs` 命令用于列出当前 Shell 中所有已启动的作业。

```c
// tsh.c - buildin_cmd 片段
    case BUILTIN_JOBS:
        listjobs(job_list, STDOUT_FILENO); // 将作业列表打印到标准输出
        return 1;
```

### bg

`bg job` 命令通过发送 SIGCONT 信号来重新启动作业，然后在后台运行它。job 参数可以是 PID 或 JID。

```c
// tsh.c - buildin_cmd 片段
    case BUILTIN_BG:
        {
            struct job_t *job = NULL;
            int id;

            if (tok->argc < 2) {
                printf("bg command requires PID or %%jobid argument\n");
                return 1;
            }

            if (tok->argv[1][0] == '%') {
                id = atoi(&tok->argv[1][1]);
                job = getjobjid(job_list, id);
                if (job == NULL) {
                    printf("%%%d: No such job\n", id);
                    return 1;
                }
            } else {
                pid_t pid = atoi(tok->argv[1]);
                job = getjobpid(job_list, pid);
                if (job == NULL) {
                    printf("(%d): No such process\n", pid);
                    return 1;
                }
            }

            job->state = BG;
            Kill(-job->pid, SIGCONT);
            printf("[%d] (%d) %s\n", job->jid, job->pid, job->cmdline);
        }
        return 1;
```

- **参数解析**: `bg` 命令接受 PID 或 JID（格式为 `%JID`）作为参数。`getjobpid` 或 `getjobjid` 被调用来查找对应的作业结构体。
- **状态转换**: 将找到的作业状态更新为 `BG`。
- **发送信号**: 使用 `Kill(-job->pid, SIGCONT)` 向该作业的**进程组**发送 `SIGCONT` 信号。`SIGCONT` 信号会使处于停止状态的进程或进程组继续运行。

### fg

`fg job` 命令通过发送 SIGCONT 信号来重新启动作业，然后在前台运行它。job 参数可以是 PID 或 JID。

```c
// tsh.c - buildin_cmd 片段
    case BUILTIN_FG:
        {
            struct job_t *job = NULL;
            int id;
            pid_t pid;

            if (tok->argc < 2) {
                printf("fg command requires PID or %%jobid argument\n");
                return 1;
            }

            if (tok->argv[1][0] == '%') {
                id = atoi(&tok->argv[1][1]);
                job = getjobjid(job_list, id);
                if (job == NULL) {
                    printf("%%%d: No such job\n", id);
                    return 1;
                }
            } else {
                pid = atoi(tok->argv[1]);
                job = getjobpid(job_list, pid);
                if (job == NULL) {
                    printf("(%d): No such process\n", pid);
                    return 1;
                }
            }

            job->state = FG;
            pid = job->pid;
            Kill(pid, SIGCONT);

            /* Wait for foreground job to terminate */
            sigset_t empty_mask;
            Sigemptyset(&empty_mask);
            while(pid == fgpid(job_list))
                Sigsuspend(&empty_mask);
        }
        return 1;
```

- **参数解析**: 同样解析 PID 或 JID，查找对应的作业。
- **状态转换**: 将找到的作业状态更新为 `FG`。
- **发送信号**: 发送 `SIGCONT` 信号恢复作业。
- **等待**: 与 `eval` 中启动前台作业的逻辑类似，Shell 必须进入 `Sigsuspend` 循环等待，直到该作业从作业列表中移除（终止）或状态不再是前台（停止）。

### kill

`kill job` 命令通过向每个相关进程发送 SIGTERM 信号来终止作业列表中的作业或一个进程组。job 参数可以是 PID 或 JID。

```c
// tsh.c - buildin_cmd 片段
    case BUILTIN_KILL:
        {
            struct job_t *job = NULL;
            int id;
            pid_t pid;

            if (tok->argc < 2) {
                printf("kill command requires PID or %%jobid argument\n");
                return 1;
            }

            if (tok->argv[1][0] == '%') {
                if ((id = atoi(&tok->argv[1][1])) == 0) {
                    printf("kill: argument must be a PID or %%jobid\n");
                    return 1;
                }
                id = id > 0 ? id : -id;
                job = getjobjid(job_list, id);
                if (job == NULL) {
                    printf("%%%d: No such job\n", id);
                    return 1;
                }
                Kill(-job->pid, SIGTERM);
            } else {
                if ((pid = atoi(tok->argv[1])) == 0) {
                    printf("kill: argument must be a PID or %%jobid\n");
                    return 1;
                }

                if (pid > 0) {
                    job = getjobpid(job_list, pid);
                    if (job == NULL) {
                        printf("(%d): No such process\n", pid);
                        return 1;
                    }
                    Kill(pid, SIGTERM);
                } else {
                    pid = -pid;
                    job = getjobpid(job_list, pid);
                    if (job == NULL) {
                        printf("(%d): No such process group\n", pid);
                        return 1;
                    }
                    Kill(-pid, SIGTERM);
                }
            }
        }
        return 1;
```

- **参数解析**: `kill` 也接受 PID 或 JID 作为参数。
- **发送信号**: 如果是 JID，则使用 `Kill(-job->pid, SIGTERM)` 向整个进程组发送终止信号。如果是 PID，则直接向该进程发送 `SIGTERM` 信号。

### nohup

`nohup [command]` 命令使后续命令忽略任何 SIGHUP 信号。我们的 Shell 不需要支持遵循此原则的内置命令，`command` 是可执行文件的路径，后跟其参数。

```c
// tsh.c - buildin_cmd 片段
    case BUILTIN_NOHUP:
        {
            sigset_t mask, prev;
            Sigemptyset(&mask);
            Sigaddset(&mask, SIGHUP);
            /* Block SIGHUP */
            Sigprocmask(SIG_BLOCK, &mask, &prev);

            eval(cmdline + 6); // 6 is the length of "nohup "

            Sigprocmask(SIG_SETMASK, &prev, NULL);
        }
        return 1;
```

在执行实际命令前**阻塞 SIGHUP 信号**，并将命令以递归调用 `eval` 的方式执行，实现对该信号的忽略。

## 信号处理

`tsh` 必须能捕获并正确处理三个关键信号：SIGINT (Ctrl+C)、SIGTSTP (Ctrl+Z) 和 SIGCHLD (子进程状态变化)。

在 `main` 函数中，我们通过 `Signal` 函数绑定这些信号的处理函数：

```c
// tsh.c - main 函数片段
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
```

### sigint_handler

当用户按下 **Ctrl+C** 时，Shell 收到 `SIGINT` 信号。该处理函数的任务是**转发**这个信号给当前的前台进程组。

```c
// tsh.c - sigint_handler 函数
void sigint_handler(int sig)
{
    int olderrno = errno;
    pid_t pid = fgpid(job_list);
	if (pid)
        Kill(-pid, sig);
    errno = olderrno;
    return;
}
```

- **定位前台**: 调用 `fgpid` 查找当前的前台作业 PID。
- **发送信号**: 使用 `Kill(-pid, sig)` 向整个前台**进程组**发送信号。负号 `-` 确保信号被发送给所有属于该进程组的成员，包括其子进程，从而实现正确的作业终止。
- **保存 errno**: 异步信号安全的函数会在出错返回时设置 `errno`。在处理程序中调用这样的函数可能会干扰主程序中其他依赖于 `errno` 的部分。我们需要在进入处理程序时把 `errno` 保存在一个局部变量中，在处理程序返回前恢复它。

### sigtstp_handler

当用户按下 **Ctrl+Z** 时，Shell 收到 `SIGTSTP` 信号。该处理函数的任务是**转发**这个信号给当前的前台进程组，使其停止运行。

```c
// tsh.c - sigtstp_handler 函数
void sigtstp_handler(int sig)
{
    int olderrno = errno;
    pid_t pid = fgpid(job_list);
	if (pid)
        Kill(-pid, sig);
    errno = olderrno;
    return;
}
```

- **定位前台**: 同样调用 `fgpid` 获取前台作业 PID。
- **发送信号**: 使用 `Kill(-pid, sig)` 向整个前台**进程组**发送信号。子进程收到该信号后会停止（暂停）。
- **保存 errno**: 异步信号安全的函数会在出错返回时设置 `errno`。在处理程序中调用这样的函数可能会干扰主程序中其他依赖于 `errno` 的部分。我们需要在进入处理程序时把 `errno` 保存在一个局部变量中，在处理程序返回前恢复它。

### sigchld_handler

`SIGCHLD` 信号在子进程终止或停止时被发送给父进程。`sigchld_handler` 的职责是处理所有终止或停止的子进程，并更新作业列表的状态。

```c
// tsh.c - sigchld_handler 函数
void sigchld_handler(int sig)
{
    int olderrno = errno;
    pid_t pid;
    int status;
    sigset_t mask_all, prev_all;

    Sigfillset(&mask_all);

    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (WIFEXITED(status)) {
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(job_list, pid);
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        if (WIFSIGNALED(status)) {
            sio_put("Job [%d] (%d) terminated by signal %d\n",
                pid2jid(pid), pid, WTERMSIG(status));
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(job_list, pid);
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        if (WIFSTOPPED(status)) {
            sio_put("Job [%d] (%d) stopped by signal %d\n",
                pid2jid(pid), pid, WSTOPSIG(status));
            struct job_t *job = getjobpid(job_list, pid);
            if (job != NULL) {
                job->state = ST;
            }
        }
    }

    if (pid < 0 && errno != ECHILD)
        unix_error("waitpid error");

    errno = olderrno;
    return;
}
```

- **保存 errno**: 异步信号安全的函数（如 `waitpid`）会在出错返回时设置 `errno`。在处理程序中调用这样的函数可能会干扰主程序中其他依赖于 `errno` 的部分。我们需要在进入处理程序时把 `errno` 保存在一个局部变量中，在处理程序返回前恢复它。
- **循环回收子进程**: 由于 pending 机制，需使用 `while` 循环调用 `waitpid(-1, &status, WNOHANG | WUNTRACED)`，直到没有子进程需要处理。
- **状态判断与处理**: 子进程发送 `SIGCHLD` 信号给父进程的情况主要有三种：
  - **子进程自然终止**: 在 `WIFEXITED(status)` 分支中处理，调用 `deletejob` 从作业列表中移除该作业。
  - **子进程收到终止信号**: 在 `WIFSIGNALED(status)` 分支中处理，打印终止信息并调用 `deletejob` 移除作业。
  - **子进程收到停止信号**: 在 `WIFSTOPPED(status)` 分支中处理，打印停止信息并将作业状态更新为 `ST`。
- **阻塞信号**：由于 `sigchld_handler` 中涉及对 `job_list` 和 `job` 的修改不是异步信号安全的操作，我们需要在修改作业列表时阻塞所有信号，防止数据竞争。
- **异步安全 I/O**: 所有的输出（如 "terminated by signal"）都必须使用 `sio_put` 系列函数，确保在信号上下文中调用的函数是安全的。

## I/O 重定向

I/O 重定向允许用户改变命令的标准输入和标准输出，使其不再指向终端，而是指向一个文件。`tsh` 支持以下两种重定向操作：

- **输入重定向**: `< infile`，将文件的内容作为命令的标准输入。
- **输出重定向**: `> outfile`，将命令的标准输出写入文件，如果文件存在则覆盖。

重定向的实现依赖于 Unix/Linux 的文件描述符 (File Descriptor, FD) 机制，通过操作 `STDIN_FILENO` 和 `STDOUT_FILENO` 来实现。

由于 I/O 重定向会改变 Shell 进程自身的 FD，我们必须在执行命令前**保存**原始的 FD，并在命令执行完成后**恢复**它们，确保 Shell 能够继续正常工作。

```c
// tsh.c - eval 函数片段 (I/O 重定向部分)
void eval(char *cmdline)
{
    // ... 解析命令行，tok 结构体包含 infile/outfile 信息
    int infile_fd, outfile_fd;
    int saved_stdin, saved_stdout;

    /* 处理输入重定向 */
    if (tok.infile) {
        saved_stdin = Dup(STDIN_FILENO);
        infile_fd = Open(tok.infile, O_RDONLY, DEF_MODE);
        Dup2(infile_fd, STDIN_FILENO);
        Close(infile_fd);
    }

    /* 处理输出重定向 */
    if (tok.outfile) {
        saved_stdout = Dup(STDOUT_FILENO);
        outfile_fd = Open(tok.outfile, O_WRONLY | O_CREAT | O_TRUNC, DEF_MODE);
        Dup2(outfile_fd, STDOUT_FILENO);
        Close(outfile_fd);
    }

    // ... 执行命令逻辑：builtin_cmd 或 fork/execve

    /* 恢复 I/O 设置 */
    if (tok.infile) {
        Dup2(saved_stdin, STDIN_FILENO);
        Close(saved_stdin);
    }
    if (tok.outfile) {
        Dup2(saved_stdout, STDOUT_FILENO);
        Close(saved_stdout);
    }

    return;
}
```

- **保存原始文件描述符**: 在进行任何重定向之前，Shell 必须调用 `Dup` 函数（`dup` 的包装器）来**复制**当前的 `STDIN_FILENO` 和 `STDOUT_FILENO`。这个操作创建了一个新的 FD (`saved_stdin`)，它指向与标准输入**相同的打开文件表项**。这个新的 FD 将被用于后续的 I/O 恢复。
- **建立新的重定向连接**:
  - **打开文件**: 使用 `Open` 函数打开或创建目标文件。
  - **复制描述符**: 使用 `Dup2(newfd, oldfd)` 关闭 `oldfd`（例如 `STDIN_FILENO`），然后将 `newfd` 复制到 `oldfd` 的位置。
- **恢复文件描述符**: Shell 必须在 `eval` 函数结束前恢复其 I/O 状态，以确保能够继续正常工作。

## 完整代码

### eval

```c
void eval(char *cmdline)
{
    int bg;              /* should the job run in bg or fg? */
    struct cmdline_tokens tok;
    int infile_fd, outfile_fd;
    int saved_stdin, saved_stdout;

    /* Parse command line */
    bg = parseline(cmdline, &tok);

    if (bg == -1) /* parsing error */
        return;
    if (tok.argv[0] == NULL) /* ignore empty lines */
        return;

    if (tok.infile) {
        saved_stdin = Dup(STDIN_FILENO);
        infile_fd = Open(tok.infile, O_RDONLY, DEF_MODE);
        Dup2(infile_fd, STDIN_FILENO);
        Close(infile_fd);
    }
    if (tok.outfile) {
        saved_stdout = Dup(STDOUT_FILENO);
        outfile_fd = Open(tok.outfile, O_WRONLY | O_CREAT | O_TRUNC, DEF_MODE);
        Dup2(outfile_fd, STDOUT_FILENO);
        Close(outfile_fd);
    }

    if (!buildin_cmd(&tok, cmdline)){ /* Not a builtin command */
        pid_t pid;
        sigset_t mask, prev;

        Sigemptyset(&mask);
        Sigaddset(&mask, SIGCHLD);
        Sigaddset(&mask, SIGINT);
        Sigaddset(&mask, SIGTSTP);
        /* Block SIGCHLD */
        Sigprocmask(SIG_BLOCK, &mask, &prev);

        if ((pid = Fork()) == 0) {
            Sigprocmask(SIG_SETMASK, &prev, NULL);
            Signal(SIGINT, SIG_DFL);
            Signal(SIGTSTP, SIG_DFL);
            Setpgid(0, 0); /* Set a new process group */
            Execve(tok.argv[0], tok.argv, environ);
        } else {
            addjob(job_list, pid, bg ? BG : FG, cmdline);
            Sigprocmask(SIG_SETMASK, &prev, NULL);
        }

        /* Parent waits for foreground job to terminate */
        if (!bg) {
            while(pid == fgpid(job_list))
                Sigsuspend(&prev);
        } else {
            printf("[%d] (%d) %s\n", pid2jid(pid), pid, cmdline);
        }
    }

    if (tok.infile) {
        Dup2(saved_stdin, STDIN_FILENO);
        Close(saved_stdin);
    }
    if (tok.outfile) {
        Dup2(saved_stdout, STDOUT_FILENO);
        Close(saved_stdout);
    }

    return;
}

int buildin_cmd(struct cmdline_tokens *tok, char *cmdline) {
    switch (tok->builtins)
    {
    case BUILTIN_QUIT:
        exit(0);
    case BUILTIN_JOBS:
        listjobs(job_list, STDOUT_FILENO);
        return 1;
    case BUILTIN_BG:
        {
            struct job_t *job = NULL;
            int id;

            if (tok->argc < 2) {
                printf("bg command requires PID or %%jobid argument\n");
                return 1;
            }

            if (tok->argv[1][0] == '%') {
                id = atoi(&tok->argv[1][1]);
                job = getjobjid(job_list, id);
                if (job == NULL) {
                    printf("%%%d: No such job\n", id);
                    return 1;
                }
            } else {
                pid_t pid = atoi(tok->argv[1]);
                job = getjobpid(job_list, pid);
                if (job == NULL) {
                    printf("(%d): No such process\n", pid);
                    return 1;
                }
            }

            job->state = BG;
            Kill(-job->pid, SIGCONT);
            printf("[%d] (%d) %s\n", job->jid, job->pid, job->cmdline);
        }
        return 1;
    case BUILTIN_FG:
        {
            struct job_t *job = NULL;
            int id;
            pid_t pid;

            if (tok->argc < 2) {
                printf("fg command requires PID or %%jobid argument\n");
                return 1;
            }

            if (tok->argv[1][0] == '%') {
                id = atoi(&tok->argv[1][1]);
                job = getjobjid(job_list, id);
                if (job == NULL) {
                    printf("%%%d: No such job\n", id);
                    return 1;
                }
            } else {
                pid = atoi(tok->argv[1]);
                job = getjobpid(job_list, pid);
                if (job == NULL) {
                    printf("(%d): No such process\n", pid);
                    return 1;
                }
            }

            job->state = FG;
            pid = job->pid;
            Kill(pid, SIGCONT);

            /* Wait for foreground job to terminate */
            sigset_t empty_mask;
            Sigemptyset(&empty_mask);
            while(pid == fgpid(job_list))
                Sigsuspend(&empty_mask);
        }
        return 1;
    case BUILTIN_NOHUP:
        {
            sigset_t mask, prev;
            Sigemptyset(&mask);
            Sigaddset(&mask, SIGHUP);
            /* Block SIGHUP */
            Sigprocmask(SIG_BLOCK, &mask, &prev);

            eval(cmdline + 6); // 6 is the length of "nohup "

            Sigprocmask(SIG_SETMASK, &prev, NULL);
        }
        return 1;
    case BUILTIN_KILL:
        {
            struct job_t *job = NULL;
            int id;
            pid_t pid;

            if (tok->argc < 2) {
                printf("kill command requires PID or %%jobid argument\n");
                return 1;
            }

            if (tok->argv[1][0] == '%') {
                if ((id = atoi(&tok->argv[1][1])) == 0) {
                    printf("kill: argument must be a PID or %%jobid\n");
                    return 1;
                }
                id = id > 0 ? id : -id;
                job = getjobjid(job_list, id);
                if (job == NULL) {
                    printf("%%%d: No such job\n", id);
                    return 1;
                }
                Kill(-job->pid, SIGTERM);
            } else {
                if ((pid = atoi(tok->argv[1])) == 0) {
                    printf("kill: argument must be a PID or %%jobid\n");
                    return 1;
                }

                if (pid > 0) {
                    job = getjobpid(job_list, pid);
                    if (job == NULL) {
                        printf("(%d): No such process\n", pid);
                        return 1;
                    }
                    Kill(pid, SIGTERM);
                } else {
                    pid = -pid;
                    job = getjobpid(job_list, pid);
                    if (job == NULL) {
                        printf("(%d): No such process group\n", pid);
                        return 1;
                    }
                    Kill(-pid, SIGTERM);
                }
            }
        }
        return 1;
    default:
        return 0;
    }
}
```

### sigchld_handler

```c
void sigchld_handler(int sig)
{
    int olderrno = errno;
    pid_t pid;
    int status;
    sigset_t mask_all, prev_all;

    Sigfillset(&mask_all);

    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (WIFEXITED(status)) {
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(job_list, pid);
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        if (WIFSIGNALED(status)) {
            sio_put("Job [%d] (%d) terminated by signal %d\n",
                pid2jid(pid), pid, WTERMSIG(status));
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(job_list, pid);
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        if (WIFSTOPPED(status)) {
            sio_put("Job [%d] (%d) stopped by signal %d\n",
                pid2jid(pid), pid, WSTOPSIG(status));
            struct job_t *job = getjobpid(job_list, pid);
            if (job != NULL) {
                job->state = ST;
            }
        }
    }

    if (pid < 0 && errno != ECHILD)
        unix_error("waitpid error");

    errno = olderrno;
    return;
}
```

### sigint_handler

```c
void sigint_handler(int sig)
{
    int olderrno = errno;
    pid_t pid = fgpid(job_list);
	if (pid)
        Kill(-pid, sig);
    errno = olderrno;
    return;
}
```

### sigtstp_handler

```c
void sigtstp_handler(int sig)
{
    int olderrno = errno;
    pid_t pid = fgpid(job_list);
	if (pid)
        Kill(-pid, sig);
    errno = olderrno;
    return;
}
```

---

[更适合北大宝宝体质的 Tsh Lab 踩坑记](https://arthals.ink/blog/tsh-lab)

[CSAPP Lab5 实验记录 —— Shell Lab（实验分析 + 完整代码）](https://blog.csdn.net/qq_37500516/article/details/120836083)

[CSAPP 课程 Lab5 Shell Lab](https://zhuanlan.zhihu.com/p/662872049)