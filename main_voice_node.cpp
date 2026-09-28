#include "chat_server_voice.hpp"
#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char *argv[])
{
    int port = 9010; // Default fallback port
    std::string config_file = "chat_server_voice.cfg";

    if (argc > 1)
    {
        config_file = argv[1];
    }

    // Read VOICE_PORT from configuration file
    std::ifstream file(config_file);
    if (file.is_open())
    {
        std::string line;
        while (std::getline(file, line))
        {
            // Trim whitespace
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
                }
            }
        }
        std::cout << "[VOICE NODE] Loaded configuration from " << config_file << " (Port: " << port << ")" << std::endl;
    }
    else
    {
        std::cout << "[VOICE NODE] Config file '" << config_file << "' not found. Using default port: " << port << std::endl;
    }

    // Start the voice streaming server node
    ChatServerVoiceNode voiceNode(port);
    voiceNode.run();

    return 0;
}