# chatserver
可以工作在nginx tcp负载均衡环境中的集群聊天服务器和客户端源码(基于muduo库实现)

### 技术栈
- c++11
- muduo网络库
- 基于发布-订阅的redis消息队列

### 编译方式
```bash
cd build
rm -rf *
cmake ..
make
```
