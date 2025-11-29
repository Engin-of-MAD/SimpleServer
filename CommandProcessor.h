//
// Created by roach on 19.11.2025.
//

#ifndef ASYNCSERVER_PARSERCLI_H
#define ASYNCSERVER_PARSERCLI_H
#include <functional>
#include <string>
#include <thread>

#include "ServerStats.h"

class CommandProcessor {
public:
    using ShutdownCallback = std::function<void()>;
    using StatsCallback = std::function<std::string()>;

    CommandProcessor() = default;
    ~CommandProcessor();

    std::string processCommand(std::string& command, ServerStats& stats);

    // handlers
    bool startConsoleHandler();
    void stopConsoleHandler();
    bool isConsoleRunning() const { return m_consoleRunning; }

    // reg callbacks
    void setShutdownCallback(ShutdownCallback cb) { m_shutdownCallback = std::move(cb); }
    void setStatsCallbacks(StatsCallback cb) { m_statsCallback = std::move(cb); }

    static std::string getCurrentDateTime();
    static std::string formatStats(const ServerStats& stats);

private:
    void consoleInputHandler();
    void processConsoleInput(const std::string& input);

    // Callbacks
    ShutdownCallback m_shutdownCallback;
    StatsCallback m_statsCallback;

    //sync
    std::atomic<bool> m_consoleRunning{false} ;
    std::atomic<bool> m_shutdownRequested{false};
    std::thread m_consoleThread;
    
};


#endif //ASYNCSERVER_PARSERCLI_H