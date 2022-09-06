#include "chatservice.hpp"
#include "public.hpp"

#include <string>
#include <vector>
#include <muduo/base/Logging.h>
using namespace std;
using namespace muduo;

// 获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return & service;
}

// 构造函数私有化
ChatService::ChatService()
{// 注册消息以及对应的handler回调操作

    // 用户基本业务管理相关事件处理回调注册
    msgHandlerMap_.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    msgHandlerMap_.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    msgHandlerMap_.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    msgHandlerMap_.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    msgHandlerMap_.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    
    // 群组业务管理相关事件处理回调注册
    msgHandlerMap_.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    msgHandlerMap_.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    msgHandlerMap_.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    if (redis_.connect())
    {
        // s设置上报消息的回调
        redis_.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 服务器异常,业务重置方法
void ChatService::reset()
{
    // 把online状态的用户重置为offline状态
    userModel_.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    auto it = msgHandlerMap_.find(msgid);
    if (it == msgHandlerMap_.end()) 
    {
       // 返回一个默认的处理器，空操作
       return [=](const TcpConnectionPtr& conn, json& js, Timestamp time){
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
       };
    } 
    else
    {
        return msgHandlerMap_[msgid];
    }
     
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"].get<string>();

    User user = userModel_.query(id);
    if (user.getId() == id && user.getPwd() == pwd) 
    {
        if (user.getState() == "online") 
        {
            // 用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "该账号已经登录，请重新输入新账号!";
            conn->send(response.dump());

        } 
        else
        {
            // 登录成功,记录用户连接信息
            {
                lock_guard<mutex> lock(ConnMutex_);
                userConnMap_.insert({id, conn});
            }

            // id用户登录成功后，向reids订阅channel
            redis_.subscribe(id);

            // 登录成功
            user.setState("online"); // 更新用户状态
            userModel_.update(user); // 数据库的并发操作由Mysql Server保证

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询用户是否有离线消息
            vector<string> vec = offlineMsgModel_.query(id);
            if (!vec.empty()) 
            {
                // 获取该用户的离线消息
                response["offlinemsg"] = vec;
                // 读取用户的离线消息后,把该用户的所有离线消息删除
                offlineMsgModel_.remove(id);
            }

            // 查询该用户的好友信息并返回给用户
            vector<User> userVec = friendModel_.query(id);

            if (!userVec.empty())
            {
                vector<string> vec2;
                for (auto& user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();

                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            

            conn->send(response.dump());
        }
    }
    else 
    {
        // 登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户不存在，登录失败!";
        conn->send(response.dump());
    }
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(ConnMutex_);
        auto it = userConnMap_.find(userid);
        if (it != userConnMap_.end()) 
        {
            userConnMap_.erase(it);
        }
    }

    // 用户注销，相当于下线，在redis中取消订阅通道
    redis_.unsubscribe(userid);

    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    userModel_.update(user); // 更新的是状态信息
}

// 处理注册业务
void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    string name = js["name"].get<string>();
    string pwd = js["password"].get<string>();

    User user;
    // 业务操作的都是数据对象
    user.setName(name);
    user.setPwd(pwd);

    bool state = userModel_.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "注册失败!";
        conn->send(response.dump());
    }
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{   
    User user;
    
    {
        lock_guard<mutex> lock(ConnMutex_);
        // 客户端异常退出，需要删除用户连接信息
        for (auto it = userConnMap_.begin(); it != userConnMap_.end(); ++it)
        {
            if (it->second == conn) 
            {
                user.setId(it->first);
                userConnMap_.erase(it);
                break;
            }
        }
    }

    // 客户端异常退出，相当于用户下线,在redis中取消订阅通道
    redis_.unsubscribe(user.getId());

    // 更新用户在数据库中的状态信息 online->offline
    if (user.getId() != -1) // 确保是一个有效的用户
    {
        user.setState("offline");
        userModel_.update(user);
    }
    
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    int toid = js["to"].get<int>(); // 获取对方id

    {
        lock_guard<mutex> lock(ConnMutex_);
        auto it = userConnMap_.find(toid);
        if (it != userConnMap_.end())
        {
            // toid和id在用一台服务器上登录，且toid在线，转发消息
            // 转发消息应该也要在锁的范围内，避免发送消息的时候，toid下线了

            // 服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    // 查询toid是否在线
    User user = userModel_.query(toid);
    if (user.getState() == "online") 
    {
        // toid在线，在另外一台服务器上登录
        // 在redis消息队列中订阅了toid通道
        redis_.publish(toid, js.dump());
        return; 
    }

    // toid不在线，存储离线消息
    offlineMsgModel_.insert(toid, js.dump());
}

// 添加好友业务
void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    friendModel_.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    Group group(-1, name, desc);
    if (groupModel_.createGroup(group)) 
    {
        // 创建群组成功后，再去存储群组创建人信息
        groupModel_.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    groupModel_.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    vector<int> userVec = groupModel_.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(ConnMutex_);
    for (int id : userVec) 
    {
             
        auto it = userConnMap_.find(id);
        if (it != userConnMap_.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else 
        {
            // 查询id是否在线
            User user = userModel_.query(id);
            if (user.getState() == "online")
            {// id在其他服务上登录
                redis_.publish(id, js.dump());
            } 
            else
            {
                // 存储离线群消息
                offlineMsgModel_.insert(id, js.dump());
            }

        }
    }
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{

    lock_guard<mutex> lock(ConnMutex_);
    auto it = userConnMap_.find(userid);
    if (it != userConnMap_.end()) 
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    offlineMsgModel_.insert(userid, msg);
}