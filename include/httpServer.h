#pragma once
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>

#include "httpHeader.h"

namespace YS
{


class httpServer
{
public:
    static httpServer* instance();

private:
    static httpServer* httpserver;

public:
    struct ClientInfo
    {
        std::string     ip;
        uint16_t        port;

        httpHeader      header;
    };

private:
    httpServer();
    ~httpServer();

    httpServer(const httpServer&) = delete;
    httpServer(httpServer&&) = delete;

public:
    void Start(const std::string& ip_port, const std::string& webPath = "");
    void Start(const std::string& ip, const std::string& port, const std::string& webPath = "");

public:
    bool Get(const std::string& reqGetStr, std::function<bool(std::string)> Func);

private:
    void Run();
    void ClientAdd(int socketfd, std::string socketip, uint16_t socketport);
    void ClientDel(int socketfd);


private:
    long        ip;
    uint16_t    port;
    int         socket_fd;
    int         epoll_fd;

    std::unordered_map<std::string, std::function<bool(std::string)>> ReqFunc_Get;
    std::unordered_map<std::string, std::function<bool(std::string)>> ReqFunc_Post;

private:
    std::unordered_map<int, ClientInfo> mapClientInfo;
};


}


