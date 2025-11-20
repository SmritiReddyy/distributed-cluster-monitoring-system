#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <thread>
#include <chrono>
#include "logger.hpp"

const char* MANAGER_IP = "127.0.0.1";
const int PORT = 5000;
const int HEARTBEAT_INTERVAL = 2; // seconds

Logger logger("worker.log");

void sendMessage(int sock, const std::string &msg) {
    send(sock, msg.c_str(), msg.length(), 0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./worker <node_id>\n";
        return 1;
    }
    std::string node_id = argv[1];
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, MANAGER_IP, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    logger.info("Connected to manager. Node ID: " + node_id);
    sendMessage(sock, "REGISTER " + node_id + "\n");

    while (true) {
        sendMessage(sock, "HEARTBEAT " + node_id + "\n");
        logger.info("Heartbeat sent from " + node_id);
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
    }

    close(sock);
    return 0;
}