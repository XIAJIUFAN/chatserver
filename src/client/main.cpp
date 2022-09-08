#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <functional>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 用于读写线程之间通信的信号量
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess(false); 

// 显示当前登录成功用户的基本信息
void showCurrentUserData();
// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间(聊天信息需要添加时间信息)
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);

// 聊天客户端实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char** argv)
{
    if (argc < 3) 
    {
        cerr << "command invalid example: ./ChatClient 127.0.0.1 8000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    /*
    考虑到客户端不需要去处理高并发，所以这里客户端tcp编程，我们就用最为基础的socket编程
    */

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd) 
    {
        cerr << "socket create error!" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port); // 主机字节序转网络字节序
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr*)& server, sizeof(sockaddr_in))) 
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    } 

    // 初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0); // (信号量地址, 线程通信/进程通信, 初始资源计数)

    // 连接服务器成功，就启动子线程
    std::thread readTask(readTaskHandler, clientfd); // (线程函数对象，线程函数对象参数)
    readTask.detach(); // 设置分离线程，线程运行完，资源自动回收

    // main线程用于接收用户输入，负责发送数据(发送线程)
    for (;;) 
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "========================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "========================" << endl;
        cout << "choice:";
        int choice = 0;
        // 无论c/c++，输入一个整数，然后输入一个字符串，都先get一下读取掉回车
        // 否则直接读取回车作为字符串了
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice) 
        {
        case 1: // login业务
        {
            int userid;
            char pwd[50] = { 0 };
            cout << "userid:";
            cin >> userid;
            cin.get();
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = userid;
            js["password"] = pwd;
            string request = js.dump();

            g_isLoginSuccess = false;

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (-1 == len) 
            {
                cerr << "send login msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待信号量，由于线程处理完后登录的响应消息后，通知这里，唤醒主线程
            
            if (g_isLoginSuccess) // 登陆成功
            {
                // 进入聊天主菜单页面
                isMainMenuRunning = true;
                mainMenu(clientfd);
            }
        }
        break;
        case 2: // register业务
        {
            char name[50] = { 0 };
            char pwd[50] = { 0 };
            cout << "username:";
            cin.getline(name, 50); // cin >> 这样的输入方式，遇到空格或非法字符就结束， getline遇到回车结束   
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump(); // 序列化

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (-1 == len)
            {
                cerr << "send reg msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待信号量，由于线程处理完后注册的响应消息后，通知这里，唤醒主线程
        }
        break;
        case 3: // quit业务
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }
    return 0;
}

// 处理登录响应业务逻辑
void doLoginResponse(json& response)
{
    if (0 != response["errno"].get<int>()) // 登录失败
    {
        cerr << response["errmsg"] << endl;
        g_isLoginSuccess = false;
    } 
    else // 登录成功 
    {
        // 记录当前用户的id和name
        g_currentUser.setId(response["id"].get<int>());
        g_currentUser.setName(response["name"]);
        
        g_currentUserFriendList.clear();
        g_currentUserGroupList.clear();
        
        // 记录当前用户的好友列表信息
        if (response.contains("friends"))
        {
            vector<string> friendVec = response["friends"];
            for (string& friendStr : friendVec) 
            {
                json js = json::parse(friendStr);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                
                g_currentUserFriendList.push_back(user);
            }
        }

        // 记录当前用户的群组列表信息
        if (response.contains("groups")) 
        {
            vector<string> groupVec = response["groups"];
            for (string& groupStr : groupVec) 
            {
                json groupjs = json::parse(groupStr);
                Group group;
                group.setId(groupjs["id"].get<int>());
                group.setName(groupjs["groupname"]);
                group.setDesc(groupjs["groupdesc"]);

                vector<string> userVec = groupjs["users"];
                for (string& userStr : userVec)
                {
                    json js = json::parse(userStr);
                    GroupUser user;
                    user.setId(js["id"].get<int>());
                    user.setName(js["name"]);
                    user.setState(js["state"]);
                    user.setRole(js["role"]);
                    group.getUsers().push_back(user);
                }
                g_currentUserGroupList.push_back(group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线消息(个人聊天消息或者群组消息)
        if (response.contains("offlinemsg"))
        {
            vector<string> vec = response["offlinemsg"];
            for (string& str : vec) 
            {
                json js = json::parse(str);
                int msgType = js["msgid"].get<int>();
                if (ONE_CHAT_MSG == msgType) // 个人聊天消息 
                {
                    cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                        << " said: " << js["msg"].get<string>() << endl;     
                }

                if (GROUP_CHAT_MSG == msgType) // 群聊消息
                {
                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() 
                        << " [" << js["id"] << "]" << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
                }
            }
        }

        g_isLoginSuccess = true;
    }  
}

// 处理注册响应业务逻辑
void doRegResponse(json& response)
{
    if (0 != response["errno"].get<int>()) // 注册失败
    {
        cerr << "name is already exist, register error!" << endl;
    }
    else 
    {
        cout << "register success, userid is " << response["id"]
            << " , do not forget it!" << endl;
    }
}

// 接收线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[1024] = { 0 };
        int len = recv(clientfd, buffer, 1024, 0); // 接收服务器端的response信息
        if (-1 == len || 0 == len) 
        {
            close(clientfd);
            exit(-1);
        }

        // 接收ChatServer转发的数据,反序列化生成json数据对象
        json js = json::parse(buffer);
        int msgType = js["msgid"].get<int>();

        if (LOGIN_MSG_ACK == msgType) 
        {
            // 处理登录响应的业务逻辑
            doLoginResponse(js);
            sem_post(&rwsem); // 通知主线程,登录响应处理完成
            continue;
        }

        if (REG_MSG_ACK == msgType) 
        {
            doRegResponse(js); // 处理注册响应的业务逻辑
            sem_post(&rwsem); // 通知主线程，注册响应处理完成
            continue;
        }

        if (ONE_CHAT_MSG == msgType) // 个人聊天消息 
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                << " said: " << js["msg"].get<string>() << endl;
            continue;     
        }
        if (GROUP_CHAT_MSG == msgType) // 群聊消息
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() 
                << " [" << js["id"] << "]" << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "======================login user======================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "----------------------friend list---------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------group list----------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout <<"    "<< user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}

// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}


// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);

// 系统支持的客户端(登录后)命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"}};

// 注册系统支持的客户端命令处理
// (int, string)-->(clientfd, js.dump())
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();

    while (isMainMenuRunning)
    {
        char buffer[1024] = { 0 };
        cin.getline(buffer, 1024);
        string commandBuf(buffer);
        string command; // 存储命令
        int idx = commandBuf.find(":");
        if (-1 == idx) 
        {
            command = commandBuf;
        } 
        else 
        {
            command = commandBuf.substr(0, idx);
        }

        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end()) 
        {
            cerr << "invalid input command!" << endl;
            continue;
        }
        // 调用相应命令的处理事件,mainMenu对修改封闭,添加新功能不需要修改该函数
        it->second(clientfd, commandBuf.substr(idx + 1, commandBuf.size() - idx)); // 调用命令处理方法
    }
}

// "help" command handler
void help(int fd, string str)
{
    cout << "show command list >>> " << endl;
    for (auto& p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }

    cout << endl;
}

// "chat" command handler
void chat(int clientfd, string str)
{
    int idx = str.find(":");
    if (idx == -1) 
    {
        cerr << "chat command invalid!" << endl;
        return;
    }
 
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId(); // 发送方id
    js["name"] = g_currentUser.getName(); // 发送方name
    js["to"] = friendid; // 接收方id
    js["msg"] = message;
    js["time"] = getCurrentTime();
    
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1,  0);
    if (-1 == len) 
    {
        cerr << "send addfriend msg error -> " << buffer << endl;
    } 
}

// "addfriend" command handler
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1,  0);
    if (-1 == len) 
    {
        cerr << "send addfriend msg error -> " << buffer << endl;
    } 
}

// "creategroup" command handler
void creategroup(int clientfd, string str)
{
    // groupname:groupdesc
    int idx = str.find(":");
    if (-1 == idx) 
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);
    
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId(); // 创建者id
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();
    
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error -> " << buffer << endl;
    } 

}

// "addgroup" command handler
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }     
}

// "groupchat" command handler
void groupchat(int clientfd, string str)
{
    // groupid:message
    int idx = str.find(":");
    if (-1 == idx) 
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["time"] = getCurrentTime();
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName(); 
    js["groupid"] = groupid;
    js["msg"] = message;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error -> " << buffer << endl;
    }   
}

// "loginout" command handler
void loginout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send logout msg error -> " << buffer << endl;
    } 
    else
    {
        isMainMenuRunning = false;
    } 
}