#include <iostream>
#include <thread>
#include <map>
#include <mutex>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <csignal>
#include "logger.hpp"
#include "include/json.hpp"
#include <csignal>

// Add these global variables after your other globals
volatile sig_atomic_t shutdown_requested = 0;
int server_sock_global = -1;

void signalHandler(int signum) {
    std::cout << "\n[INFO] Shutdown signal received. Cleaning up...\n";
    shutdown_requested = 1;
    if (server_sock_global != -1) {
        close(server_sock_global);
    }
    exit(0);
}
using json = nlohmann::json;

struct NodeInfo {
    time_t last_seen;
    std::string status;
};

std::map<std::string, NodeInfo> cluster;
std::mutex cluster_mutex;

const int PORT = 5050;
const int TIMEOUT = 11; // seconds
const int DISPLAY_INTERVAL = 10; // seconds
time_t last_display_time = 0;

Logger logger("manager.log");

// ----------------------------------------------------
// Persist and load cluster state
// ----------------------------------------------------
void persistClusterState() {
    json j;
    {
        std::lock_guard<std::mutex> lock(cluster_mutex);
        for (auto &p : cluster) {
            j[p.first] = {
                {"status", p.second.status},
                {"last_seen", p.second.last_seen}
            };
        }
    }
    std::ofstream file("cluster_state.json");
    file << j.dump(4);
}

void loadClusterState() {
    std::ifstream file("cluster_state.json");
    if (!file.is_open()) return;
    json j; file >> j;
    for (auto &[node, info] : j.items()) {
        cluster[node] = {info["last_seen"], info["status"]};
    }
    logger.info("Cluster state loaded from file.");
}

// ----------------------------------------------------
// Display the current cluster state
// ----------------------------------------------------
void displayClusterState() {
    std::lock_guard<std::mutex> lock(cluster_mutex);
    std::cout << "\n=== Cluster State ===" << std::endl;
    for (auto &p : cluster) {
        std::string last_seen = std::string(ctime(&p.second.last_seen));
        if (!last_seen.empty() && last_seen.back() == '\n') {
            last_seen.pop_back();
        }
        std::cout << p.first << " | " << p.second.status
                  << " | Last seen: " << last_seen << std::endl;
    }
    std::cout << "=====================\n" << std::endl;
}

// ----------------------------------------------------
// Thread that monitors nodes and marks failures
// ----------------------------------------------------
void monitorNodes() {
    logger.info("Monitor thread started...");
    displayClusterState(); // show on startup

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        time_t now = time(nullptr);
        bool failure_detected = false;

        {
            std::lock_guard<std::mutex> lock(cluster_mutex);
            for (auto &[node, info] : cluster) {
                double diff = difftime(now, info.last_seen);
                if (diff > TIMEOUT && info.status == "active") {
                    info.status = "failed";
                    logger.warn("Node " + node + " failed (no heartbeat)");
                    failure_detected = true;
                }
            }
        }

        if (failure_detected || difftime(now, last_display_time) >= DISPLAY_INTERVAL) {
            displayClusterState();
            persistClusterState();
            last_display_time = now;
        }
    }
}

// ----------------------------------------------------
// Handles each client connection
// ----------------------------------------------------
void handleClient(int client_sock) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = read(client_sock, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) break;

        std::string msg(buffer);
        
        // Process line by line to prevent concatenation
        size_t pos = 0;
        while ((pos = msg.find('\n')) != std::string::npos) {
            std::string line = msg.substr(0, pos);
            msg.erase(0, pos + 1);
            
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r"));
            line.erase(line.find_last_not_of(" \t\r") + 1);
            
            if (line.rfind("REGISTER ", 0) == 0) {
                std::string node_id = line.substr(9);
                std::lock_guard<std::mutex> lock(cluster_mutex);
                cluster[node_id] = {time(nullptr), "active"};
                logger.info("REGISTER received for " + node_id);
            }
            else if (line.rfind("HEARTBEAT ", 0) == 0) {
                std::string node_id = line.substr(10);
                std::lock_guard<std::mutex> lock(cluster_mutex);
                cluster[node_id].last_seen = time(nullptr);
                cluster[node_id].status = "active";
            }
        }
    }
    close(client_sock);
}

// ----------------------------------------------------
// Server start function
// ----------------------------------------------------
// void startServer() {
//     int server_sock = socket(AF_INET, SOCK_STREAM, 0);
//     sockaddr_in server_addr{};
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_addr.s_addr = INADDR_ANY;
//     server_addr.sin_port = htons(PORT);

//     bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
//     listen(server_sock, 5);
//     logger.info("Manager listening on port " + std::to_string(PORT));

//     std::thread monitorThread(monitorNodes);
//     monitorThread.detach();

//     while (true) {
//         sockaddr_in client_addr{};
//         socklen_t client_len = sizeof(client_addr);
//         int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
//         std::thread(handleClient, client_sock).detach();
//     }
// }
void startServer() {
    server_sock_global = socket(AF_INET, SOCK_STREAM, 0);
    
    // Allow port reuse immediately after restart
    int opt = 1;
    setsockopt(server_sock_global, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock_global, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock_global, 5);
    logger.info("Manager listening on port " + std::to_string(PORT));

    std::thread monitorThread(monitorNodes);
    monitorThread.detach();

    while (!shutdown_requested) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock_global, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock < 0) {
            if (shutdown_requested) break;
            continue;
        }
        
        std::thread(handleClient, client_sock).detach();
    }
    
    close(server_sock_global);
    logger.info("Manager shut down gracefully.");
}

bool isPortAvailable(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    // If connect() fails, port not in use (manager down)
    return (result < 0);
}

int main(int argc, char* argv[]) {
    std::string role = "primary";
    if (argc >= 2) role = argv[1];

    if (role == "primary") {
        std::cout << "[INFO] Starting PRIMARY manager..." << std::endl;
        loadClusterState();
        startServer();
    }
    else if (role == "backup") {
        std::cout << "[INFO] Starting BACKUP manager monitor..." << std::endl;

        while (true) {
            if (isPortAvailable(PORT)) {
                std::cout << "[WARN] Primary not reachable. Taking over as ACTIVE..." << std::endl;
                loadClusterState();
                startServer();   // This will block until terminated
            } else {
                std::cout << "[INFO] Primary alive. Backup waiting..." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    else {
        std::cerr << "Usage: ./manager [primary|backup]" << std::endl;
    }

    return 0;
}
// ----------------------------------------------------
// Main
// ----------------------------------------------------
// int main() {
//     loadClusterState();
//     startServer();
//     return 0;
// }
