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
    static httpServer* Instance();

private:
    static httpServer* httpserver;

public:
    struct Client
    {
        
    };

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
    bool Get(const std::string& reqGetStr, std::function<bool(const std::string&, const uint16_t&, const httpHeader&)> Func);
    bool Post(const std::string& reqPostStr, std::function<bool(const std::string&, const uint16_t&, const httpHeader&)> Func);


private:
    void Run();
    void ConnectAccept(int conectSocketfd);
    void ConnectHandle(int connectSocketfd);
    void ClientAdd(int socketfd, std::string socketip, uint16_t socketport);
    void ClientDel(int socketfd);


private:
    long        ip;
    uint16_t    port;
    int         socket_fd;
    int         epoll_fd;

    std::unordered_map<std::string, std::function<bool(const std::string&, const uint16_t&, const httpHeader&)>> ReqFunc_Get;
    std::unordered_map<std::string, std::function<bool(const std::string&, const uint16_t&, const httpHeader&)>> ReqFunc_Post;

private:
    std::unordered_map<int, ClientInfo> mapClientInfo;
};


}


