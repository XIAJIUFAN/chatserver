#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>
using namespace std;
using namespace muduo;
using namespace muduo::net;

#include <json.hpp>
using json = nlohmann::json;

#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"

#include "redis.hpp"

using MsgHandler = std::function<void(const TcpConnectionPtr&, json&, Timestamp)>;

// 单例模式实现服务器聊天业务类型
class ChatService
{
public:
    // 获取单例对象的接口函数
    static ChatService* instance();

    // 处理登录业务
    void login(const TcpConnectionPtr& conn, json& js, Timestamp time);

    // 处理注销业务
    void loginout(const TcpConnectionPtr& conn, json& js, Timestamp time);

    // 处理注册业务
    void reg(const TcpConnectionPtr& conn, json& js, Timestamp time);

    // 服务器异常,业务重置方法
    void reset();

    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr& conn);

    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time);

    // 添加好友业务
    void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time);

    // 创建群组业务
    void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);

    // 加入群组业务
    void addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);

    // 群组聊天业务
    void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time);

    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    
    // 从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int ,string);
    
private:
    // 构造函数私有化
    ChatService();
    unordered_map<int, MsgHandler> msgHandlerMap_; // 存储消息id和其对应的业务处理方法
    
    // 数据操作类对象==>给业务层提供的都是对象
    UserModel userModel_;
    OfflineMsgModel offlineMsgModel_;
    FriendModel friendModel_;
    GroupModel groupModel_;

    // 存储在线用户的通信连接
    // 连接会随着用户的上线和下线所改变，需要考虑线程安全问题
    unordered_map<int, TcpConnectionPtr> userConnMap_;

    // 定义互斥锁，保证userConnMap_对象的线程安全
    mutex ConnMutex_;

    // redis操作对象
    Redis redis_;
};

#endif