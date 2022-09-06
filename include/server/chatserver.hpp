#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
using namespace muduo;
using namespace muduo::net;

#include <iostream>
#include <string>

class ChatServer
{
public:
    // 初始化聊天服务器对象
    ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const std::string& nameArg);
    
    // 启动服务
    void start();

private:
    // 连接建立以及连接断开的回调函数
    void onConnection(const TcpConnectionPtr&);

    // 读写事件相关的回调函数
    void onMessgae(const TcpConnectionPtr&,
                    Buffer *,
                    Timestamp);

    TcpServer server_; // 组合的muduo库中实现服务器功能的类对象
    EventLoop* loop_; // 指向事件循环对象的指针
};

#endif