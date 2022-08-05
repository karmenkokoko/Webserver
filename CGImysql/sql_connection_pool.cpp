#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
    m_CurConn = 0;  // 当前连接数
    m_FreeConn = 0; // 空闲连接数
}


// 单例模式获取类对象
connection_pool* connection_pool::GetInstace()
{
    static connection_pool conn_pool;
    return &conn_pool;
}


// 初始化
void connection_pool::init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log)
{
    m_url = url;
	m_Port = Port;
	m_User = User;
	m_PassWord = PassWord;
	m_DataBaseName = DataBaseName;
	m_close_log = close_log;

    for (int i = 0; i < MaxConn; ++i)
    {
        MYSQL *con = NULL;
        con = mysql_init(con);

        if(con == NULL)
        {
            // 日志记录数据库连接失败
            LOG_ERROR("MYSQL ERROR");
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DataBaseName.c_str(), Port, NULL, 0);

        if(con == NULL)
        {
            LOG_ERROR("MYSQL, url, user, pass, basename, port error");
            exit(1);
        }
        connList.push_back(con);
        ++m_FreeConn;
    }

    reserve = sem(m_FreeConn);  // 传递了信号量的初始值

    m_MaxConn = m_FreeConn;
}

// 申请资源 开始使用
MYSQL *connection_pool::GetConnection()
{
    MYSQL *con = NULL;
    if(connList.size() == 0)
    {
        return NULL;
    }

    reserve.wait();
    lock.lock();
    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;
    lock.unlock();
    return con;
}
// 释放资源
bool connection_pool::ReleaseConnection(MYSQL *con)
{
    if(con == NULL)
    {
        return false;
    }
    lock.lock();

    // 重新放回连接池
    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();
    reserve.post();
    return true;
}


void connection_pool::DestroyPool()
{
    lock.lock();
    if(connList.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }
    lock.unlock();
}

int connection_pool::GetFreeConn()
{
    return this->m_FreeConn;
}

// 析构即全部销毁
connection_pool::~connection_pool()
{
    DestroyPool();
}

// RAII
// 获取即初始化一个conn
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *conn_pool)
{
    *SQL = conn_pool->GetConnection();
    conRAII = *SQL;
    poolRAII = conn_pool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}
