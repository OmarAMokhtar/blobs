cmake_minimum_required(VERSION 3.4.1)

# now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

add_library(gl2jni SHARED
            gl_code.cpp
            3ds.cpp
            texture.cpp)

# add lib dependencies
target_link_libraries(gl2jni
                      android
                      log 
                      EGL
                      GLESv2)

include_directories(/home/omar/Android/Sdk/ndk-bundle/sources/third_party/vulkan/src/libs)
