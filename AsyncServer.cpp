//
// Created by roach on 19.11.2025.
//

#include "AsyncServer.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
EPollManager::EPollManager() {
    std::cout << "EPollManager::EPollManager" << std::endl;
    m_epoll_fd = epoll_create1(0);
    if (m_epoll_fd == -1) {
        throw std::system_error(errno, std::system_category(),
            "epoll_create1 failed");
    }
}
EPollManager::~EPollManager() {
    if (m_epoll_fd != -1) {
        ::close(m_epoll_fd);
    }
}
void EPollManager::addFD(int fd, uint32_t events, void *userData) {
    std::cout << "EPollManager::addFD" << std::endl;
    epoll_event event{0};
    event.events = events;
    event.data.ptr = userData;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        throw std::system_error(errno, std::system_category(),
            "epoll_ctl ADD failed");
    }
}
void EPollManager::modifyFD(int fd, uint32_t events, void *userData) {
    std::cout << "EPollManager::modifyFD" << std::endl;
    if (m_epoll_fd == -1) {
        throw std::system_error(EBADF, std::system_category(),
            "\tEPoll instance is invalid");
    }

    struct epoll_event event{0};
    event.events = events;
    event.data.ptr = userData;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) {
        throw std::system_error(errno, std::system_category(),
            "\tepoll_ctl MODIFY for fd " + std::to_string(fd));
    }

}
void EPollManager::removeFD(int fd) {
    std::cout << "EPollManager::removeFD" << std::endl;
    if (m_epoll_fd == -1) return;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1 && errno != ENOENT) {
        throw std::system_error(errno, std::system_category(),
            "\tepoll_ctl DEL failed for fd" + std::to_string(fd));
    }
}


int EPollManager::waitForEvents(epoll_event *events, int maxEvents, int timeout) {
    std::cout << "EPollManager::waitForEvents" << std::endl;
    if (m_epoll_fd == -1) {
        throw std::system_error(EBADF, std::system_category(),"EPoll instance is not initialized");
    }
    if (maxEvents <= 0) {
        throw std::invalid_argument("maxEvents must be positive");
    }

    // std::cout << "EPollManager::waitForEvents - timeout: " << timeout << "ms" << std::endl;

    int count = epoll_wait(m_epoll_fd, events, maxEvents, timeout);
    if (count == -1 && errno != EINTR) {
        throw std::system_error(errno, std::system_category(),"epoll_wait failed");
    }
    return (count == -1) ? 0 : count;
}
UDPServer::~UDPServer() {
    std::cout << "UDPServer::~UDPServer" << std::endl;
    stop();
}
bool UDPServer::start(std::string &ip, int port, EPollManager *epollManager) {
    std::cout << "UDPServer::start" << std::endl;

    if (m_running) {
        std::cout << "UDPServer already running" << std::endl;
        return false;
    }

    m_epollManager = epollManager;

    m_server_fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (m_server_fd == -1) {
        std::cerr << "UDP socket failed" << strerror(errno) << std::endl;
        return false;
    }

    int opt = 1;
    if (::setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "UDP setsockopt(SO_REUSEADDR) failed: " << strerror(errno) << std::endl;
        ::close(m_server_fd);
        return false;
    }
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (ip == "0.0.0.0") {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) == -1) {
            std::cerr << "UDP invalid IP address: " << ip << std::endl;
            ::close(m_server_fd);
            return false;
        }
    }

    if (::bind(m_server_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        std::cerr << "UDP bind () failed: " << strerror(errno) << std::endl;
        return false;
    }

    try {
        m_epollManager->addFD(m_server_fd, EPOLLIN | EPOLLET, this);
    } catch (const std::exception& e) {
        std::cerr << "UDP epoll add failed: " << e.what() << std::endl;
        ::close(m_server_fd);
        return false;
    }

    m_running = true;
    std::cout << "✅ UDP Server started successfully!" << std::endl;
    printServerInfo();

    return true;
}
void UDPServer::stop() {
    std::cout << "UDPServer::stop" << std::endl;
    if (!m_running) {
        std::cout << "UDP server already stopped" << std::endl;
        return;
    }

    m_running = false;

    try {
        if (m_epollManager && m_server_fd != -1) {
            try {
                m_epollManager->removeFD(m_server_fd);
                std::cout << "Removed from epoll monitoring" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "UDP epoll remove failed: " << e.what() << std::endl;
            }
        }
        if (m_server_fd != -1) {
            if (::close(m_server_fd) == -1) {
                std::cerr << "UDP server close failed: " << strerror(errno) << std::endl;
            } else {
                std::cout << "UDP server closed" << std::endl;
            }
            m_server_fd = -1;
        }
        m_epollManager = nullptr;
        std::cout << "UDP server stopped" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "UDP server stop failed: " << e.what() << std::endl;
        m_server_fd = -1;
        m_epollManager = nullptr;
    }
}
bool UDPServer::sendResponse(const sockaddr_in &clientAddr, const std::string &data) {
    if (!m_running || m_server_fd == -1) {
        std::cerr << "Cannot send - UDP Server not running";
        return false;
    }

    if (data.empty()) {
        std::cerr << "Attempted to send empty UDP message";
        return false;
    }

    ssize_t bytesSent = ::sendto(
        m_server_fd,
        data.data(),
        data.size(),
        0,
        reinterpret_cast<const sockaddr *>(&clientAddr),
        sizeof(clientAddr)
        );

    if (bytesSent == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cerr << "UDP send buffer full, packet dropped" << std::endl;
        } else {
            std::cerr << "UDP sendto error: " << strerror(errno) << std::endl;
        }
        return false;
    }

    if (bytesSent != static_cast<ssize_t>(data.size())) {
        std::cerr << "UDP send only " << bytesSent << " of " << data.size() << "bytes" << std::endl;
        return false;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(clientAddr.sin_port);

    std::cout << "UDP sent " << bytesSent << " bytes to " << client_ip << ":" << client_port << std::endl;
    std::cout << "\tResponse: " << data << std::endl;
    return true;
}


void UDPServer::printServerInfo() {
    sockaddr_in actual_addr{};
    socklen_t addr_len = sizeof(actual_addr);


    if (::getsockname(m_server_fd, reinterpret_cast<sockaddr*>(&actual_addr), &addr_len) == 0) {
        char actual_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &actual_addr.sin_addr, actual_ip, sizeof(actual_ip));
        int actual_port = ntohs(actual_addr.sin_port);

        std::cout << "UdpServerIp:    " << actual_ip << ":" << actual_port << std::endl;
    } else {
        std::cerr << "UdpServer failed: " << strerror(errno) << std::endl;
    }
}
ServerInfo UDPServer::getServerInfo() {
    m_serverInfo.isValid = (m_running && m_server_fd != -1);

    if (!m_serverInfo.isValid) {
        m_serverInfo.errorMessage = m_running ? "Socket is invalid" : "Server is not running";
        return m_serverInfo;
    }

    sockaddr_in actual_addr{};
    socklen_t addr_len = sizeof(actual_addr);

    if (::getsockname(m_server_fd, reinterpret_cast<sockaddr*>(&actual_addr), &addr_len) == 0) {
        char ip_buffer[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &actual_addr.sin_addr, ip_buffer, sizeof(ip_buffer)) != nullptr) {
            m_serverInfo.serverIP = ip_buffer;  // Успех
        } else {
            m_serverInfo.serverIP = "unknown";  // Ошибка преобразования
        }
        m_serverInfo.port = ntohs(actual_addr.sin_port);
        m_serverInfo.clientCount = 0;
        m_serverInfo.errorMessage = "";
    } else {
        m_serverInfo.isValid = false;
        m_serverInfo.errorMessage = std::string("getsockname failed: ") + strerror(errno);;
    }
    return m_serverInfo;
}
void UDPServer::handleMessage() const {
    constexpr size_t BUFFER_SIZE = 65507;
    char buffer[BUFFER_SIZE];
    sockaddr_in clientAddr{};
    socklen_t addr_len = sizeof(clientAddr);
    while (true) {
        ssize_t bytesReceived = recvfrom(
            m_server_fd,
            buffer,
            sizeof(buffer),
            0,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &addr_len
        );
        if (bytesReceived == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            std::cerr << "UDP recvfrom error: " << strerror(errno) << std::endl;
            break;
        }

        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string message(buffer, bytesReceived);

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, client_ip, sizeof(client_ip));
            int client_port = ntohs(clientAddr.sin_port);

            std::cout << "UDP received " << bytesReceived << " bytes from " << client_ip << ":" << client_port << std::endl;
            std::cout << "Message" << message << std::endl;

            if (m_messageCallback) {
                m_messageCallback(message, clientAddr);
            }
        }
    }
}

TCPServer::~TCPServer() {
    std::cout << "TCPServer::~TCPServer" << std::endl;
    stop();
}
bool TCPServer::start(std::string &ip, int port, EPollManager *epollManager) {
    std::cout << "TCPServer::start" << std::endl;
    if (m_running) {
        std::cout << "TCPServer already running" << std::endl;
        return false;
    }
    m_epollManager = epollManager;
    // Создание сокета
    m_server_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (m_server_fd == -1) {
        return false;
    }

    // Настройка ip адреса
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (ip == "0.0.0.0") {
        serv_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        int res = inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);
        if (res == -1 || res == 0 ) {
            std::cerr << "Invalid IP address" << std::endl;
            ::close(m_server_fd);
            return false;
        } else {
            std::cout << "Successfully parsed IP: " << ip << std::endl;
        }
    }

    // Настройка Опций
    int opt = 1;
    ::setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Привязка
    if (::bind(m_server_fd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) == -1) {
        std::cerr << "bind() failed to" << ip << ":" << port << std::endl;
        ::close(m_server_fd);
        return false;
    }

    if (::listen(m_server_fd, SOMAXCONN) == -1) {
        std::cerr << "listen() failed to" << strerror(errno) << std::endl;
    }
    std::cout << "TCP Server listening on " << ip << ":" << port << std::endl;

    m_epollManager->addFD(m_server_fd, EPOLLIN, this);
    m_running = true;
    return true;
}
void TCPServer::stop() {
    std::cout << "TCPServer::stop" << std::endl;
    if (!m_running) {
        std::cout << "TCPServer already stopped" << std::endl;
        return;
    }

    m_running = false;

    try {
        size_t client_count = m_clients.size();
        for (auto it = m_clients.begin(); it != m_clients.end(); ) {
            int client_fd = it->first;
            // Callback about disconnect
            if (m_disconnectCallback) {
                m_disconnectCallback(client_fd);
            }

            // Delete from epoll
            if (m_epollManager) {
                try {
                    m_epollManager->removeFD(client_fd);
                } catch (std::exception &e) {
                    std::cerr << "Warning: Failed to remove client from epoll: "<< client_fd << e.what() << std::endl;
                }
            }

            if (::close(client_fd) == -1) {
                std::cerr << "Warning: Failed to close client socket "
                << client_fd << ": " << strerror(errno) << std::endl;
            } else {
                std::cout << "Closed client connection: fd=" << client_fd << std::endl;
            }

            it = m_clients.erase(it);
        }

        if (m_server_fd != -1) {
            if (m_epollManager) {
                try {
                    m_epollManager->removeFD(m_server_fd);
                } catch (const std::exception &e) {
                    std::cerr << "Warning: Failed to remove server from epoll:  "<< m_server_fd << std::endl;
                }
            }
        }
        if (::close(m_server_fd) == -1) {
            std::cerr << "Warning: Failed to close server socket: " << strerror(errno) << std::endl;
        } else {
            std::cout << "Closed server socket" << std::endl;
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error during TCPServer stop: " << ex.what() << std::endl;
        m_server_fd = -1;
    }
}
bool TCPServer::sendData(int client_fd, const std::string &data) {

}
