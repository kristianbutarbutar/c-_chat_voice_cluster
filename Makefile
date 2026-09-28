# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread

# Executable Targets
TARGET_MASTER = voice_server_master
TARGET_VOICE = chat_server_voice_node
TARGET_MONITOR = mon_chat_node_cluster

# Source Files & Object Files
SRC_MASTER = voice_server_master.cpp
SRC_VOICE_NODE = chat_server_voice.cpp main_voice_node.cpp
SRC_MONITOR = mon_chat_node_cluster.cpp

# Detect OS and Homebrew Path for PostgreSQL (libpq)
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    # macOS Homebrew Paths
    PG_PREFIX := $(shell brew --prefix libpq 2>/dev/null || brew --prefix postgresql 2>/dev/null || echo /usr/local)
    PG_INCLUDE = -I$(PG_PREFIX)/include
    PG_LIB = -L$(PG_PREFIX)/lib -lpq
else
    # Linux Paths (Ubuntu / Debian)
    PG_INCLUDE = 
    PG_LIB = -lpq
endif

all: $(TARGET_MASTER) $(TARGET_VOICE) $(TARGET_MONITOR)
	@echo "All cluster binaries compiled successfully!"

# Build Voice Server Master Controller
$(TARGET_MASTER): $(SRC_MASTER)
	@echo "Building Master Controller..."
	$(CXX) $(CXXFLAGS) $(SRC_MASTER) -o $(TARGET_MASTER)

# Build Chat Server Voice Node (Requires a main wrapper if compiled standalone)
$(TARGET_VOICE): $(SRC_VOICE_NODE)
	@echo "Building Chat Server Voice Node..."
	$(CXX) $(CXXFLAGS) $(SRC_VOICE_NODE) -o $(TARGET_VOICE)

# Build PostgreSQL Cluster Monitor Daemon
$(TARGET_MONITOR): $(SRC_MONITOR)
	@echo "Building Cluster Monitor Daemon..."
	$(CXX) $(CXXFLAGS) $(SRC_MONITOR) -o $(TARGET_MONITOR) $(PG_INCLUDE) $(PG_LIB)

clean:
	@echo "Cleaning up binaries..."
	rm -f $(TARGET_MASTER) $(TARGET_VOICE) $(TARGET_MONITOR)

.PHONY: all clean