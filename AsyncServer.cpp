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
void EPollManager::addFD(int fd, uint32_t events) {
    std::cout << "EPollManager::addFD" << std::endl;
    epoll_event event{0};
    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        throw std::system_error(errno, std::system_category(),
            "epoll_ctl ADD failed");
    }
}
void EPollManager::modifyFD(int fd, uint32_t events) {
    std::cout << "EPollManager::modifyFD" << std::endl;
    if (m_epoll_fd == -1) {
        throw std::system_error(EBADF, std::system_category(),
            "\tEPoll instance is invalid");
    }

    struct epoll_event event{0};
    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) {
        throw std::system_error(errno, std::system_category(),
            "\tepoll_ctl MODIFY for fd " + std::to_string(fd));
    }

}
void EPollManager::removeFD(int fd) const {
    std::cout << "EPollManager::removeFD" << std::endl;
    if (m_epoll_fd == -1) {
        return;
    }
    std::cout << "EPollManager::removeFD fd=" << fd << std::endl;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);

}


int EPollManager::waitForEvents(epoll_event *events, int maxEvents, int timeout) {
    // std::cout << "EPollManager::waitForEvents" << std::endl;
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
        if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) != 1) {
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
        m_epollManager->addFD(m_server_fd, EPOLLIN | EPOLLET);
    } catch (const std::exception& e) {
        std::cerr << "UDP epoll add failed: " << e.what() << std::endl;
        ::close(m_server_fd);
        return false;
    }

    m_running = true;
    std::cout << " UDP Server started successfully!" << std::endl;
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
    constexpr size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    sockaddr_in clientAddr{};
    socklen_t addr_len = sizeof(clientAddr);

    while (true) {
        ssize_t bytesReceived = ::recvfrom(
        m_server_fd,
        buffer,
        BUFFER_SIZE - 1,
        0,
        reinterpret_cast<sockaddr*>(&clientAddr),
        &addr_len);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string message(buffer);

            if (m_messageCallback) {
                m_messageCallback(message, clientAddr);
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                std::cerr << "UDP recvfrom error: " << strerror(errno) << std::endl;
                break;
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
        ::close(m_server_fd);
        m_server_fd = -1;
        return false;
    }
    std::cout << "TCP Server listening on " << ip << ":" << port << std::endl;

    m_epollManager->addFD(m_server_fd, EPOLLIN);
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

    for (auto it = m_clients.begin(); it != m_clients.end();) {
        int client_fd = it->first;
        // Callback about disconnect
        if (m_disconnectCallback) {
            m_disconnectCallback(client_fd);
        }

        // Delete from epoll
        if (m_epollManager) {
            m_epollManager->removeFD(client_fd);
        }

        // Close clients sockets
        if (::close(client_fd) == -1) {
            std::cerr << "Warning: Failed to close client socket "
            << client_fd << ": " << strerror(errno) << std::endl;
        }
        it = m_clients.erase(it);
    }

    // Close server sockets
    if (m_server_fd != -1) {
        if (m_epollManager) {
            m_epollManager->removeFD(m_server_fd);
        }
        if (::close(m_server_fd) == -1) {
            std::cerr << "Warning: Failed to close server socket: "
                      << strerror(errno) << std::endl;
        } else {
            std::cout << "Closed server socket" << std::endl;
        }
        m_server_fd = -1;
    }
}
bool TCPServer::sendData(int client_fd, const std::string &data) {
    if (m_clients.find(client_fd) == m_clients.end()) {
        std::cerr << "Cannot send data - client " << client_fd << " not found" << std::endl;
        return false;
    }
    if (!m_running) {
        std::cerr << "Cannot send data - TCP server not running" << std::endl;
        return false;
    }

    if (data.empty()) {
        std::cerr << "Attempt to send empty data to client" << std::endl;
        return false;
    }

    ssize_t bytes_sent = send(client_fd, data.data(), data.size(), 0);
    if (bytes_sent == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            disconnectClient(client_fd);
            std::cerr << "TCP send error to client " << client_fd << ": " << strerror(errno) << std::endl;
        }
        return false;
    }

    return true;
}

void TCPServer::disconnectClient(int client_fd) {
    auto it = m_clients.find(client_fd);
    if (it == m_clients.end()) {
        std::cout << "Client " << client_fd << " already disconnected" << std::endl;
        return;
    }

    std::cout << "Disconnect client" << client_fd << "..." << std::endl;
    if (m_disconnectCallback) {
        m_disconnectCallback(client_fd);
    }

    if (m_epollManager) {
        try {
            m_epollManager->removeFD(client_fd);
        } catch (const std::exception &e) {
            std::cerr << "Error removing client " << client_fd << "from epoll: " << e.what() << std::endl;
        }
    }

    if (::close(client_fd) == -1) {
        std::cerr << "Error closing client socket " << client_fd << ":" << strerror(errno) << std::endl;
    }

    m_clients.erase(it);
    std::cout << "Client " << client_fd << " disconnected successfully" << std::endl;
}
void TCPServer::handleNewConnection() {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept4(
            m_server_fd,
            reinterpret_cast<sockaddr*>(&client_addr),
            &addr_len,
            SOCK_NONBLOCK
            );
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                std::cerr << "TCPServer accept error: " << strerror(errno) << std::endl;
                break;
            }
        }

        m_clients[client_fd] = client_addr;

        try {
            m_epollManager->addFD(client_fd, EPOLLIN | EPOLLET | EPOLLRDHUP);
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            int client_port = ntohs(client_addr.sin_port);
            std::cout << "New TCP client connected: " << client_ip << ":" << client_port
            << " (fd: " << client_fd << ")" << std::endl;
            if (m_connectCallback) {
                m_connectCallback(client_fd, client_addr);
            }
        } catch (const std::exception &e) {
            std::cerr << "Failed to add client to epoll: " << e.what() << std::endl;
            ::close(client_fd);
            m_clients.erase(client_fd);
        }
    }
}
void TCPServer::handleClientData(int client_fd) {
    constexpr size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            std::string message(buffer);

            if (m_dataCallback) {
                m_dataCallback(client_fd, message);
            }
        } else if (bytes_read == 0) {
            disconnectClient(client_fd);
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                disconnectClient(client_fd);
                break;
            }
        }

    }
}
AsyncServer::AsyncServer(const std::string &serverIP, int port)
: m_serverIP(serverIP)
, m_serverPort(port) {
    m_epollManager = std::make_unique<EPollManager>();
    m_udpServer = std::make_unique<UDPServer>();
    m_tcpServer = std::make_unique<TCPServer>();
    m_serverStats = std::make_unique<ServerStats>();
    m_commandProcessor = std::make_unique<CommandProcessor>();

    setupCallbacks();
    setupCommandProcessor();
}
AsyncServer::~AsyncServer() {
    shutdown();
}
void AsyncServer::runEventLoop() {
    constexpr size_t MAX_EVENTS = 64;
    constexpr size_t EPOLL_TIMEOUT_MS = 100;

    epoll_event events[MAX_EVENTS];

    int tcp_server_fd = m_tcpServer->getFD();
    int udp_server_fd = m_udpServer->getFD();

    std::cout << "Starting event loop. TCP server fd: " << tcp_server_fd
              << ", UDP server fd: " << udp_server_fd << std::endl;

    while (m_running) {
        int event_count = m_epollManager->waitForEvents(events, MAX_EVENTS, EPOLL_TIMEOUT_MS);

        for (int i = 0; i < event_count; ++i) {
            int fd = events[i].data.fd;
            uint32_t event_mask = events[i].events;

            if (fd == tcp_server_fd) {
                std::cout << "TCP server socket event" << std::endl;
                if (event_mask & EPOLLIN) {
                    m_tcpServer->handleNewConnection();
                }
                if (event_mask & EPOLLERR) {
                    std::cerr << "TCP server socket error" << std::endl;
                }
            } else if (fd == udp_server_fd) {
                std::cout << "UDP server socket event" << std::endl;
                if (event_mask & EPOLLIN) {
                    m_udpServer->handleMessage();
                }
                if (event_mask & EPOLLERR) {
                    std::cerr << "UDP server socket error" << std::endl;
                }
            } else {
                if (event_mask & (EPOLLIN | EPOLLRDHUP)) {
                    m_tcpServer->handleClientData(fd);
                }
                if (event_mask & EPOLLERR) {
                    std::cerr << "TCP client socket " << fd << " error" << std::endl;
                    m_tcpServer->disconnectClient(fd);
                }

                if (event_mask & EPOLLHUP) {
                    std::cout << "Client " << fd << " disconnected (EPOLLHUP)" << std::endl;
                    m_tcpServer->disconnectClient(fd);
                }
            }
        }
    }
}
void AsyncServer::exec() {
    std::cout << "AsyncServer::exec - Starting server..." << std::endl;
    if (m_running) {
        std::cout << "Server is already running" << std::endl;
        return;
    }
    if (!m_tcpServer->start(m_serverIP, m_serverPort, m_epollManager.get())) {
        std::cerr << "Failed to start TCP server: " << std::endl;
        return;
    }
    if (!m_udpServer->start(m_serverIP, m_serverPort, m_epollManager.get())) {
        std::cerr << "Failed to start UDP server: " << std::endl;
        return;
    }

    startConsoleHandler();
    m_running = true;
    std::cout << "AsyncServer started successfully on " << m_serverIP << ":" << m_serverPort << std::endl;
    runEventLoop();
}
void AsyncServer::shutdown() {
    std::cout << "AsyncServer::shutdown - Initiating shutdown..." << std::endl;
    m_running = false;
}

void AsyncServer::startConsoleHandler() {
    if (m_commandProcessor) {
        m_commandProcessor->startConsoleHandler();
    }
}
void AsyncServer::stopConsoleHandler() {
    if (m_commandProcessor) {
        m_commandProcessor->stopConsoleHandler();
    }
}
bool AsyncServer::isConsoleRunning() const {
    return m_commandProcessor ? m_commandProcessor->isConsoleRunning() : false;
}
void AsyncServer::setupCallbacks() {
    m_tcpServer->setDataCallback([this](int client_fd,  const std::string &message) {
        this->handleTCPData(client_fd, message);
    });

    m_tcpServer->setConnectCallback([this](int client_fd, const sockaddr_in& addr) {
        this->handleTCPConnect(client_fd, addr);
    });

    m_tcpServer->setDisconnectCallback([this](int client_fd) {
        this->handleTCPDisconnect(client_fd);
    });

    m_udpServer->setMessageCallback([this](const std::string &message, const sockaddr_in& addr) {
        this->handleUDPData(message, addr);
    });
}
void AsyncServer::setupCommandProcessor() {
    std::cout << "AsyncServer::setupCommandProcessor" << std::endl;
    m_commandProcessor->setShutdownCallback([this]() {
        std::cout << "Shutdown requested via CommandProcessor" << std::endl;
        this->shutdown();
    });

    m_commandProcessor->setStatsCallbacks([this]() {
        return CommandProcessor::formatStats(*m_serverStats);
    });
}

void AsyncServer::handleTCPConnect(int client_fd, const sockaddr_in &addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(addr.sin_port);

    std::cout << "AsyncServer::handleTCPConnect - Client connected: "
                  << client_ip << ":" << client_port << " (fd: " << client_fd << ")" << std::endl;
    m_serverStats->clientConnected();
}
void AsyncServer::handleTCPData(int client_fd, const std::string &data) {
    std::cout << "AsyncServer::handleTCPData from client " << client_fd << ": " << data << std::endl;
    std::string response;
    std::string trimmedData = trimNetworkData(data);
    if (!trimmedData.empty() && trimmedData[0] == '/') {
        // for processor commands
        std::string command = trimmedData;
        response = m_commandProcessor->processCommand(command, *m_serverStats);
        if (response == "SHUTDOWN") {
            m_tcpServer->sendData(client_fd, "Server shutting down...");
            shutdown();
            return;
        }
    } else {
        response = trimmedData;
    }
    m_tcpServer->sendData(client_fd, response);
}
void AsyncServer::handleTCPDisconnect(int client_fd) {
    std::cout << "AsyncServer::handleTCPDisconnect - Client disconnected: " << client_fd << std::endl;
    m_serverStats->clientDisconnected();
}

void AsyncServer::handleUDPData(const std::string data, const sockaddr_in &addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(addr.sin_port);

    std::cout << "AsyncServer::handleUDPData from " << client_ip << ":" << client_port  << ": " << data << std::endl;

    std::string response;
    if (!data.empty() && data[0] != '/') {
        // to command processor
        std::string command = data;
        response = m_commandProcessor->processCommand(command, *m_serverStats);
        if (response.find("SHUTDOWN")) {
            m_udpServer->sendResponse(addr, "Server shutting down...");
            shutdown();
            return;
        }
    } else {
        response = data;
    }

    m_udpServer->sendResponse(addr, response);
}
std::string AsyncServer::trimNetworkData(const std::string &data)  {
    if (data.empty()) return data;

    size_t end = data.length();

    while (end > 0 && (data[end-1] == '\n' || data[end-1] == '\r' || data[end-1] == ' ' || data[end-1] == '\t')) {
        end--;
    }

    return data.substr(0, end);
}
void AsyncServer::gracefulShutdown() {
    std::cout << "AsyncServer::gracefulShutdown - Performing graceful shutdown..." << std::endl;

    if (m_commandProcessor && m_commandProcessor->isConsoleRunning()) {
        std::cout << "Stopping console handler in gracefulShutdown..." << std::endl;
        m_commandProcessor->stopConsoleHandler();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    m_running = false;

    if (m_tcpServer) {
        m_tcpServer->stop();
    }

    if (m_udpServer) {
        m_udpServer->stop();
    }

    std::cout << "AsyncServer shutdown complete" << std::endl;
}
