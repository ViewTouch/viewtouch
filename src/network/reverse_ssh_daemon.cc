/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * reverse_ssh_daemon.cc - Standalone reverse SSH tunnel daemon
 */

#include "reverse_ssh_service.hh"
#include "src/utils/vt_logger.hh"
#include "basic.hh"
#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <filesystem>

namespace fs = std::filesystem;

// Global variables for signal handling
volatile bool running = true;
std::unique_ptr<vt::ReverseSSHService> ssh_service;

// Signal handler
void signal_handler(int signum) {
    vt::Logger::info("[ReverseSSH Daemon] Received signal {}, shutting down...", signum);
    running = false;
    if (ssh_service) {
        ssh_service->Stop();
    }
}

// Configuration file structure
struct DaemonConfig {
    std::string management_server;
    int management_port = 22;
    std::string remote_user;
    int local_port = 22;
    int remote_port = 0;
    std::string ssh_key_path;
    int reconnect_interval = 30;
    int health_check_interval = 60;
    int max_retries = 10;
    std::string log_file = "/var/log/viewtouch/reverse_ssh_daemon.log";
    std::string pid_file = "/var/run/viewtouch/reverse_ssh_daemon.pid";
    bool daemonize = true;
};

// Load configuration from file
bool load_config(const std::string& config_file, DaemonConfig& config) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_file << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // Trim whitespace
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (key == "management_server") config.management_server = value;
            else if (key == "management_port") config.management_port = std::stoi(value);
            else if (key == "remote_user") config.remote_user = value;
            else if (key == "local_port") config.local_port = std::stoi(value);
            else if (key == "remote_port") config.remote_port = std::stoi(value);
            else if (key == "ssh_key_path") config.ssh_key_path = value;
            else if (key == "reconnect_interval") config.reconnect_interval = std::stoi(value);
            else if (key == "health_check_interval") config.health_check_interval = std::stoi(value);
            else if (key == "max_retries") config.max_retries = std::stoi(value);
            else if (key == "log_file") config.log_file = value;
            else if (key == "pid_file") config.pid_file = value;
            else if (key == "daemonize") config.daemonize = (value == "true" || value == "1");
        }
    }

    return true;
}

// Validate configuration
bool validate_config(const DaemonConfig& config) {
    if (config.management_server.empty()) {
        std::cerr << "Error: management_server not specified" << std::endl;
        return false;
    }
    if (config.remote_user.empty()) {
        std::cerr << "Error: remote_user not specified" << std::endl;
        return false;
    }
    if (config.local_port <= 0 || config.local_port > 65535) {
        std::cerr << "Error: invalid local_port" << std::endl;
        return false;
    }
    return true;
}

// Daemonize the process
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Failed to fork daemon process" << std::endl;
        exit(1);
    }
    if (pid > 0) {
        exit(0); // Parent exits
    }

    // Child continues
    if (setsid() < 0) {
        std::cerr << "Failed to create new session" << std::endl;
        exit(1);
    }

    // Change working directory
    if (chdir("/") < 0) {
        std::cerr << "Failed to change working directory" << std::endl;
        exit(1);
    }

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

// Write PID file
bool write_pid_file(const std::string& pid_file) {
    std::ofstream file(pid_file);
    if (!file.is_open()) {
        std::cerr << "Failed to write PID file: " << pid_file << std::endl;
        return false;
    }
    file << getpid() << std::endl;
    return true;
}

// Remove PID file
void remove_pid_file(const std::string& pid_file) {
    unlink(pid_file.c_str());
}

// Print usage information
void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c, --config FILE    Configuration file (default: /etc/viewtouch/reverse_ssh.conf)" << std::endl;
    std::cout << "  -f, --foreground     Run in foreground (don't daemonize)" << std::endl;
    std::cout << "  -h, --help           Show this help message" << std::endl;
    std::cout << "  -v, --version        Show version information" << std::endl;
    std::cout << std::endl;
    std::cout << "Configuration file format:" << std::endl;
    std::cout << "  management_server=hostname" << std::endl;
    std::cout << "  management_port=22" << std::endl;
    std::cout << "  remote_user=username" << std::endl;
    std::cout << "  local_port=22" << std::endl;
    std::cout << "  remote_port=0" << std::endl;
    std::cout << "  ssh_key_path=/path/to/key" << std::endl;
    std::cout << "  reconnect_interval=30" << std::endl;
    std::cout << "  health_check_interval=60" << std::endl;
    std::cout << "  max_retries=10" << std::endl;
    std::cout << "  log_file=/var/log/viewtouch/reverse_ssh_daemon.log" << std::endl;
    std::cout << "  pid_file=/var/run/viewtouch/reverse_ssh_daemon.pid" << std::endl;
    std::cout << "  daemonize=true" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file = "/etc/viewtouch/reverse_ssh.conf";
    bool foreground = false;

    // Parse command line options
    struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"foreground", no_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;
    while ((c = getopt_long(argc, argv, "c:fhv", long_options, &option_index)) != -1) {
        switch (c) {
            case 'c':
                config_file = optarg;
                break;
            case 'f':
                foreground = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                std::cout << "ViewTouch Reverse SSH Daemon v1.0" << std::endl;
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Load configuration
    DaemonConfig config;
    if (!load_config(config_file, config)) {
        return 1;
    }

    // Override daemonize if foreground requested
    if (foreground) {
        config.daemonize = false;
    }

    // Validate configuration
    if (!validate_config(config)) {
        return 1;
    }

    // Initialize logging
    try {
        if (config.daemonize) {
            vt::Logger::Initialize("/var/log/viewtouch", "info", false, false);
        } else {
            vt::Logger::Initialize("/var/log/viewtouch", "debug", true, true);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logging: " << e.what() << std::endl;
        return 1;
    }

    vt::Logger::info("[ReverseSSH Daemon] Starting ViewTouch Reverse SSH Daemon v1.0");
    vt::Logger::info("[ReverseSSH Daemon] Configuration file: {}", config_file);

    // Daemonize if requested
    if (config.daemonize) {
        vt::Logger::info("[ReverseSSH Daemon] Daemonizing...");
        daemonize();
    }

    // Write PID file
    if (!write_pid_file(config.pid_file)) {
        vt::Logger::error("[ReverseSSH Daemon] Failed to write PID file");
        return 1;
    }

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    // Initialize reverse SSH service
    try {
        vt::ReverseSSHService::Configuration ssh_config;
        ssh_config.enabled = true;
        ssh_config.management_server = config.management_server;
        ssh_config.management_port = config.management_port;
        ssh_config.remote_user = config.remote_user;
        ssh_config.local_port = config.local_port;
        ssh_config.remote_port = config.remote_port;
        ssh_config.ssh_key_path = config.ssh_key_path;
        ssh_config.reconnect_interval = std::chrono::seconds(config.reconnect_interval);
        ssh_config.health_check_interval = std::chrono::seconds(config.health_check_interval);
        ssh_config.max_retry_attempts = config.max_retries;

        ssh_service = std::make_unique<vt::ReverseSSHService>();
        ssh_service->Initialize(ssh_config);

        vt::Logger::info("[ReverseSSH Daemon] Starting reverse SSH service...");
        if (!ssh_service->Start()) {
            vt::Logger::error("[ReverseSSH Daemon] Failed to start reverse SSH service");
            remove_pid_file(config.pid_file);
            return 1;
        }

    } catch (const std::exception& e) {
        vt::Logger::error("[ReverseSSH Daemon] Exception during initialization: {}", e.what());
        remove_pid_file(config.pid_file);
        return 1;
    }

    vt::Logger::info("[ReverseSSH Daemon] Reverse SSH daemon started successfully");
    vt::Logger::info("[ReverseSSH Daemon] PID: {}", getpid());
    vt::Logger::info("[ReverseSSH Daemon] Tunnel: {}:{} -> localhost:{}",
                    config.management_server, config.remote_port, config.local_port);

    // Main loop
    while (running) {
        sleep(1);
    }

    // Cleanup
    vt::Logger::info("[ReverseSSH Daemon] Shutting down...");
    if (ssh_service) {
        ssh_service->Stop();
    }
    remove_pid_file(config.pid_file);
    vt::Logger::Shutdown();

    vt::Logger::info("[ReverseSSH Daemon] Shutdown complete");
    return 0;
}
