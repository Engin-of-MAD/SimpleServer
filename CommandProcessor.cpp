//
// Created by roach on 19.11.2025.
//

#include "CommandProcessor.h"

#include <iomanip>
#include <iostream>
#include <termios.h>
CommandProcessor::~CommandProcessor() {
    stopConsoleHandler();
}
std::string CommandProcessor::processCommand(std::string &command, ServerStats &stats) {
    if (command == "/time") {
        return getCurrentDateTime();
    } else if (command == "/stats") {
        return formatStats(stats);
    } else if (command == "/shutdown") {
        return "SHUTDOWN";
    } else if (!command.empty() && command[0] == '/') {
        return "Unknown command: " + command;
    } else {
        return command;
    }
}
bool CommandProcessor::startConsoleHandler() {
    if (m_consoleRunning.exchange(true)) {
        std::cout << "Console handler is already running" << std::endl;
        return false;
    }
    try {
        m_consoleThread = std::thread(&CommandProcessor::consoleInputHandler, this);
        std::cout << "Console command handler started. Type 'help' for available commands." << std::endl;
        return true;
    } catch (std::exception& e) {
        m_consoleRunning = false;
        std::cerr << "Failed to start console thread: " << e.what() << std::endl;
        return false;
    }
}
void CommandProcessor::stopConsoleHandler() {
    if (!m_consoleRunning.exchange(false)) {
        return;
    }

    if (m_consoleThread.joinable()) {
        try {
            m_consoleThread.join();
        } catch (std::exception& e) {
            std::cerr << "Error joining console thread: " << e.what() << std::endl;
        }
    }
    std::cout << "Console command handler stopped" << std::endl;
}
std::string CommandProcessor::getCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&time);

    std::ostringstream oss;
    oss << std::setfill('0')
        << (localTime->tm_year + 1900) << "-"
        << (localTime->tm_mon + 1) << "-"
        << (localTime->tm_mday) << " "
        << (localTime->tm_hour) << ":"
        << (localTime->tm_min) << ":"
        << (localTime->tm_sec);
    return oss.str();
}
std::string CommandProcessor::formatStats(const ServerStats &stats) {
    std::ostringstream oss;
    oss << "Server statistics:\n"
    << "\tCurrent connected clients: " << stats.getCurrentClients() << "\n"
    << "\tTotal clients connected: " << stats.getTotalClients();

    return oss.str();
}
void CommandProcessor::consoleInputHandler() {

    std::cout << "> ";
    while (m_consoleRunning.load()) {
        std::string input;
        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input.empty()) {
            std::cout << "> ";
            continue;
        }

        processConsoleInput(input);

        if (!m_consoleRunning.load()) break;
        std::cout << "> ";
    }
}

void CommandProcessor::processConsoleInput(const std::string &input) {
    if (input == "help" || input == "?") {
        std::cout << "Available console commands:\n"
          << "  /help, ?        - Show this help\n"
          << "  /shutdown - Stop the server\n"
          << "  /stats          - Show server statistics\n"
          << "  /time           - Show current time\n"
          << std::endl;
    } else if (input == "/status") {
        if (m_statsCallback) {
            std::cout << m_statsCallback() << std::endl;
        } else {
            std::cout << "Status callback not set" << std::endl;
        }
    } else if (input == "/shutdown") {
        std::cout << "Shutting down server..." << std::endl;
        if (m_shutdownCallback) {
            m_shutdownCallback();
        } else {
            std::cout << "Shutdown callback not set" << std::endl;
        }
    } else if (input == "/stats"){
        if (m_statsCallback) {
            std::cout << m_statsCallback() << std::endl;
        } else {
            std::cout << "Stats callback not set" << std::endl;
        }
    } else if (input == "/time") {
        std::cout << getCurrentDateTime() << std::endl;
    } else if (input[0] == '/') {
        std::cout << "Unknown console command: " << input
                  << "\nType 'help' for available commands." << std::endl;
    } else {
        std::cout << input << std::endl;
    }
}

