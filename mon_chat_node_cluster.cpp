#include "mon_chat_node_cluster.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdio>
#include <array>
#include <memory>
#include <libpq-fe.h>

ClusterMonitorNode::ClusterMonitorNode(std::string cfg_path)
    : config_filepath(std::move(cfg_path)), interval_minutes(5), running(true) {}

ClusterMonitorNode::~ClusterMonitorNode()
{
    stop();
}

void ClusterMonitorNode::stop()
{
    running = false;
}

bool ClusterMonitorNode::loadConfiguration()
{
    std::ifstream file(config_filepath);
    if (!file.is_open())
    {
        std::cerr << "[MONITOR ERROR] Failed to open config file: " << config_filepath << std::endl;
        return false;
    }

    apps.clear();
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
            if (key == "INTERVAL_MINUTES")
            {
                interval_minutes = std::stoi(val);
            }
            else if (key == "DB_CONNECTION")
            {
                db_connection_str = val;
            }
            continue;
        }

        size_t pipe_pos = line.find('|');
        if (pipe_pos != std::string::npos)
        {
            MonitoredApp app;
            app.app_name = line.substr(0, pipe_pos);
            app.command = line.substr(pipe_pos + 1);

            app.app_name.erase(app.app_name.find_last_not_of(" \t") + 1);
            app.command.erase(0, app.command.find_first_not_of(" \t"));

            apps.push_back(app);
        }
    }
    return true;
}

bool ClusterMonitorNode::logToDatabase(const std::string &timestamp, const std::string &app_name, const std::string &status, int pid, const std::string &description, bool is_running)
{
    PGconn *conn = PQconnectdb(db_connection_str.c_str());
    if (PQstatus(conn) != CONNECTION_OK)
    {
        std::cerr << "[MONITOR DB ERROR] Connection failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return false;
    }

    const char *create_table_query =
        "CREATE TABLE IF NOT EXISTS t_mon_chat_node_cluster ("
        "id SERIAL PRIMARY KEY, "
        "scan_timestamp TIMESTAMP, "
        "application_name VARCHAR(255), "
        "status VARCHAR(50), "
        "pid INT, "
        "description TEXT, "
        "isrunning BOOLEAN);";

    PGresult *res = PQexec(conn, create_table_query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        std::cerr << "[MONITOR DB ERROR] Table creation failed: " << PQerrorMessage(conn) << std::endl;
    }
    PQclear(res);

    std::string query = "INSERT INTO t_mon_chat_node_cluster (scan_timestamp, application_name, status, pid, description, isrunning) VALUES ($1, $2, $3, $4, $5, $6);";
    std::string pid_str = std::to_string(pid);
    std::string is_running_str = is_running ? "true" : "false";

    const char *paramValues[6] = {
        timestamp.c_str(),
        app_name.c_str(),
        status.c_str(),
        pid_str.c_str(),
        description.c_str(),
        is_running_str.c_str()};

    res = PQexecParams(conn, query.c_str(), 6, NULL, paramValues, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        std::cerr << "[MONITOR DB ERROR] Insert failed: " << PQerrorMessage(conn) << std::endl;
    }

    PQclear(res);
    PQfinish(conn);
    return true;
}

std::pair<bool, int> ClusterMonitorNode::checkProcessRunning(const std::string &app_name)
{
    std::string cmd = "ps -ef | grep \"" + app_name + "\" | grep -v grep";
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe)
    {
        return {false, -1};
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }

    if (result.empty())
    {
        return {false, -1};
    }

    std::istringstream iss(result);
    std::string user;
    int pid = -1;
    if (iss >> user >> pid)
    {
        return {true, pid};
    }

    return {true, -1};
}

void ClusterMonitorNode::checkAndRestartApplications()
{
    if (!loadConfiguration())
        return;

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    char time_buf[100];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));
    std::string scan_time(time_buf);

    for (const auto &app : apps)
    {
        auto [is_running, pid] = checkProcessRunning(app.app_name);

        if (is_running)
        {
            std::cout << "[MONITOR] App running: " << app.app_name << " (PID: " << pid << ")" << std::endl;
            logToDatabase(scan_time, app.app_name, "start", pid, "Application is running normally", true);
        }
        else
        {
            std::cout << "[MONITOR] App DOWN! Attempting restart: " << app.app_name << std::endl;
            logToDatabase(scan_time, app.app_name, "off", -1, "Application not found in ps -ef, restarting...", false);

            std::string exec_cmd = app.command + " &";
            int exec_result = system(exec_cmd.c_str());
            if (exec_result == 0)
            {
                std::cout << "[MONITOR] Restart command executed successfully for: " << app.app_name << std::endl;
            }
            else
            {
                std::cerr << "[MONITOR ERROR] Failed to execute restart command for: " << app.app_name << std::endl;
            }
        }
    }
}

void ClusterMonitorNode::start()
{
    std::cout << "[MONITOR NODE] Started monitoring cluster applications every " << interval_minutes << " minute(s)." << std::endl;
    while (running)
    {
        checkAndRestartApplications();
        std::this_thread::sleep_for(std::chrono::minutes(interval_minutes));
    }
}

int main()
{
    ClusterMonitorNode monitor("mon_chat_node_cluster.cfg");
    monitor.start();
    return 0;
}