#include "json.hpp"
using json = nlohmann::json;

#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;

// json序列化示例1
string func1() 
{
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "hello, what are you doing now?";
    string sendBuf = js.dump(); // json数据对象 ==》序列化为json字符串
    return sendBuf;
}

// json序列化示例2
void func2()
{
    json js;
    js["id"] = {1,2,3,4,5,6};
    js["from"] = "zhang san";
    js["msg"]["zhang san"] = "hello world";
    js["msg"]["li si"] = "hello world";
    // 上面等同于
    js["msg"] = {{"zhang san", "hello world"}, {"li si", "hello world"}};
    cout << js << endl;
}

// json序列化示例3
string func3()
{
    json js;

    // 序列化vector容器
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    js["list"] = vec;

    // 序列化map容器
    map<int,string> m;
    m.insert({1,"哈哈"});
    m.insert({2,"呵呵"});
    m.insert({3,"等等"});
    js["path"] = m;
    js["from"] = "zhang san";
     
    string sendBuf = js.dump();
    return sendBuf;
}

int main()
{
    string recvBuf = func1();
    json jsbuf = json::parse(recvBuf);
    cout << jsbuf["msg_type"] << endl;
    cout << jsbuf["from"] << endl;
    cout << jsbuf["to"] << endl;

    recvBuf = func3();
    jsbuf = json::parse(recvBuf);
    vector<int> vec = jsbuf["list"]; // js对象里面的数据类型，直接放入vector容器中
    for (auto& v: vec)
    {
        cout << v << " ";
    }
    cout << endl;

    map <int, string> mymap = jsbuf["path"];
    for (auto& p : mymap) 
    {
        cout << p.first << " " << p.second << " ";
    }
    cout << endl;
    return 0;
}