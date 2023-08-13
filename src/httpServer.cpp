#include "httpServer.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <unistd.h>
#include <cstddef>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <utility>


namespace Ys
{



httpServer* httpServer::m_httpserver = new httpServer();

httpServer::httpServer() 
    :m_ip(0), m_port(0), m_socket_fd(0), m_epoll_fd(0)
{}
httpServer::~httpServer() {}


httpServer* httpServer::instance()
{
    return m_httpserver;
}

// 启动
void httpServer::start(const std::string& ip_port, const std::string& webPath/*=""*/)
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
    return start(t_ip, t_port, webPath);
}

void httpServer::start(const std::string& ip, const std::string& port, const std::string& webPath)
{
    // 设置ip和port
    {
        m_ip = ::inet_addr(ip.c_str());
        m_port = ::htons(::atoi(port.c_str()));

        if (m_port == 0)
        { std::cout << "ip port Error\n"; return; }
    }

    // 初始化地址
    ::sockaddr_in t_socketAdd{};
    t_socketAdd.sin_family = AF_INET;
    t_socketAdd.sin_addr.s_addr = m_ip;
    t_socketAdd.sin_port = m_port;

    // 创建socket
    m_socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket_fd == 0)
    { std::cout << "socket Error!\n"; return; }

    // 设置端口复用
    int t_setsockoptvar{ 1 };
    int t_setsockoptvar2{ 1 };
    if (setsockopt(m_socket_fd, SOL_SOCKET, SO_REUSEADDR, &t_setsockoptvar, sizeof(t_setsockoptvar)) != 0
        || setsockopt(m_socket_fd, SOL_SOCKET, SO_REUSEPORT, &t_setsockoptvar2, sizeof(t_setsockoptvar2)) != 0)
    { std::cout << "setsockopt ADDR Or PORT Error!\n"; ::close(m_socket_fd); return; }

    // 绑定端口
    if (::bind(m_socket_fd, reinterpret_cast<::sockaddr*>(&t_socketAdd), sizeof(t_socketAdd)) != 0)
    { std::cout << "bind Error!\n"; ::close(m_socket_fd); return; }

    // 设置为被动
    if (::listen(m_socket_fd, 0) != 0)
    { std::cout << "listen Error!\n"; ::close(m_socket_fd); return; }

    // 创建epoll
    m_epoll_fd = ::epoll_create1(0);
    if (m_epoll_fd == 0)
    { std::cout << "epoll Error!\n"; ::close(m_socket_fd); return; }

    // 设置epoll监听连接请求
    epoll_event t_epenv{};
    t_epenv.events = EPOLLIN;
    t_epenv.data.fd = m_socket_fd;
    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_socket_fd, &t_epenv) != 0)
    { std::cout << "epoll_ctl Error!\n"; ::close(m_socket_fd); ::close(m_epoll_fd); return; }

    // 开启运行
    run();
}

void httpServer::run()
{
    // 创建epoll_wait返回用的内存
    std::vector<epoll_event> vecEpenv(1024);
    ::memset(vecEpenv.data(), 0, sizeof(epoll_event) * vecEpenv.size());

    // 用于接收连接时的临时变量
    int client_socket_fd = 0;
    ::sockaddr_in client_sockaddr_in{};
    ::socklen_t client_socklen{ sizeof(client_sockaddr_in) };
    std::string client_sockip;
    int client_sockport;

    epoll_event client_epent;
    client_epent.events = EPOLLIN | EPOLLET;

    std::cout << "---start---\n";
    // 监听等待
    for(size_t epWaitSize = 0; true; epWaitSize = 0)
    {
        epWaitSize = epoll_wait(m_epoll_fd, vecEpenv.data(), vecEpenv.size(), -1);
        for (int epWaitNum = 0; epWaitNum < epWaitSize; ++epWaitNum)
        {
            epoll_event& t_epenv{ vecEpenv[epWaitNum] };
            // 如果触发epoll的自己的套接字, 说明有人请求连接
            if (t_epenv.data.fd == m_socket_fd)
            {
                client_socket_fd = ::accept(m_socket_fd, reinterpret_cast<::sockaddr*>(&client_sockaddr_in), &client_socklen);
                if (client_socket_fd == 0) continue;

                // 同意连接, 开始监听
                client_sockip = ::inet_ntoa(client_sockaddr_in.sin_addr);
                client_sockport = ::ntohs(client_sockaddr_in.sin_port);
                m_map_socket_info[client_socket_fd] = std::make_pair(client_sockip, client_sockport);

                client_epent.data.fd = client_socket_fd;
                if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, client_socket_fd, &client_epent) != 0) std::cout << client_sockip << ":" << client_sockport <<" epoll 监听失败\n";
                else std::cout << client_sockip << ":" << client_sockport <<" 连上服务器\n";
            }
            else 
            {
                const auto& t_socket_info_iter{ m_map_socket_info.find(t_epenv.data.fd) };

                std::string readbuff(1024, '\0');
                int readNum = read(t_epenv.data.fd, readbuff.data(), readbuff.size());

                // 对方断开连接
                if (readNum < 1)
                {
                    if (t_socket_info_iter != m_map_socket_info.end()) 
                    {
                        std::cout << client_sockip << ":" << client_sockport <<" 和服务器断开连接\n";
                        m_map_socket_info.erase(t_socket_info_iter);
                    }
                    
                    epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, t_epenv.data.fd, nullptr);
                    continue;
                }

                // 接收对方消息
                if (t_socket_info_iter != m_map_socket_info.end())
                { std::cout << t_socket_info_iter->second.first << ":" << t_socket_info_iter->second.second << "\n\t"; }
                std::cout << "\n------------\n";
                for (; readNum > 0;)
                {
                    std::cout << readbuff;
                    write(t_epenv.data.fd, readbuff.data(), readNum);
                    readbuff.clear();
                    readNum = read(t_epenv.data.fd, readbuff.data(), readbuff.size());
                }
                std::cout << "\n------------\n\n";
            }
        }
    }
}



}
