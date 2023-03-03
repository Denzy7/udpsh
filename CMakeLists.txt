cmake_minimum_required(VERSION 3.5)

project(udpsh LANGUAGES C)
add_compile_options(-Wall)
add_library(udpsh-sock udpsh_sock.h  udpsh_sock.c)
add_executable(udpsh-client client.c)
add_executable(udpsh-server server.c)
target_link_libraries(udpsh-client udpsh-sock)
target_link_libraries(udpsh-server udpsh-sock)
