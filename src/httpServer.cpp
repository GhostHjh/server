#include "httpServer.h"
#include "httpHeader.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>


namespace YS
{



httpServer* httpServer::httpserver = new httpServer();

httpServer::httpServer() 
    :ip(0), port(0), socket_fd(0), epoll_fd(0)
{}
httpServer::~httpServer() {}


httpServer* httpServer::instance()
{
    return httpserver;
}

// 启动
void httpServer::Start(const std::string& ip_port, const std::string& webPath/*=""*/)
{
    // 读取ip和端口
    size_t t_index{ ip_port.find(':') };
   if (t_index == std::string::npos || t_index == ip_port.size() -1)
    {
        t_index = ip_port.find(' ');
        if (t_index == std::string::npos || t_index == ip_port.size() -1)
        { std::cout << "ip port Error\n"; return; }
    }

    const std::string t_ip{ ip_port.substr(0, t_index) };
    ++t_index;
    const std::string t_port{ ip_port.substr(t_index, ip_port.size() -t_index) };
    return Start(t_ip, t_port, webPath);
}

void httpServer::Start(const std::string& ip_, const std::string& port_, const std::string& webPath_)
{
    // 设置ip和port
    {
        ip = ::inet_addr(ip_.c_str());
        port = ::htons(::atoi(port_.c_str()));

        if (port == 0)
        { std::cout << "ip port Error\n"; return; }
    }

    // 初始化地址
    ::sockaddr_in t_socketAdd{};
    t_socketAdd.sin_family = AF_INET;
    t_socketAdd.sin_addr.s_addr = ip;
    t_socketAdd.sin_port = port;

    // 创建socket
    socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == 0)
    { std::cout << "socket Error!\n"; return; }

    // 设置端口复用
    int t_setsockoptvar{ 1 };
    int t_setsockoptvar2{ 1 };
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &t_setsockoptvar, sizeof(t_setsockoptvar)) != 0
        || setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &t_setsockoptvar2, sizeof(t_setsockoptvar2)) != 0)
    { std::cout << "setsockopt ADDR Or PORT Error!\n"; ::close(socket_fd); return; }

    // 绑定端口
    if (::bind(socket_fd, reinterpret_cast<::sockaddr*>(&t_socketAdd), sizeof(t_socketAdd)) != 0)
    { std::cout << "bind Error!\n"; ::close(socket_fd); return; }

    // 设置为被动
    if (::listen(socket_fd, 0) != 0)
    { std::cout << "listen Error!\n"; ::close(socket_fd); return; }

    // 创建epoll
    epoll_fd = ::epoll_create1(0);
    if (epoll_fd == 0)
    { std::cout << "epoll Error!\n"; ::close(socket_fd); return; }

    // 设置epoll监听连接请求
    epoll_event t_epenv{};
    t_epenv.events = EPOLLIN;
    t_epenv.data.fd = socket_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &t_epenv) != 0)
    { std::cout << "epoll_ctl Error!\n"; ::close(socket_fd); ::close(epoll_fd); return; }

    // 开启运行
    Run();
}

void httpServer::Run()
{
    // 创建epoll_wait返回用的内存
    std::vector<epoll_event> vecEpenv(1024);

    // 用于接收连接时的临时变量
    int client_socket_fd = 0;
    ::sockaddr_in client_sockaddr_in{};
    ::socklen_t client_socklen{ sizeof(client_sockaddr_in) };

    std::cout << "---start---\n";
    // 监听等待
    for(size_t epWaitSize = 0; true; epWaitSize = 0)
    {
        epWaitSize = epoll_wait(epoll_fd, vecEpenv.data(), vecEpenv.size(), -1);
        for (int epWaitNum = 0; epWaitNum < epWaitSize; ++epWaitNum)
        {
            epoll_event& t_epenv{ vecEpenv[epWaitNum] };
            // 如果触发epoll的自己的套接字, 说明有人请求连接
            if (t_epenv.data.fd == socket_fd)
            {
                client_socket_fd = ::accept(socket_fd, reinterpret_cast<::sockaddr*>(&client_sockaddr_in), &client_socklen);
                if (client_socket_fd == 0) continue;

                // 同意连接, 开始监听
                ClientAdd(client_socket_fd, ::inet_ntoa(client_sockaddr_in.sin_addr), ::ntohs(client_sockaddr_in.sin_port));
            }
            else 
            {
                std::string readbuff(1024, '\0');
                int readNum = read(t_epenv.data.fd, readbuff.data(), readbuff.size());

                // 对方断开连接
                if (readNum < 1)
                {
                    ClientDel(t_epenv.data.fd);
                    continue;
                }

                auto t_Client_iter{ mapClientInfo.find(t_epenv.data.fd) };
                httpHeader& header {t_Client_iter->second.header};

                // 接收对方消息
                for (header.Append(readbuff.c_str()); readNum > 0 && header.Status() == httpHeader::HeaderStatus::AnalysisCompleteNo; header.Append(readbuff.c_str()))
                {
                    readbuff.clear();
                    readNum = read(t_epenv.data.fd, readbuff.data(), readbuff.size());
                }

                if (header.Status() == httpHeader::HeaderStatus::HeaderFormatError)
                {
                    ClientDel(t_epenv.data.fd);
                    std::cout << "header 格式有误\n";
                    continue;
                }
                else if (header.Status() == httpHeader::HeaderStatus::AnalysisCompleteOK)
                {
                    ClientDel(t_epenv.data.fd);
                    std::cout << "header 格式完整\n";
                    continue;
                }
            }
        }
    }
}

void httpServer::ClientAdd(int socketfd, std::string socketip, uint16_t socketport)
{
    const auto& t_Client_Iter{ mapClientInfo.emplace(socketfd, ClientInfo{std::move(socketip), socketport, httpHeader{}}).first };
    
    epoll_event client_epent;
    client_epent.events = EPOLLIN | EPOLLET;
    client_epent.data.fd = socketfd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socketfd, &client_epent) != 0) 
        std::cout << t_Client_Iter->second.ip << ":" << t_Client_Iter->second.port <<" epoll 监听失败\n";
    else
        std::cout << t_Client_Iter->second.ip << ":" << t_Client_Iter->second.port <<" 连上服务器\n";
}

void httpServer::ClientDel(int socketfd)
{
    const auto& t_Client_iter{ mapClientInfo.find(socketfd) };
    if (t_Client_iter != mapClientInfo.end()) 
    {
        std::cout << t_Client_iter->second.ip << ":" << t_Client_iter->second.port <<" 和服务器断开连接\n";
        mapClientInfo.erase(t_Client_iter);
    }
    
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, socketfd, nullptr);
}



}
