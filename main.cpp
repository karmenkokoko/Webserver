#include "config.h"

int main(int argc, char *argv[])
{
    // 数据库信息
    string user= "cshi";
    string passwd = "S159357zdqmlo!";
    string databasename = "webserverdb";

    // 命令行解析参数
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    // server 初始化
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite,
                config.OPT_LINGER, config.TRIGMode, config.sql_num, config.thread_num,
                config.close_log, config.actor_model);
    
    // 写日志
    server.log_write();

    // 数据库
    server.sql_pool();
    
    // 线程池
    server.thread_pool();

    // 触发模式 ET, LT  边缘触发和条件触发
    server.trig_mode();

    // 监听socket活动
    server.eventListen();

    // 运行
    server.eventLoop();

    return 0;
}