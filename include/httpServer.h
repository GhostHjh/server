#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>

namespace Ys
{


class httpServer
{
public:
    static httpServer* instance();

private:
    static httpServer* m_httpserver;

private:
    httpServer();
    ~httpServer();

    httpServer(const httpServer&) = delete;
    httpServer(httpServer&&) = delete;

public:
    void start(const std::string& ip_port, const std::string& webPath = "");
    void start(const std::string& ip, const std::string& port, const std::string& webPath = "");

public:
    bool Get(const std::string& reqGetStr, std::function<bool(std::string)> Func);

private:
    void run();

private:
    long        m_ip;
    uint16_t    m_port;
    int         m_socket_fd;
    int         m_epoll_fd;

    std::unordered_map<std::string, std::function<bool(std::string)>> m_ReqFunc_Get;
    std::unordered_map<std::string, std::function<bool(std::string)>> m_ReqFunc_Post;

private:
    std::unordered_map<int, std::pair<std::string, int>> m_map_socket_info;
};


}


