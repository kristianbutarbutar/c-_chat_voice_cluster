#ifndef MON_CHAT_NODE_CLUSTER_HPP
#define MON_CHAT_NODE_CLUSTER_HPP

#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <utility>

struct MonitoredApp
{
    std::string app_name;
    std::string command;
};

class ClusterMonitorNode
{
private:
    std::string config_filepath;
    int interval_minutes;
    std::string db_connection_str;
    std::vector<MonitoredApp> apps;
    std::atomic<bool> running;

    bool loadConfiguration();
    bool logToDatabase(const std::string &timestamp, const std::string &app_name, const std::string &status, int pid, const std::string &description, bool is_running);
    void checkAndRestartApplications();
    std::pair<bool, int> checkProcessRunning(const std::string &app_name);

public:
    explicit ClusterMonitorNode(std::string cfg_path);
    ~ClusterMonitorNode();

    ClusterMonitorNode(const ClusterMonitorNode &) = delete;
    ClusterMonitorNode &operator=(const ClusterMonitorNode &) = delete;

    void start();
    void stop();
};

#endif // MON_CHAT_NODE_CLUSTER_HPP