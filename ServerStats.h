//
// Created by roach on 19.11.2025.
//

#ifndef ASYNCSERVER_SERVERSTATS_H
#define ASYNCSERVER_SERVERSTATS_H

#include <atomic>

class ServerStats {
    std::atomic<size_t> m_total_clients;
    std::atomic<size_t> m_current_clients;

public:
    ServerStats() : m_total_clients(0), m_current_clients(0) {}

    void clientConnected() {
        m_total_clients++;
        m_current_clients++;
    }
    void clientDisconnected() { m_current_clients--; }
    size_t getTotalClients() const { return m_total_clients; }
    size_t getCurrentClients() const { return m_current_clients; }
};


#endif //ASYNCSERVER_SERVERSTATS_H