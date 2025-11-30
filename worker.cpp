#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include "logger.hpp"

const char* MANAGER_IP = "127.0.0.1";
const int PORT = 5050;
const int HEARTBEAT_INTERVAL = 2; // seconds
const int RETRY_INTERVAL = 3;     // seconds

Logger logger("worker.log");

// ------------------------------------------------------------------
// Helper to send a message safely
// ------------------------------------------------------------------
void sendMessage(int sock, const std::string &msg) {
    send(sock, msg.c_str(), msg.length(), 0);
}

// ------------------------------------------------------------------
// Try connecting repeatedly until manager is available
// ------------------------------------------------------------------
int connectWithRetry(sockaddr_in &serv_addr) {
    int sock;
    while (true) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
            return sock; // success
        }
        std::cerr << "[WARN] Manager unavailable. Retrying in "
                  << RETRY_INTERVAL << "s...\n";
        std::this_thread::sleep_for(std::chrono::seconds(RETRY_INTERVAL));
        close(sock);
    }
}

void enableKeepAlive(int sock) {
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    
    #ifdef __linux__
    // Linux-specific TCP keepalive settings
    int keepidle = 2;   // Start probes after 2 seconds idle
    int keepintvl = 2;  // Probe every 2 seconds
    int keepcnt = 2;    // Drop after 2 failed probes
    
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
    #endif
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------
// int main(int argc, char* argv[]) {
//     // Prevent worker from crashing when manager disconnects
//     signal(SIGPIPE, SIG_IGN);

//     if (argc < 2) {
//         std::cerr << "Usage: ./worker <node_id>\n";
//         return 1;
//     }
//     std::string node_id = argv[1];

//     sockaddr_in serv_addr{};
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(PORT);
//     inet_pton(AF_INET, MANAGER_IP, &serv_addr.sin_addr);

//     int sock = connectWithRetry(serv_addr);
//     logger.info("Connected to manager. Node ID: " + node_id);
//     sendMessage(sock, "REGISTER " + node_id + "\n");

//     while (true) {
//         std::string msg = "HEARTBEAT " + node_id + "\n";
//         ssize_t result = send(sock, msg.c_str(), msg.length(), 0);

//         if (result <= 0) {
//             logger.warn("Lost connection to manager. Reconnecting...");
//             close(sock);
//             sock = connectWithRetry(serv_addr);
//             logger.info("Reconnected to manager. Re-registering " + node_id);
//             sendMessage(sock, "REGISTER " + node_id + "\n");
//         } else {
//             logger.info("Heartbeat sent from " + node_id);
//         }

//         std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
//     }

//     close(sock);
//     return 0;
// }

int main(int argc, char* argv[]) {
    // Prevent worker from crashing when manager disconnects
    signal(SIGPIPE, SIG_IGN);

    if (argc < 2) {
        std::cerr << "Usage: ./worker <node_id>\n";
        return 1;
    }
    std::string node_id = argv[1];

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, MANAGER_IP, &serv_addr.sin_addr);

    int sock = connectWithRetry(serv_addr);
    enableKeepAlive(sock);  // ← ADD THIS
    logger.info("Connected to manager. Node ID: " + node_id);
    sendMessage(sock, "REGISTER " + node_id + "\n");

    while (true) {
        std::string msg = "HEARTBEAT " + node_id + "\n";
        ssize_t result = send(sock, msg.c_str(), msg.length(), 0);

        if (result <= 0) {
            logger.warn("Lost connection to manager. Reconnecting...");
            close(sock);
            sock = connectWithRetry(serv_addr);
            enableKeepAlive(sock);  // ← AND HERE
            logger.info("Reconnected to manager. Re-registering " + node_id);
            sendMessage(sock, "REGISTER " + node_id + "\n");
        } else {
            logger.info("Heartbeat sent from " + node_id);
        }

        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
    }

    close(sock);
    return 0;
}