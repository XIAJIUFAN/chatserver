#ifndef PUBLIC_H
#define PUBLIC_H

/*
server和client额公共文件
*/
enum EnMsgType
{
    LOGIN_MSG = 1, // 登录消息
    LOGIN_MSG_ACK, // 登录响应消息
    LOGINOUT_MSG, // 注销消息
    REG_MSG,     // 注册消息
    REG_MSG_ACK, // 注册响应消息
    ONE_CHAT_MSG, // 聊天消息
    ADD_FRIEND_MSG, // 添加好友消息

    CREATE_GROUP_MSG, // 创建群组
    ADD_GROUP_MSG, // 加入群组
    GROUP_CHAT_MSG, // 群聊天
};

#endif
// {"msgid":1, "id":3, "password":" 123456"}
// {"msgid":1, "id":5, "password":" qwerty"}
// {"msgid":5, "id":4, "from":"hhh", "to":5, "msg":"hhh hello!"}
// {"msgid":5, "id":5, "from":"jjj", "to":4, "msg":"ok!"}
// {"msgid":5, "id":4, "from":"hhh", "to":5, "msg":"hello!"}

//{"msgid":6, "id":3, "friendid":4}