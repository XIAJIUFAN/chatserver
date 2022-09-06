#include "chatserver.hpp"
#include "chatservice.hpp"

#include <functional>

// 初始化聊天服务器对象
ChatServer::ChatServer(EventLoop* loop,
        const InetAddress& listenAddr,
        const std::string& nameArg)
    : loop_(loop),
      server_(loop, listenAddr, nameArg)
{
    // 注册连接回调
    server_.setConnectionCallback(
        std::bind(&ChatServer::onConnection, this, _1)
    );

    // 注册消息回调
    server_.setMessageCallback(
        std::bind(&ChatServer::onMessgae, this, _1, _2, _3));

    // 设置线程数量
    server_.setThreadNum(4);
}

// 启动服务
void ChatServer::start()
{
    server_.start();
}

// 连接建立以及连接断开的回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    // 客户端断开连接
    if (!conn->connected()) 
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }    
}

// 上报读写事件相关的回调函数
void ChatServer::onMessgae(const TcpConnectionPtr& conn,
                Buffer * buffer,
                Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    // 数据反序列化
    json js = json::parse(buf);
    // 目的：完全解耦网络模块和业务模块的代码
    // 通过js["msgid"].get<int>()来获取消息id所对应的业务处理器(handler)
    auto msghandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    // 利用获取的事件处理器来执行相应的业务处理
    msghandler(conn, js, time);
}