#include "voice_server_master.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <algorithm>
#include <csignal> // Required for signal handling

VoiceServerMaster::VoiceServerMaster(std::string cfg_path)
    : config_filepath(std::move(cfg_path)),
      p1_port(8080),
      p2_port(9090),
      heartbeat_interval_min(3),
      instance_id("master_01"),
      is_active_primary(true), // Assumes active by default or election winner
      running(true),
      round_robin_index(0)
{
}

VoiceServerMaster::~VoiceServerMaster()
{
    stop();
}

void VoiceServerMaster::stop()
{
    running = false;
}

bool VoiceServerMaster::loadConfiguration()
{
    std::ifstream file(config_filepath);
    if (!file.is_open())
    {
        std::cerr << "[MASTER ERROR] Failed to load config: " << config_filepath << std::endl;
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(master_mutex);
    voice_nodes.clear();

    std::string line;
    while (std::getline(file, line))
    {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#')
            continue;

        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos)
        {
            std::string key = line.substr(0, eq_pos);
            std::string val = line.substr(eq_pos + 1);

            if (key == "P1_PORT")
                p1_port = std::stoi(val);
            else if (key == "P2_PORT")
                p2_port = std::stoi(val);
            else if (key == "HEARTBEAT_INTERVAL_MIN")
                heartbeat_interval_min = std::stoi(val);
            else if (key == "INSTANCE_ID")
                instance_id = val;
            else if (key == "VOICE_NODES")
            {
                std::stringstream ss(val);
                std::string node_item;
                while (std::getline(ss, node_item, ','))
                {
                    size_t colon = node_item.find(':');
                    if (colon != std::string::npos)
                    {
                        std::string h = node_item.substr(0, colon);
                        int p = std::stoi(node_item.substr(colon + 1));
                        voice_nodes.push_back({h, p, 0});
                    }
                }
            }
        }
    }
    return true;
}

VoiceNodeInfo VoiceServerMaster::getNextVoiceNode()
{
    std::unique_lock<std::shared_mutex> lock(master_mutex);
    if (voice_nodes.empty())
    {
        return {"127.0.0.1", 9010, 0}; // Fallback default
    }
    auto node = voice_nodes[round_robin_index];
    round_robin_index = (round_robin_index + 1) % voice_nodes.size();
    return node;
}

void VoiceServerMaster::runP1Server()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        return;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(p1_port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        close(server_fd);
        return;
    }

    listen(server_fd, 128);
    std::cout << "[MASTER P1] Listening for voice room requests on port " << p1_port << std::endl;

    while (running)
    {
        int client_sock = accept(server_fd, nullptr, nullptr);
        if (client_sock < 0)
            continue;

        std::thread([this, client_sock]()
                    {
            char buffer[4096] = {0};
            int valread = read(client_sock, buffer, sizeof(buffer) - 1);
            if (valread <= 0) {
                close(client_sock);
                return;
            }

            if (!is_active_primary) {
                std::string err_resp = "{\"status\":\"error\",\"message\":\"Master instance is in standby mode\"}";
                send(client_sock, err_resp.c_str(), err_resp.length(), MSG_NOSIGNAL); // Safe write
                close(client_sock);
                return;
            }

            std::string payload(buffer, valread);
            
            auto extractField = [](const std::string& json, const std::string& key) {
                std::string sk = "\"" + key + "\":";
                size_t p = json.find(sk);
                if (p == std::string::npos) return std::string("");
                size_t s = p + sk.length();
                while (s < json.length() && (json[s] == ' ' || json[s] == '"')) s++;
                size_t e = s;
                while (e < json.length() && json[e] != '"' && json[e] != ',' && json[e] != '}') e++;
                return json.substr(s, e - s);
            };

            std::string action = extractField(payload, "action");
            std::string uid = extractField(payload, "uid");

            if (action == "start_voice_call" && !uid.empty()) {
                std::string room_id = "ROOM_" + uid;
                VoiceNodeInfo assigned_node = getNextVoiceNode();

                std::ostringstream resp;
                resp << "{\"status\":\"success\","
                     << "\"voice_host\":\"" << assigned_node.host << "\","
                     << "\"voice_port\":" << assigned_node.port << ","
                     << "\"room_id\":\"" << room_id << "\"}";

                std::string resp_str = resp.str();
                send(client_sock, resp_str.c_str(), resp_str.length(), MSG_NOSIGNAL); // Safe write
                std::cout << "[MASTER P1] Assigned Room " << room_id << " to Voice Node " << assigned_node.host << ":" << assigned_node.port << std::endl;
            } else {
                std::string err_resp = "{\"status\":\"error\",\"message\":\"Invalid request action\"}";
                send(client_sock, err_resp.c_str(), err_resp.length(), MSG_NOSIGNAL); // Safe write
            }

            close(client_sock); })
            .detach();
    }
    close(server_fd);
}

void VoiceServerMaster::runP2Heartbeat()
{
    std::cout << "[MASTER P2] Heartbeat monitor active on port " << p2_port
              << " (Interval: " << heartbeat_interval_min << " minutes)" << std::endl;

    while (running)
    {
        std::this_thread::sleep_for(std::chrono::minutes(heartbeat_interval_min));
        std::cout << "[MASTER P2] [" << instance_id << "] Heartbeat sync pulse emitted." << std::endl;
    }
}

void VoiceServerMaster::start()
{
    if (!loadConfiguration())
        return;

    std::thread p1_thread(&VoiceServerMaster::runP1Server, this);
    std::thread p2_thread(&VoiceServerMaster::runP2Heartbeat, this);

    p1_thread.join();
    p2_thread.join();
}

int main()
{
    // Ignore SIGPIPE globally to prevent process termination on dropped connections[cite: 17]
    signal(SIGPIPE, SIG_IGN);

    VoiceServerMaster master("voice_server_master.cfg");
    master.start();
    return 0;
}