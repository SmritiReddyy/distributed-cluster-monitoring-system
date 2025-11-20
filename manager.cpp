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

const int PORT = 5000;
const int TIMEOUT = 5; // seconds
Logger logger("manager.log");

void monitorNodes() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        time_t now = time(0);
        std::lock_guard<std::mutex> lock(cluster_mutex);
        for (auto &pair : cluster) {
            if (difftime(now, pair.second.last_seen) > TIMEOUT &&
                pair.second.status == "active") {
                pair.second.status = "failed";
                logger.warn("Node " + pair.first + " failed (no heartbeat)");
            }
        }
    }
}

void displayClusterState() {
    std::lock_guard<std::mutex> lock(cluster_mutex);
    std::cout << "\n=== Cluster State ===\n";
    for (auto &pair : cluster) {
        std::cout << pair.first << " | " << pair.second.status
                  << " | Last seen: " << ctime(&pair.second.last_seen);
    }
    std::cout << "=====================\n";
}

void handleClient(int client_sock) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = read(client_sock, buffer, sizeof(buffer));
        if (bytes <= 0) break;
        std::string msg(buffer);
        if (msg.rfind("REGISTER", 0) == 0) {
            std::string node_id = msg.substr(9);
            node_id.erase(node_id.find_last_not_of(" \n\r\t")+1);
            std::lock_guard<std::mutex> lock(cluster_mutex);
            cluster[node_id] = {time(0), "active"};
            logger.info("Node registered: " + node_id);
        } else if (msg.rfind("HEARTBEAT", 0) == 0) {
            std::string node_id = msg.substr(10);
            node_id.erase(node_id.find_last_not_of(" \n\r\t")+1);
            std::lock_guard<std::mutex> lock(cluster_mutex);
            cluster[node_id].last_seen = time(0);
            cluster[node_id].status = "active";
        }
        displayClusterState();
    }
    close(client_sock);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);
    logger.info("Manager listening on port " + std::to_string(PORT));

    std::thread monitorThread(monitorNodes);
    monitorThread.detach();

    while (true) {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        std::thread(handleClient, client_sock).detach();
    }
}