#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>

#include "../lock/locker.h"
#include "../log/log.h"
using namespace std;

// 数据库链接池
// 单例模式，保证唯一
class connection_pool
{
    public:
        MYSQL *GetConnection();     // get sql connection
        bool ReleaseConnection(MYSQL *con);   // 释放连接
        int GetFreeConn();          // 获取链接
        void DestroyPool();         // 销毁所有连接

        // 单例模式连接
        static connection_pool *GetInstace();

        void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);

    private:
        // 单例模式构造和析构都要私有化
        connection_pool();
        ~connection_pool();

        int m_MaxConn;
        int m_CurConn;
        int m_FreeConn;
        locker lock;        // 互斥锁
        list<MYSQL *> connList; //连接池
        sem reserve;
    
    public:
        string m_url;
        string m_Port;
        string m_User;
        string m_PassWord;
        string m_DataBaseName;
        int m_close_log;
};

// Resource Acquisition Is Initialization（wiki上面翻译成 “资源获取就是初始化”）
class connectionRAII{
    public:
        connectionRAII(MYSQL **con, connection_pool *connPool);
        ~connectionRAII();
    
    private:
        MYSQL *conRAII;
        connection_pool *poolRAII;
};

#endif