#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

class Log{
    public:
        static Log *get_instance()
        {
            static Log instance;
            return &instance;
        }
    
        static void *flush_log_thread(void *args)
        {
            Log::get_instance()->async_write_log();
        }

        // 可选择参数， 文件， 缓冲大小， 最大行数， 最长日志条队列
        bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

        void write_log(int level, const char *format, ...);

        void flush(void);

    private:
        Log();
        virtual ~Log();
        void async_write_log()
        {
            string single_log;
            // 从阻塞队列中取string 写入
            while(m_log_queue->pop(single_log))
            {
                m_mutex.lock();
                fputs(single_log.c_str(), m_fp);
                m_mutex.unlock();
            }
        }
    
    private:
        char dir_name[128];
        char log_name[128];
        int my_split_lines;
        int m_log_buf_size;
        long long m_count; // 日志行数记录
        int m_today;       // 记录时间
        char *m_buf;
        locker m_mutex;
        FILE *m_fp;
        block_queue<string> *m_log_queue;

        bool m_is_async; // 是否同步标志位
        int m_close_log;
};

#define LOG_DEBUG(format, ...)                                    \
    if (m_close_log == 0)                                         \
    {                                                             \
        Log::get_instance()->write_log(0, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }

#define LOG_INFO(format, ...)                                     \
    if (m_close_log == 0)                                         \
    {                                                             \
        Log::get_instance()->write_log(1, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }

#define LOG_WARN(format, ...)                                     \
    if (m_close_log == 0)                                         \
    {                                                             \
        Log::get_instance()->write_log(2, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }

#define LOG_ERROR(format, ...)                                    \
    if (m_close_log == 0)                                         \
    {                                                             \
        Log::get_instance()->write_log(3, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#endif