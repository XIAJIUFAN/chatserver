#ifndef GROUPUSERMODEL_H
#define GROUPUSERMODEL_H

#include <string>
using namespace std;

#include "user.hpp"

 
// 群组用户
// 继承User类，复制User的其他信息，多了一个角色信息
class GroupUser : public User
{
public:
    
    void setRole(string role) { this->role = role; }
    string getRole() { return this->role; }

private:
    string role; // 组内成员角色
};

#endif