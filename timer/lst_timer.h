#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

class util_timer;

// 客户数据类型， 
struct client_data
{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

class util_timer
{
    public:
        util_timer() : prev(NULL), next(NULL){}
    
    public:
        time_t expire;

        void (*cb_func)(client_data *);
        client_data *user_data;
        util_timer *prev;
        util_timer *next;
};

// 升序链表管理计时器
class sort_timer_lst
{
    public:
        sort_timer_lst();
        ~sort_timer_lst();

        void add_timer(util_timer *timer);
        void adjust_timer(util_timer *timer);
        void del_timer(util_timer *timer);
        void tick();
    
    private:
        void add_timer(util_timer *timer, util_timer *lst_head);

        util_timer *head;
        util_timer *tail;
};

class Utils
{
    public:
        Utils() {}
        ~Utils() {}

        void init(int timeslot);

        // 文件描述符非阻塞
        int setnonblocking(int fd);

        // 需要在注册时间时加上EPOLLSHOT标志，EPOLLSHOT相当于说，
        // 某次循环中epoll_wait唤醒该事件fd后，就会从注册中删除该fd,
        // 也就是说以后不会epollfd的表格中将不会再有这个fd,也就不会出现多个线程同时处理一个fd的情况。
        // 内核事件表注册读事件 ET
        void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

        // 信号处理函数
        static void sig_handler(int sig);

        // 设置信号函数
        void addsig(int sig, void(handler)(int), bool restart = true);

        // 不断触发sigalarm
        void timer_handler();

        void show_error(int connfd, const char *info);

    public:
        static int *u_pipefd;
        sort_timer_lst m_timer_lst;
        static int u_epollfd;
        int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif