#include "chat_server_voice.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <csignal> // Required for signal handling

int main(int argc, char *argv[])
{
    // Ignore SIGPIPE globally to prevent abrupt termination on broken socket connections[cite: 13]
    signal(SIGPIPE, SIG_IGN);

    int port = 9010; // Default fallback port[cite: 13]
    std::string config_file = "chat_server_voice.cfg";
    [cite:13]

        if (argc > 1)
    {
        config_file = argv[1];
        [cite:13]
    }

    // Read VOICE_PORT from configuration file[cite: 13]
    std::ifstream file(config_file);
    if (file.is_open())
    {
        std::string line;
        while (std::getline(file, line))
        {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == '#')
                continue;

            size_t eq = line.find('=');
            if (eq != std::string::npos)
            {
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                if (key == "VOICE_PORT")
                {
                    port = std::stoi(val);
                    [cite:13]
                }
            }
        }
        std::cout << "[VOICE NODE] Loaded configuration from " << config_file << " (Port: " << port << ")" << std::endl;
        [cite:13]
    }
    else
    {
        std::cout << "[VOICE NODE] Config file '" << config_file << "' not found. Using default port: " << port << std::endl;
        [cite:13]
    }

    // Start the voice streaming server node[cite: 13]
    ChatServerVoiceNode voiceNode(port);
    [cite:13] voiceNode.run();
    [cite:13]

        return 0;
}