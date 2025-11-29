//
// Created by roach on 19.11.2025.
//

#ifndef ASYNCSERVER_ASYNCSERVER_H
#define ASYNCSERVER_ASYNCSERVER_H

#include <functional>
#include <memory>
#include <sys/epoll.h>

#include <netinet/in.h>
#include <unordered_map>
#include "CommandProcessor.h"
#include "ServerStats.h"


class EPollManager {
public:
    EPollManager();
    EPollManager(const EPollManager&) = delete;
    EPollManager& operator=(const EPollManager&) = delete;
    ~EPollManager();

    void addFD(int fd, uint32_t events);
    void modifyFD(int fd, uint32_t events);
    void removeFD(int fd) const;
    int waitForEvents(epoll_event* events, int maxEvents, int timeout = -1);
    int getFD() const { return m_epoll_fd; }
    bool isValid() const { return m_epoll_fd != -1; }
private:
    int m_epoll_fd = -1;
};

struct ServerInfo {
    std::string serverIP;
    std::string errorMessage;
    int port;
    size_t clientCount;         // connected clients on current moment
    bool isValid;

    const std::string getAddress() const {
        return serverIP + ":" + std::to_string(port);
    }
    const std::string getStatus() const {
        if (!isValid) return "ERROR: " + errorMessage;
        return "Running on " + getAddress() + " (" + std::to_string(clientCount) + " clients connected)";
    }
};

class UDPServer {
public:
    struct UdpPacket {
        std::string data;
        sockaddr_in clientAddr;
        time_t timestamp;
    };
    using MessageCallback = std::function<void(const std::string& data, const sockaddr_in & addr)>;
    UDPServer() = default;
    UDPServer(const UDPServer&) = delete;
    UDPServer(const UDPServer&&) = delete;
    UDPServer &operator=(const UDPServer &) = delete;
    UDPServer &operator=(const UDPServer &&) = delete;
    ~UDPServer();

    bool start(std::string& ip,int port, EPollManager *epollManager);
    void stop();
    void setMessageCallback(MessageCallback cb) {m_messageCallback = std::move(cb); }
    bool sendResponse(const sockaddr_in& clientAddr, const std::string& data);
    int getFD() const { return m_server_fd; }
    bool isRunning() const {return m_running; }
    void printServerInfo();
    ServerInfo getServerInfo();

    void handleMessage() const;

private:
    int m_server_fd = -1;
    bool m_running = false;
    EPollManager * m_epollManager = nullptr;
    MessageCallback m_messageCallback;
    ServerInfo m_serverInfo;
};

class TCPServer {
public:
    using DataCallback = std::function<void(int client_fd, const std::string& data)>;
    using ConnectCallback = std::function<void(int client_fd, const sockaddr_in & addr)>;
    using DisconnectCallback = std::function<void(int client_fd)>;

    TCPServer() = default;
    TCPServer(const TCPServer&) = delete;
    TCPServer(const TCPServer&&) = delete;
    TCPServer &operator=(const TCPServer &) = delete;
    TCPServer &operator=(const TCPServer &&) = delete;
    ~TCPServer();

    bool start(std::string& ip, int port, EPollManager *epollManager);
    void stop();

    void setDataCallback(DataCallback cb) { m_dataCallback = std::move(cb); }
    void setConnectCallback(ConnectCallback cb) { m_connectCallback = std::move(cb); }
    void setDisconnectCallback(DisconnectCallback cb) { m_disconnectCallback = std::move(cb); }

    bool sendData(int client_fd, const std::string& data);
    void disconnectClient(int client_fd);

    int getFD() const { return m_server_fd; }
    bool isRunning() const { return m_running; }
    void handleNewConnection();
    void handleClientData(int client_fd);
private:
    int m_server_fd = -1;
    bool m_running = false;
    EPollManager* m_epollManager = nullptr;

    std::unordered_map<int, sockaddr_in> m_clients;

    DataCallback m_dataCallback;
    ConnectCallback m_connectCallback;
    DisconnectCallback m_disconnectCallback;


};

class AsyncServer {
public:
    explicit AsyncServer(const std::string& serverIP = "0.0.0.0", int port = 8080);
    ~AsyncServer();
    void runEventLoop();
    void exec();
    void shutdown();

    void startConsoleHandler();
    void stopConsoleHandler();
    bool isConsoleRunning() const;
private:
    std::unique_ptr<EPollManager> m_epollManager;
    std::unique_ptr<UDPServer> m_udpServer;
    std::unique_ptr<TCPServer> m_tcpServer;
    std::unique_ptr<ServerStats> m_serverStats;
    std::unique_ptr<CommandProcessor> m_commandProcessor;
    std::string m_serverIP;
    int m_serverPort;
    bool m_running = false;

    void setupCallbacks();
    void setupCommandProcessor();

    void handleTCPConnect(int client_fd, const sockaddr_in &addr);
    void handleTCPData(int client_fd, const std::string &data);
    void handleTCPDisconnect(int client_fd);
    void handleUDPData(const std::string data, const sockaddr_in& addr);
    std::string trimNetworkData(const std::string& data);
    void gracefulShutdown();
};

#endif //ASYNCSERVER_ASYNCSERVER_H
