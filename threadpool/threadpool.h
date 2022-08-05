#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template<typename T>
class threadpool
{
    public:
        threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
        ~threadpool();
        bool append(T *request, int state);
        bool append_p(T *request);
    
    private:
        // 工作线程运行函数， 取队头来执行任务
        static void *worker(void *arg);
        void run();
    private:
        int m_thread_number;     //线程池的线程数
        int m_max_requests;      //队列允许的最大长度
        pthread_t *m_threads;    //线程池数组
        std::list<T *> m_workqueue; // 请求队列
        locker m_queuelocker;    // 请求队列的互斥锁
        sem m_queuestat;         // 是否有任务需要处理
        connection_pool *m_connPool; // 数据库
        int m_actor_model;      // proactor reactor
};

template<typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests)
{
    if(thread_number <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }
    // 开提个thread number数量的线程池
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {
        if(pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])) // 线程分离的状态，0是成功，非0是失败
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

// 队尾加入conn请求等
template<typename T>
bool threadpool<T>::append(T *request, int state)
{
    m_queuelocker.lock();
    // 大于队列的最大值就停止加入
    if(m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request); // 加入队尾
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

// 队尾加入conn请求等, 未知state
template<typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    // 大于队列的最大值就停止加入
    if(m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request); // 加入队尾
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while(true)
    {
        // 信号量 大于等于0， 小于0进入阻塞状态
        // wait -1
        // post +1
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request)
            continue;
        
        if(m_actor_model == 1) // 0是proactor(工作线程处理逻辑), 1是reactor
        {
            if(request->m_state == 0)
            {
                // sql connection 操作
                if(request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else{
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else{
            // proactor模式
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}

#endif