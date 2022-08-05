#include "webserver.h"

WebServer::WebServer()
{
    users = new http_conn[MAX_FD];

    // root 文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    // 定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

// 初始化参数
void WebServer::init(int port, string user, string passWord, string databaseName,
                  int log_write, int opt_linger, int trigmode, int sql_num,
                  int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_password = passWord;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actor_model = actor_model;
}

void WebServer::trig_mode()
{
    // LT + LT
    if(m_TRIGMode == 0)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    else if(m_TRIGMode == 1)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    else if(m_TRIGMode == 2)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    else if(m_TRIGMode == 3)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}


void WebServer::log_write()
{
    if(m_close_log == 0)
    {
        if(m_log_write == 1)
        {
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        }
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

// 初始化一个sql连接池
void WebServer::sql_pool()
{
    m_connPool = connection_pool::GetInstace();
    m_connPool->init("localhost", m_user, m_password, m_databaseName, 3306, m_sql_num, m_close_log);

    // 初始化数据库
    users->initmysql_result(m_connPool); // 读取表，
}

// 初始化一个http请求线程池
void WebServer::thread_pool()
{
    // 线程池
    m_pool = new threadpool<http_conn>(m_actor_model, m_connPool, m_thread_num);
    // http请求线程
}

void WebServer::eventListen()
{
    // 这个是服务器socket
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 优雅关闭连接
    // SO_LINGER采取 l_onoff, l_linger最长时限
    // onoff = 1 l_linger = 0 立即关闭链接
    // onoff = 1 l_linger > 0 等待超时或者发送完成
    if(m_OPT_LINGER == 0)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if(m_OPT_LINGER == 1)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    // 等同memset address = 0
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);

    // epoll 创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5); // 创建epoll例程
    assert(m_epollfd != -1);

    // 把文件描述符加入 并设置非阻塞
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    //套接字可以用于网络通信，也可以用于本机内的进程通信
    // 函数用于创建一对无名的、相互连接的套接子。
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    // 当服务器close一个连接时，若client端接着发数据。
    // 根据TCP协议的规定，会收到一个RST响应，client再往这个服务器发送数据时，
    // 系统会发出一个SIGPIPE信号给进程，告诉进程这个连接已经断开了，不要再写了。
    // 根据信号的默认处理规则SIGPIPE信号的默认执行动作是terminate(终止、退出),
    // 所以client会退出。若不想客户端退出可以把SIGPIPE设为SIG_IGN
    utils.addsig(SIGPIPE, SIG_IGN); // 为了防止客户端进程终止，而导致服务器进程被SIGPIPE信号终止，因此服务器程序要处理SIGPIPE信号。
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false); // SIGTERM也可以称为软终止，因为接收SIGTERM信号的进程可以选择忽略它。
    
    alarm(TIMESLOT);

    // 计时器
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_password, m_databaseName);
    // 初始化client data
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;

    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    // 超时则调用回调函数
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

// 删除计时器
void WebServer::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if(m_LISTENTrigmode == 0) // 监听LT
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if(connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }
    else 
    {
        while(1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if(connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(ret == -1)
    {
        return false;
    }
    else if(ret == 0)
    {
        return false;
    }
    else{
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {                
                timeout = true;
                break;
            }
            case SIGTERM:
            {                
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    // reactor 非阻塞同步模式
    if(m_actor_model == 1)
    {
        if(timer)
        {
            adjust_timer(timer);
        }
        m_pool->append(users + sockfd, 0);

        while(true)
        {
            if(users[sockfd].improv == 1)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break; 
            }
        }
    }
    else{
        //proactor
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            //若监测到读事件，将该事件放入请求队列
            m_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    //reactor
    if (1 == m_actor_model)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR) // 错误为EINTR表示在读/写的时候出现了中断错误
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == m_listenfd)
            {
                bool flag = dealclientdata();
                if(flag == false)
                    continue;
            }
            // EPOLLRDHUP：断开连接或半关闭的情况，这在边缘触发方式下非常有用
            // EPOLLERR：发生错误的情况
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            // 处理信号
            else if((sockfd == m_pipefd[0]) && events[i].events & EPOLLIN)
            {
                bool flag = dealwithsignal(timeout, stop_server);
                if(false == flag)
                    LOG_ERROR("%s", "dealclientdata failure")
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if(timeout)
        {
            utils.timer_handler();
            
            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }
}