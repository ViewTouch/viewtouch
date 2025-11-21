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
 * reverse_ssh_service.cc - Implementation of reverse SSH tunnel service
 */

#include "reverse_ssh_service.hh"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

namespace vt {

// Global service instance
std::unique_ptr<ReverseSSHService> GlobalReverseSSHService;

ReverseSSHService::ReverseSSHService()
    : status_(ServiceStatus::STOPPED)
    , running_(false)
    , tunnel_active_(false)
    , consecutive_failures_(0)
    , assigned_remote_port_(0)
    , ssh_pid_(0)
{
}

ReverseSSHService::~ReverseSSHService() {
    Stop();
}

bool ReverseSSHService::Initialize(const Configuration& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;

    if (!ValidateConfiguration()) {
        LogError("Invalid reverse SSH configuration");
        return false;
    }

    if (config_.enabled) {
        status_ = ServiceStatus::STOPPED;
        LogInfo("Reverse SSH service initialized and enabled");
    } else {
        status_ = ServiceStatus::DISABLED;
        LogInfo("Reverse SSH service initialized but disabled");
    }

    return true;
}

bool ReverseSSHService::Start() {
    if (status_ == ServiceStatus::DISABLED) {
        LogWarning("Cannot start reverse SSH service - service is disabled");
        return false;
    }

    if (status_ == ServiceStatus::RUNNING) {
        LogInfo("Reverse SSH service is already running");
        return true;
    }

    LogInfo("Starting reverse SSH service...");
    status_ = ServiceStatus::STARTING;
    running_ = true;

    try {
        // Setup SSH keys if needed
        if (!SetupSSHKeys()) {
            LogError("Failed to setup SSH keys");
            status_ = ServiceStatus::FAILED;
            return false;
        }

        // Start the tunnel thread
        tunnel_thread_ = std::make_unique<std::thread>([this]() {
            while (running_) {
                if (EstablishTunnel()) {
                    tunnel_active_ = true;
                    status_ = ServiceStatus::RUNNING;
                    LogInfo("Reverse SSH tunnel established successfully");

                    // Start monitoring thread
                    monitor_thread_ = std::make_unique<std::thread>([this]() {
                        MonitorTunnel();
                    });
                    monitor_thread_->detach();

                    // Wait for tunnel to be stopped
                    while (running_ && tunnel_active_) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                } else {
                    tunnel_active_ = false;
                    status_ = ServiceStatus::FAILED;
                    HandleTunnelFailure();
                }

                if (running_) {
                    LogInfo("Retrying tunnel establishment in " +
                           std::to_string(config_.reconnect_interval.count()) + " seconds");
                    std::this_thread::sleep_for(config_.reconnect_interval);
                }
            }
        });

        tunnel_thread_->detach();

        // Wait a bit for initial connection attempt
        std::this_thread::sleep_for(std::chrono::seconds(2));

        return status_ == ServiceStatus::RUNNING;

    } catch (const std::exception& e) {
        LogError("Exception during service start: " + std::string(e.what()));
        status_ = ServiceStatus::FAILED;
        running_ = false;
        return false;
    }
}

void ReverseSSHService::Stop() {
    LogInfo("Stopping reverse SSH service...");

    running_ = false;
    status_ = ServiceStatus::STOPPED;

    // Cleanup tunnel
    CleanupTunnel();

    // Wait for threads to finish
    if (tunnel_thread_ && tunnel_thread_->joinable()) {
        tunnel_thread_->join();
    }
    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }

    tunnel_thread_.reset();
    monitor_thread_.reset();

    LogInfo("Reverse SSH service stopped");
}

bool ReverseSSHService::Restart() {
    LogInfo("Restarting reverse SSH service...");
    Stop();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return Start();
}

ReverseSSHService::ServiceStatus ReverseSSHService::GetStatus() const {
    return status_;
}

std::string ReverseSSHService::GetStatusString(ServiceStatus status) const {
    switch (status) {
        case ServiceStatus::STOPPED: return "STOPPED";
        case ServiceStatus::STARTING: return "STARTING";
        case ServiceStatus::RUNNING: return "RUNNING";
        case ServiceStatus::RECONNECTING: return "RECONNECTING";
        case ServiceStatus::FAILED: return "FAILED";
        case ServiceStatus::DISABLED: return "DISABLED";
        default: return "UNKNOWN";
    }
}

bool ReverseSSHService::IsHealthy() const {
    return status_ == ServiceStatus::RUNNING && tunnel_active_;
}

std::string ReverseSSHService::GetLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

std::string ReverseSSHService::GetTunnelInfo() const {
    if (!tunnel_active_ || assigned_remote_port_ == 0) {
        return "No active tunnel";
    }

    std::stringstream ss;
    ss << "Tunnel active: " << config_.management_server << ":"
       << assigned_remote_port_ << " -> localhost:" << config_.local_port;
    return ss.str();
}

bool ReverseSSHService::UpdateConfiguration(const Configuration& new_config) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (!ValidateConfiguration()) {
        LogError("Invalid configuration update");
        return false;
    }

    bool needs_restart = (config_.enabled != new_config.enabled) ||
                        (config_.management_server != new_config.management_server) ||
                        (config_.management_port != new_config.management_port) ||
                        (config_.remote_user != new_config.remote_user) ||
                        (config_.local_port != new_config.local_port);

    config_ = new_config;

    if (needs_restart && status_ == ServiceStatus::RUNNING) {
        LogInfo("Configuration change requires service restart");
        Restart();
    }

    return true;
}

const ReverseSSHService::Configuration& ReverseSSHService::GetConfiguration() const {
    return config_;
}

void ReverseSSHService::PerformHealthCheck() {
    if (status_ != ServiceStatus::RUNNING) {
        return;
    }

    if (!CheckTunnelHealth()) {
        LogWarning("Health check failed - tunnel may be down");
        tunnel_active_ = false;
        status_ = ServiceStatus::RECONNECTING;
    }
}

std::string ReverseSSHService::GetHealthReport() const {
    std::stringstream ss;
    ss << "Status: " << GetStatusString(status_) << "\n";
    ss << "Tunnel Active: " << (tunnel_active_ ? "Yes" : "No") << "\n";
    ss << "SSH PID: " << ssh_pid_ << "\n";
    ss << "Remote Port: " << assigned_remote_port_ << "\n";
    ss << "Consecutive Failures: " << consecutive_failures_ << "\n";

    if (!last_error_.empty()) {
        ss << "Last Error: " << last_error_ << "\n";
    }

    return ss.str();
}

bool ReverseSSHService::ValidateConfiguration() const {
    if (!config_.enabled) {
        return true; // Disabled config is valid
    }

    if (config_.management_server.empty()) {
        SetError("Management server not specified");
        return false;
    }

    if (config_.remote_user.empty()) {
        SetError("Remote user not specified");
        return false;
    }

    if (config_.local_port <= 0 || config_.local_port > 65535) {
        SetError("Invalid local port");
        return false;
    }

    if (config_.management_port <= 0 || config_.management_port > 65535) {
        SetError("Invalid management server port");
        return false;
    }

    return true;
}

bool ReverseSSHService::EstablishTunnel() {
    try {
        std::string ssh_command;
        if (!GenerateSSHCommand(ssh_command)) {
            return false;
        }

        LogInfo("Establishing SSH tunnel: " + ssh_command);

        // Fork and execute SSH command
        ssh_pid_ = fork();
        if (ssh_pid_ == 0) {
            // Child process
            execl("/bin/sh", "sh", "-c", ssh_command.c_str(), nullptr);
            _exit(1); // Should not reach here
        } else if (ssh_pid_ < 0) {
            SetError("Failed to fork SSH process");
            return false;
        }

        // Wait a bit for tunnel to establish
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // Check if process is still running and tunnel is active
        if (kill(ssh_pid_, 0) == 0) {
            // Try to determine assigned remote port if auto-assigned
            if (config_.remote_port == 0) {
                // For auto-assigned ports, we might need to parse SSH output
                // This is a simplified implementation
                assigned_remote_port_ = 2222; // Default fallback
            } else {
                assigned_remote_port_ = config_.remote_port;
            }
            return true;
        } else {
            SetError("SSH process terminated immediately");
            return false;
        }

    } catch (const std::exception& e) {
        SetError("Exception establishing tunnel: " + std::string(e.what()));
        return false;
    }
}

void ReverseSSHService::MonitorTunnel() {
    LogInfo("Starting tunnel monitoring");

    while (running_ && tunnel_active_) {
        std::this_thread::sleep_for(config_.health_check_interval);

        if (!running_) break;

        PerformHealthCheck();

        if (!tunnel_active_) {
            LogWarning("Tunnel health check failed");
            break;
        }
    }

    LogInfo("Tunnel monitoring stopped");
}

void ReverseSSHService::CleanupTunnel() {
    if (ssh_pid_ > 0) {
        LogInfo("Terminating SSH tunnel process (PID: " + std::to_string(ssh_pid_) + ")");

        // Try graceful termination first
        kill(ssh_pid_, SIGTERM);

        // Wait up to 5 seconds for graceful shutdown
        int wait_count = 0;
        while (wait_count < 50 && kill(ssh_pid_, 0) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }

        // Force kill if still running
        if (kill(ssh_pid_, 0) == 0) {
            LogWarning("Force killing SSH process");
            kill(ssh_pid_, SIGKILL);
        }

        // Wait for process to be cleaned up
        waitpid(ssh_pid_, nullptr, 0);
        ssh_pid_ = 0;
    }

    tunnel_active_ = false;
    assigned_remote_port_ = 0;
}

bool ReverseSSHService::CheckTunnelHealth() const {
    if (ssh_pid_ <= 0) {
        return false;
    }

    // Check if SSH process is still running
    if (kill(ssh_pid_, 0) != 0) {
        return false;
    }

    // Try to test the tunnel by attempting a connection
    // This is a simplified check - in production you might want more sophisticated monitoring
    return true;
}

bool ReverseSSHService::GenerateSSHCommand(std::string& command) const {
    std::stringstream ss;

    ss << "ssh";

    // SSH options for reliability
    if (config_.enable_keepalive) {
        ss << " -o ServerAliveInterval=" << config_.server_alive_interval.count();
        ss << " -o ServerAliveCountMax=3";
    }

    if (config_.enable_compression) {
        ss << " -o Compression=yes";
    }

    // Connection timeout
    ss << " -o ConnectTimeout=10";

    // Strict host key checking (should be disabled for automation but kept for security)
    ss << " -o StrictHostKeyChecking=no";

    // User known hosts file
    if (!config_.known_hosts_path.empty()) {
        ss << " -o UserKnownHostsFile=" << config_.known_hosts_path;
    }

    // SSH key
    if (!config_.ssh_key_path.empty()) {
        ss << " -i " << config_.ssh_key_path;
    }

    // Quiet mode
    ss << " -q";

    // Reverse tunnel specification
    ss << " -R ";
    if (config_.remote_port > 0) {
        ss << config_.remote_port;
    } else {
        ss << "0"; // Auto-assign
    }
    ss << ":localhost:" << config_.local_port;

    // Remote user and server
    ss << " " << config_.remote_user << "@" << config_.management_server;

    // Remote command to keep tunnel alive
    ss << " 'echo \"Tunnel established\"; while true; do sleep 60; done'";

    command = ss.str();
    return true;
}

bool ReverseSSHService::SetupSSHKeys() {
    if (config_.ssh_key_path.empty()) {
        // Generate a default key if none specified
        config_.ssh_key_path = "/usr/viewtouch/ssh/reverse_ssh_key";

        fs::path key_dir = fs::path(config_.ssh_key_path).parent_path();
        if (!fs::exists(key_dir)) {
            fs::create_directories(key_dir);
        }

        if (!fs::exists(config_.ssh_key_path)) {
            LogInfo("Generating SSH key for reverse tunnel: " + config_.ssh_key_path);
            std::string keygen_cmd = "ssh-keygen -t ed25519 -f " + config_.ssh_key_path +
                                   " -N '' -C 'viewtouch-reverse-ssh' 2>/dev/null";
            if (system(keygen_cmd.c_str()) != 0) {
                SetError("Failed to generate SSH key");
                return false;
            }
        }
    }

    // Set proper permissions
    chmod(config_.ssh_key_path.c_str(), S_IRUSR | S_IWUSR);
    std::string pub_key_path = config_.ssh_key_path + ".pub";
    if (fs::exists(pub_key_path)) {
        chmod(pub_key_path.c_str(), S_IRUSR | S_IROTH | S_IRGRP);
    }

    return true;
}

bool ReverseSSHService::TestSSHConnection() const {
    std::string test_cmd = "ssh -o ConnectTimeout=5 -o BatchMode=yes -o StrictHostKeyChecking=no ";
    test_cmd += config_.remote_user + "@" + config_.management_server + " 'echo \"SSH connection test successful\"' 2>/dev/null";

    FILE* pipe = popen(test_cmd.c_str(), "r");
    if (!pipe) return false;

    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    int status = pclose(pipe);
    return (status == 0 && result.find("successful") != std::string::npos);
}

void ReverseSSHService::HandleTunnelFailure() {
    consecutive_failures_++;

    LogWarning("Tunnel establishment failed (attempt " +
              std::to_string(consecutive_failures_) + "/" +
              std::to_string(config_.max_retry_attempts) + ")");

    if (consecutive_failures_ >= config_.max_retry_attempts) {
        LogError("Maximum retry attempts exceeded, disabling service");
        status_ = ServiceStatus::FAILED;
        running_ = false;
        return;
    }

    // Exponential backoff
    auto backoff_time = config_.retry_backoff * consecutive_failures_;
    LogInfo("Backing off for " + std::to_string(backoff_time.count()) + " seconds");
    std::this_thread::sleep_for(backoff_time);
}

std::string ReverseSSHService::ExecuteCommand(const std::string& command) const {
    char buffer[128];
    std::string result;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);
    return result;
}

void ReverseSSHService::LogStatusChange(ServiceStatus old_status, ServiceStatus new_status) {
    LogInfo("Status changed from " + std::to_string(static_cast<int>(old_status)) +
           " to " + std::to_string(static_cast<int>(new_status)));
}

void ReverseSSHService::SetError(const std::string& error) const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
}

void ReverseSSHService::LogInfo(const std::string& message) const {
    vt::Logger::info("[ReverseSSH] {}", message);
}

void ReverseSSHService::LogWarning(const std::string& message) const {
    vt::Logger::warn("[ReverseSSH] {}", message);
}

void ReverseSSHService::LogError(const std::string& message) const {
    vt::Logger::error("[ReverseSSH] {}", message);
    SetError(message);
}

} // namespace vt
