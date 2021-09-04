# CMakeList.txt: demo 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

project ("demo")

# 将源代码添加到此项目的可执行文件。
add_executable(
demo
"main.cpp" 
"thread/global.h"
"thread/utils.h" 
"thread/utils.cpp"
"thread/WKThread.h"
"thread/WKThread.cpp"
"thread/WKThreadPool.h"
"thread/WKThreadPool.cpp"
"thread/WKThreadTask.h"
"thread/WKLocker.h" 
"thread/WKLocker.cpp")

# TODO: 如有需要，请添加测试并安装目标。
