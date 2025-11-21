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
 * reverse_ssh_service.hh - Reverse SSH tunnel service for remote access
 */

#ifndef REVERSE_SSH_SERVICE_HH
#define REVERSE_SSH_SERVICE_HH

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <memory>
#include <chrono>
#include "src/utils/vt_logger.hh"

namespace vt {

/**
 * ReverseSSHService - Manages reverse SSH tunnels for remote access
 *
 * This service creates SSH tunnels that allow remote access to ViewTouch
 * systems that are behind NAT/firewalls. The POS system initiates outbound
 * connections to a management server, which then allows inbound connections
 * back to the POS system.
 *
 * Features:
 * - Automatic tunnel establishment and maintenance
 * - Connection health monitoring
 * - Secure key-based authentication
 * - Configurable retry logic
 * - System integration with ViewTouch lifecycle
 */
class ReverseSSHService {
public:
    // Service status enumeration
    enum class ServiceStatus {
        STOPPED,
        STARTING,
        RUNNING,
        RECONNECTING,
        FAILED,
        DISABLED
    };

    // Configuration structure
    struct Configuration {
        bool enabled = false;
        std::string management_server;
        int management_port = 22;
        std::string remote_user;
        int local_port = 22;           // Local SSH port to expose
        int remote_port = 0;           // Remote port on management server (0 = auto-assign)
        std::string ssh_key_path;      // Path to SSH private key
        std::string known_hosts_path;  // Path to known_hosts file
        std::chrono::seconds reconnect_interval = std::chrono::seconds(30);
        std::chrono::seconds health_check_interval = std::chrono::seconds(60);
        int max_retry_attempts = 10;
        std::chrono::seconds retry_backoff = std::chrono::seconds(5);
        bool enable_compression = true;
        bool enable_keepalive = true;
        std::chrono::seconds server_alive_interval = std::chrono::seconds(60);
    };

    // Constructor/Destructor
    ReverseSSHService();
    ~ReverseSSHService();

    // Delete copy operations
    ReverseSSHService(const ReverseSSHService&) = delete;
    ReverseSSHService& operator=(const ReverseSSHService&) = delete;

    // Service lifecycle
    bool Initialize(const Configuration& config);
    bool Start();
    void Stop();
    bool Restart();

    // Status and monitoring
    ServiceStatus GetStatus() const;
    std::string GetStatusString(ServiceStatus status) const;
    bool IsHealthy() const;
    std::string GetLastError() const;
    std::string GetTunnelInfo() const;

    // Configuration
    bool UpdateConfiguration(const Configuration& new_config);
    const Configuration& GetConfiguration() const;

    // Health monitoring
    void PerformHealthCheck();
    std::string GetHealthReport() const;

private:
    // Internal state
    std::atomic<ServiceStatus> status_;
    Configuration config_;
    mutable std::mutex config_mutex_;
    std::unique_ptr<std::thread> tunnel_thread_;
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> tunnel_active_;

    // Error handling
    mutable std::string last_error_;
    mutable std::mutex error_mutex_;
    int consecutive_failures_;
    int assigned_remote_port_;

    // Tunnel management
    pid_t ssh_pid_;

    // Internal methods
    bool ValidateConfiguration() const;
    bool EstablishTunnel();
    void MonitorTunnel();
    void CleanupTunnel();
    bool CheckTunnelHealth() const;
    bool GenerateSSHCommand(std::string& command) const;
    bool SetupSSHKeys();
    bool TestSSHConnection() const;
    void LogStatusChange(ServiceStatus old_status, ServiceStatus new_status);
    void HandleTunnelFailure();
    std::string ExecuteCommand(const std::string& command) const;

    // Error handling
    void SetError(const std::string& error) const;

    // Logging helpers
    void LogInfo(const std::string& message) const;
    void LogWarning(const std::string& message) const;
    void LogError(const std::string& message) const;
};

// Global service instance
extern std::unique_ptr<ReverseSSHService> GlobalReverseSSHService;

} // namespace vt

#endif // REVERSE_SSH_SERVICE_HH
