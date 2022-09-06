cmake_minimum_required(VERSION 2.6)
project(chat)

# 配置编译选项
# 设置调试信息,以及启动C++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

# 配置最终的可执行文件的输出路径
set(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin)
 

# 配置头文件的搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/server)
include_directories(${PROJECT_SOURCE_DIR}/include/server/db)
include_directories(${PROJECT_SOURCE_DIR}/include/server/model)
include_directories(${PROJECT_SOURCE_DIR}/include/server/redis)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)
 
# 加载子目录
add_subdirectory(src)

