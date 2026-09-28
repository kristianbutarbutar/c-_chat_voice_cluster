#ifndef VOICE_SERVER_MASTER_HPP
#define VOICE_SERVER_MASTER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <thread>

struct VoiceNodeInfo
{
    std::string host;
    int port;
    int active_connections;
};

class VoiceServerMaster
{
private:
    std::string config_filepath;
    int p1_port; // Client voice request port
    int p2_port; // Heartbeat / HA sync port
    int heartbeat_interval_min;
    std::string instance_id;
    std::atomic<bool> is_active_primary;
    std::atomic<bool> running;

    std::vector<VoiceNodeInfo> voice_nodes;
    std::size_t round_robin_index;
    mutable std::shared_mutex master_mutex;

    bool loadConfiguration();
    void runP1Server();
    void runP2Heartbeat();
    VoiceNodeInfo getNextVoiceNode();

public:
    explicit VoiceServerMaster(std::string cfg_path);
    ~VoiceServerMaster();

    VoiceServerMaster(const VoiceServerMaster &) = delete;
    VoiceServerMaster &operator=(const VoiceServerMaster &) = delete;

    void start();
    void stop();
};

#endif // VOICE_SERVER_MASTER_HPP