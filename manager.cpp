#include <iostream>
#include <thread>
#include <map>
#include <mutex>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include "logger.hpp"

struct NodeInfo {
    time_t last_seen;
    std::string status;
};

std::map<std::string, NodeInfo> cluster;
std::mutex cluster_mutex;

const int PORT = 5050;
const int TIMEOUT = 11; // seconds
time_t last_display_time = 0;
const int DISPLAY_INTERVAL = 10; // seconds (change to 10 if you want)

Logger logger("manager.log");

// ----------------------------------------------------
// Display the current cluster state
// ----------------------------------------------------
// void displayClusterState() {
//     std::lock_guard<std::mutex> lock(cluster_mutex);
//     std::cout << "\n=== Cluster State ===" << std::endl;
//     for (auto &p : cluster) {
//         std::cout << p.first << " | " << p.second.status
//                   << " | Last seen: " << ctime(&p.second.last_seen);
//     }
//     std::cout << "=====================\n" << std::endl;
// }
void displayClusterState() {
    std::lock_guard<std::mutex> lock(cluster_mutex);
    std::cout << "\n=== Cluster State ===" << std::endl;
    for (auto &p : cluster) {
        std::string last_seen = std::string(ctime(&p.second.last_seen));
        if (!last_seen.empty() && last_seen.back() == '\n') {
            last_seen.pop_back(); // remove newline
        }
        std::cout << p.first << " | " << p.second.status
                  << " | Last seen: " << last_seen << std::endl;
    }
    std::cout << "=====================\n" << std::endl;
}


// ----------------------------------------------------
// Thread that monitors nodes and marks failures
// ----------------------------------------------------
// void monitorNodes() {
//     while (true) {
//         std::this_thread::sleep_for(std::chrono::seconds(2));
//         std::lock_guard<std::mutex> lock(cluster_mutex);
//         time_t now = time(nullptr);

//         for (auto &[node, info] : cluster) {
//             double diff = difftime(now, info.last_seen);
//             if (diff > TIMEOUT && info.status == "active") {
//                 info.status = "failed";
//                 logger.warn("Node " + node + " failed (no heartbeat)");
//                 displayClusterState();
//             }
//         }
//     }
// }

void monitorNodes() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        time_t now = time(nullptr);
        bool failure_detected = false;

        // Detect failures
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

        // Display cluster state outside lock (so it never gets stuck)
        if (failure_detected) {
            displayClusterState();
        }

        // Periodic display every 10 seconds
        if (difftime(now, last_display_time) >= DISPLAY_INTERVAL) {
            displayClusterState();
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
        if (msg.rfind("REGISTER", 0) == 0) {
            std::string node_id = msg.substr(9);
            node_id.erase(node_id.find_last_not_of(" \n\r\t") + 1);
            {
                std::lock_guard<std::mutex> lock(cluster_mutex);
                cluster[node_id] = {time(nullptr), "active"};
            }
            logger.info("Node " + node_id + " registered.");
            // displayClusterState();
            time_t now = time(nullptr);
            if (difftime(now, last_display_time) >= DISPLAY_INTERVAL) {
                displayClusterState();
                last_display_time = now;
}

        }
        // else if (msg.rfind("HEARTBEAT", 0) == 0) {
        //     std::string node_id = msg.substr(10);
        //     node_id.erase(node_id.find_last_not_of(" \n\r\t") + 1);
        //     {
        //         std::lock_guard<std::mutex> lock(cluster_mutex);
        //         cluster[node_id].last_seen = time(nullptr);
        //         cluster[node_id].status = "active";
        //     }
        //     logger.info("Heartbeat received from " + node_id);
        //     std::cout << "HEARTBEAT " << node_id << " received.\n";
        //     std::cout << "[INFO] Heartbeat received from " << node_id << std::endl;
        //     displayClusterState();

        // }




        else if (msg.rfind("HEARTBEAT", 0) == 0) {
            std::string node_id = msg.substr(10);
            node_id.erase(node_id.find_last_not_of(" \n\r\t") + 1);
            {
                std::lock_guard<std::mutex> lock(cluster_mutex);
                cluster[node_id].last_seen = time(nullptr);
                cluster[node_id].status = "active";
            }
            logger.info("Heartbeat received from " + node_id);

            time_t now = time(nullptr);
            if (difftime(now, last_display_time) >= DISPLAY_INTERVAL) {
                displayClusterState();
                last_display_time = now;
            }
        }


    }

    close(client_sock);
}

// ----------------------------------------------------
// Server start function
// ----------------------------------------------------
void startServer() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);
    logger.info("Manager listening on port " + std::to_string(PORT));

    std::thread monitorThread(monitorNodes);
    monitorThread.detach();

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        std::thread(handleClient, client_sock).detach();
    }
}

// ----------------------------------------------------
// Main
// ----------------------------------------------------
int main() {
    startServer();
    return 0;
}
