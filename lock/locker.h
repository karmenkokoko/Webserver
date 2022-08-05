#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

/* 互斥锁 信号量 以及条件变量 实现对临界区的访问 */ 

class sem{
    public:
        sem()
        {
            if(sem_init(&m_sem, 0, 0) != 0)
            {
                throw std::exception();
            }
        }
        sem(int num)
        {
            if(sem_init(&m_sem, 0, num) != 0) // 传递了信号量的初始值
            {
                throw std::exception();
            }
        }
        ~sem()
        {
            sem_destroy(&m_sem);
        }

        bool wait()
        {
            // wait = -1  
            // 信号量不能小于0
            return sem_wait(&m_sem) == 0; 
        }

        bool post()
        {
            return sem_post(&m_sem) == 0;
        }

    private:
        sem_t m_sem;
};

class locker
{
    public:
        locker()
        {
            if(pthread_mutex_init(&m_mutex, NULL) != 0)
            {
                throw std::exception();
            }
        }
        ~locker()
        {
            pthread_mutex_destroy(&m_mutex);
        }
        bool lock()
        {
            return pthread_mutex_lock(&m_mutex) == 0;
        }
        bool unlock()
        {
            return pthread_mutex_unlock(&m_mutex) == 0;
        }
        pthread_mutex_t *get()
        {
            return &m_mutex;
        }
    private:
    // 互斥锁
        pthread_mutex_t m_mutex;
};

// 条件变量
// 条件变量通过允许线程阻塞和等待另一个线程发送信号的方法弥补了互斥锁的不足，
// 条件变量常和互斥锁一起使用。
// 使用时，条件变量被用来阻塞一个线程，当条件不满足时，线程往往解开相应的互斥锁并等待条件发生变化。
// 一旦其它的某个线程改变了条件变量，它将通知相应的条件变量唤醒一个或多个正被此条件变量阻塞的线程。
// 这些线程将重新锁定互斥锁并重新测试条件是否满足。
class cond
{
    public:
        cond()
        {
            if(pthread_cond_init(&m_cond, NULL) != 0)
            {
                // pthread_mutex_destroy(&m_mutex);
                throw std::exception();
            }
        }
        ~cond()
        {
            pthread_cond_destroy(&m_cond);
        }
        bool wait(pthread_mutex_t *m_mutex)
        {
            int ret = 0;
            // pthread_mutex_lock(&m_mutex);
            ret = pthread_cond_wait(&m_cond, m_mutex);
            return ret == 0;
        }
        bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
        {
            int ret = 0;
            //如果在 abstime指定的时间内 cond未触发，互斥量 mutex被重新加锁
            ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
            return ret == 0;
        }
        bool singal()
        {
            return pthread_cond_signal(&m_cond) == 0;
        }
        bool broadcast()
        {
            return pthread_cond_broadcast(&m_cond) == 0;
        }

    private:
        // static pthread_mutex_t m_mutex;
        pthread_cond_t m_cond;
};

#endif