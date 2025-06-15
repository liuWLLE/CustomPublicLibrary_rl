#include "rlcmd.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "rl/rlstr.h"

#define __FILENAME__ "rlcmd"

int rl_system_100ms_ex(const char *cmdStr, int timeout_count, int try_count)
{
    if (rl_str_isempty(cmdStr) == RL_TRUE)
    {
        rl_log_error("[%s:%s:%d] cmdStr invalid", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 处理 SIGINT 和 SIGQUIT 信号
    // 暂时忽略这两个信号,确保父进程不会意外终止，同时子进程可以独立执行
    struct sigaction ignore, saveintr, savequit;
    // SIG_IGN 表示忽略该信号
    ignore.sa_handler = SIG_IGN;
    // 清空 ignore 结构体中的 sa_mask，表示不屏蔽任何额外的信号
    sigemptyset(&ignore.sa_mask);
    // sa_flags 设为 0，表示不启用额外的信号处理选项
    ignore.sa_flags = 0;
    // 忽略 SIGINT（Ctrl+C）和 SIGQUIT（Ctrl+\），防止命令执行时被终端中断
    if (sigaction(SIGINT, &ignore, &saveintr) < 0)
    {
        rl_log_error("[%s:%s:%d] sigaction:SIGINT failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    if (sigaction(SIGQUIT, &ignore, &savequit) < 0)
    {
        rl_log_error("[%s:%s:%d] sigaction:SIGQUIT failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 临时屏蔽 SIGCHLD 信号，并确保子进程退出后不会变成僵尸进程
    // chldmask：存储要屏蔽的信号集合（SIGCHLD）、savemask：存储原来的信号屏蔽集，以便稍后恢复
    sigset_t chldmask, savemask;
    // 定义 sighandler_t 为信号处理函数的类型,sighandler_t 变量就可以用于存储 signal() 的返回值
    typedef void (*sighandler_t)(int);
    sighandler_t old_handler;
    // 初始化 chldmask，清空信号集
    sigemptyset(&chldmask);
    // 向 chldmask 中添加 SIGCHLD，表示要屏蔽这个信号（SIGCHLD 是子进程终止时发送给父进程的信号）
    sigaddset(&chldmask, SIGCHLD);
    // 屏蔽 SIGCHLD 信号，即暂时不让 SIGCHLD 触发（&savemask 用于保存原来的信号屏蔽状态，以便后续恢复）
    if (sigprocmask(SIG_BLOCK, &chldmask, &savemask) < 0)
    {
        rl_log_error("[%s:%s:%d] sigprocmask:SIG_BLOCK failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    // 将 SIGCHLD 信号的处理方式设置为默认 (SIG_DFL)，当子进程终止时，不会变成僵尸进程，而是直接被回收
    old_handler = signal(SIGCHLD, SIG_DFL);

    // 标志是否需要重试
    int needRetry = RL_TRUE;
    // 存储 fork() 产生的子进程 ID
    pid_t pid;
    // 存储 waitpid() 的返回状态
    int status = RL_SUCCESS;
    for (int i = -1; i < try_count && RL_TRUE == needRetry; i++)
    {
        // 表示第一次执行命令 不算重试，后续才是重试次数
        needRetry = RL_FALSE;

        // 创建子进程
        pid = fork();
        // 创建失败
        if (pid < 0)
        {
            status = RL_FAILED;
            exit(EXIT_FAILURE);
        }
        // 进入子进程的执行逻辑
        else if (0 == pid)
        {
            // 子进程恢复 SIGINT 和 SIGQUIT 的默认行为，避免继承父进程的信号屏蔽状态
            sigaction(SIGINT, &saveintr, NULL);
            sigaction(SIGQUIT, &savequit, NULL);
            sigprocmask(SIG_SETMASK, &savemask, NULL);
            // /bin/sh 是默认 shell 解释器的路径
            // sh 是 shell 解释器
            // -c 让 sh 以命令行模式运行
            // 执行 cmdStr 作为 shell 命
            // NULL 表示参数列表结束
            execl("/bin/sh", "sh", "-c", cmdStr, NULL);
            // 如果 execl() 失败，_exit(127) 退出，避免影响父进程（127 是标准 shell 退出码，表示命令未找到）
            _exit(127);
        }
        // 父进程等待子进程执行完成
        else
        {
            int leftTime = timeout_count;
            int rv = RL_SUCCESS;
            // 如果子进程未结束，立即返回 0
            // 如果子进程结束，waitpid() 返回 pid，status 存储子进程的退出状态
            while ((rv = waitpid(pid, &status, WNOHANG)) <= 0)
            {
                // 如果 waitpid() 返回 -1，且 errno 不是 EINTR（被信号打断），说明进程出错，退出循环
                if (RL_FAILED == rv && EINTR != errno)
                {
                    status = RL_FAILED;
                    break;
                }
                if (timeout_count > 0)
                {
                    // 休眠 100 ms
                    usleep(100 * 1000);
                    leftTime--;
                    // 超时时
                    if (leftTime <= 0)
                    {
                        rl_log_error("[%s:%s:%d] cmd:%s retry=%d try_count...", __FILENAME__, __FUNCTION__, __LINE__, cmdStr, i);
                        // 强制杀死子进程，确保不会一直卡住
                        if (kill(pid, SIGKILL) == 0)
                        {
                            // 确保子进程资源被释放，避免僵尸进程
                            waitpid(pid, NULL, 0);
                        }
                        // 设置 status = RL_FAILED，表示失败
                        status = RL_FAILED;
                        // 允许重试
                        needRetry = RL_TRUE;
                        break;
                    }
                }
            }
        }
    }

    // 恢复 SIGCHLD 处理，防止影响后续子进程
    if (sigprocmask(SIG_SETMASK, &savemask, NULL) < 0)
    {
        rl_log_error("[%s:%s:%d] sigprocmask:SIG_SETMASK failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    signal(SIGCHLD, old_handler);
    // 恢复 SIGINT 和 SIGQUIT，确保程序能正常响应中断信号
    if (sigaction(SIGINT, &saveintr, NULL) < 0)
    {
        rl_log_error("[%s:%s:%d] sigaction:SIGINT failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    if (sigaction(SIGQUIT, &savequit, NULL) < 0)
    {
        rl_log_error("[%s:%s:%d] sigaction:SIGQUIT failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 如果子进程正常终止
    if (WIFEXITED(status))
    {
        return RL_SUCCESS;
    }
    // 如果子进程被信号终止（负数表示被信号终止）
    else if (WIFSIGNALED(status))
    {
        return -WTERMSIG(status);
    }
    else
    {
        rl_log_error("[%s:%s:%d] function failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
}
