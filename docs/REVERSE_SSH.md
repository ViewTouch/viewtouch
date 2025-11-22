# ViewTouch Reverse SSH Tunnel

This document describes the reverse SSH tunnel functionality in ViewTouch, which enables remote access to POS systems behind firewalls or NAT devices.

## Overview

Reverse SSH tunneling allows a device behind a firewall to initiate an outbound SSH connection to a management server, creating a secure tunnel that enables inbound connections back to the device. This is essential for remote support, monitoring, and management of ViewTouch POS systems.

## Architecture

ViewTouch provides two ways to implement reverse SSH tunnels:

1. **Integrated Service**: Built into the main ViewTouch application (vt_main)
2. **Standalone Daemon**: Independent reverse SSH daemon (reverse_ssh_daemon)

### Integrated Service

The integrated service runs as part of the ViewTouch application and **always starts automatically** when ViewTouch starts. It is configured through the ViewTouch settings interface and automatically stops when the POS system shuts down. The service is always enabled and will attempt to establish reverse SSH tunnels using the configured parameters.

### Standalone Daemon

The standalone daemon runs independently of ViewTouch and can be managed via systemd. This is useful for scenarios where remote access is needed even when ViewTouch is not running.

## Configuration

### Integrated Service Configuration

Configure reverse SSH settings through the ViewTouch settings interface:

- **Enable Reverse SSH**: Enable/disable the reverse SSH tunnel
- **Management Server**: Hostname/IP of the management server
- **Management Port**: SSH port on the management server (default: 22)
- **Remote User**: SSH username on the management server
- **Local Port**: Local port to expose (typically 22 for SSH access)
- **Remote Port**: Port on management server for tunnel (0 = auto-assign)
- **SSH Key Path**: Path to SSH private key file
- **Reconnect Interval**: Seconds between reconnection attempts
- **Health Check Interval**: Seconds between health checks
- **Max Retries**: Maximum reconnection attempts before giving up

### Standalone Daemon Configuration

Edit `/etc/viewtouch/reverse_ssh.conf` (installed automatically with ViewTouch):

```bash
# Management server details
management_server=management.viewtouch.com
management_port=22
remote_user=viewtouch

# Tunnel configuration
local_port=22
remote_port=0

# SSH authentication
ssh_key_path=/usr/viewtouch/ssh/reverse_ssh_key

# Connection settings
reconnect_interval=30
health_check_interval=60
max_retries=10

# Daemon settings
log_file=/var/log/viewtouch/reverse_ssh_daemon.log
pid_file=/var/run/viewtouch/reverse_ssh_daemon.pid
daemonize=true
```

## Setup Instructions

### Option A: Using Your Own Computer as Management Server

If you don't have a dedicated management server, you can use your own computer:

#### 1. Make Your Computer Accessible
```bash
# Install SSH server if not already installed
sudo apt install openssh-server  # Ubuntu/Debian
sudo dnf install openssh-server  # Fedora/RHEL
sudo pacman -S openssh           # Arch

# Start and enable SSH service
sudo systemctl start sshd
sudo systemctl enable sshd

# Find your public IP (if accessible from internet)
curl ifconfig.me
# OR
curl icanhazip.com
```

#### 2. Configure SSH Server
```bash
# Edit SSH config
sudo nano /etc/ssh/sshd_config

# Ensure these settings:
PubkeyAuthentication yes
PasswordAuthentication no  # For security
PermitTunnel yes
GatewayPorts yes

# Restart SSH
sudo systemctl restart sshd
```

#### 3. Configure ViewTouch Reverse SSH
Set in `/etc/viewtouch/reverse_ssh.conf`:
```bash
management_server=your.public.ip.address
management_port=22
remote_user=your_username
local_port=22
remote_port=2222  # Or 0 for auto-assign
ssh_key_path=/usr/viewtouch/ssh/reverse_ssh_key
```

### Option B: Using Public Tunneling Services (Easier)

For personal use without a public IP, use free tunneling services:

#### Using Serveo (SSH-based):
```bash
# Start tunnel to serveo
ssh -R 80:localhost:22 serveo.net

# This gives you a URL like: tcp://subdomain.serveo.net:xxxxx
# Configure ViewTouch to connect to: subdomain.serveo.net
```

#### Using Ngrok (if you prefer):
```bash
# Install ngrok
wget https://bin.equinox.io/c/bNyj1mQVY4c/ngrok-v3-stable-linux-amd64.tgz
tar xvf ngrok-v3-stable-linux-amd64.tgz

# Start tunnel
./ngrok tcp 22

# This gives you: tcp://0.tcp.ngrok.io:xxxxx
# Configure ViewTouch to connect to: 0.tcp.ngrok.io
```

### 4. Configure ViewTouch for Your Setup

#### For Personal Computer Setup:
```bash
# Edit the config
sudo nano /etc/viewtouch/reverse_ssh.conf

# Example configuration:
management_server=your.home.ip.address    # Your public IP
management_port=22
remote_user=your_linux_username
local_port=22
remote_port=2222
ssh_key_path=/usr/viewtouch/ssh/reverse_ssh_key
```

#### For Serveo/Ngrok Services:
```bash
# Edit the config
sudo nano /etc/viewtouch/reverse_ssh.conf

# For Serveo:
management_server=subdomain.serveo.net
management_port=22
remote_user=serveo  # Usually just 'serveo'

# For Ngrok:
management_server=0.tcp.ngrok.io
management_port=xxxxx  # The port ngrok gives you
remote_user=ngrok_user  # Whatever user you set up
```

### 5. Generate SSH Key

For both integrated and standalone configurations:

```bash
# Using the management script (recommended)
sudo vt_reverse_ssh generate-key

# Or manually
sudo mkdir -p /usr/viewtouch/ssh
sudo ssh-keygen -t ed25519 -f /usr/viewtouch/ssh/reverse_ssh_key -N '' -C "viewtouch-reverse-ssh-$(hostname)"
sudo chmod 600 /usr/viewtouch/ssh/reverse_ssh_key
sudo chmod 644 /usr/viewtouch/ssh/reverse_ssh_key.pub
```

### 2. Configure Management Server

1. Copy the public key (`/usr/viewtouch/ssh/reverse_ssh_key.pub`) to your management server
2. Add it to the authorized_keys file of the remote user:

```bash
# On management server
echo "public_key_content_here" >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
```

3. Ensure the SSH server allows public key authentication and tunneling:

```bash
# /etc/ssh/sshd_config on management server
PubkeyAuthentication yes
PasswordAuthentication no
PermitTunnel yes
GatewayPorts yes
```

### 3. Configure Firewall

Ensure outbound SSH connections are allowed from the POS system to the management server.

### 4. Start the Service

#### Integrated Service (with ViewTouch)

The service **always starts automatically** when ViewTouch starts and stops when ViewTouch shuts down. Configuration settings are used for connection parameters, but the service itself is always enabled.

#### Standalone Daemon

```bash
# Manual management
sudo vt_reverse_ssh start
sudo vt_reverse_ssh status
sudo vt_reverse_ssh stop

# Systemd management
sudo systemctl enable reverse-ssh-daemon
sudo systemctl start reverse-ssh-daemon
sudo systemctl status reverse-ssh-daemon
```

## Usage

### Testing Connection

```bash
# Test SSH connection
sudo vt_reverse_ssh test

# Check daemon status
sudo vt_reverse_ssh status

# View configuration
sudo vt_reverse_ssh config
```

### Connecting to POS System

Once the tunnel is established, connect from your computer:

#### From Your Personal Computer (as management server):
```bash
# Connect to the ViewTouch system via the tunnel
ssh -p 2222 localhost

# Or with specific user/key:
ssh -i ~/.ssh/viewtouch_key -p 2222 your_user@localhost
```

#### When Using Serveo/Ngrok:
```bash
# For Serveo - connect directly (serveo handles authentication)
ssh serveo@subdomain.serveo.net -p assigned_port

# For Ngrok - connect to the ngrok endpoint
ssh -p assigned_port user@0.tcp.ngrok.io
```

#### Dynamic Port Assignment:
If using `remote_port=0` (auto-assign), check the assigned port:
```bash
# Check which port was assigned
sudo vt_reverse_ssh status
# Shows: "Tunnel active: server.com:XXXX -> localhost:22"

# Then connect using that port
ssh -p XXXX localhost
```

### Monitoring

- Check logs: `/var/log/viewtouch/reverse_ssh_daemon.log`
- Monitor tunnel status: `sudo vt_reverse_ssh status`
- View systemd status: `sudo systemctl status reverse-ssh-daemon`

## Security Considerations

### Key Management

- Use strong SSH keys (Ed25519 recommended over RSA)
- Regularly rotate SSH keys using the security script
- Store private keys securely with proper permissions (600)
- Use different keys for different purposes

### Network Security

- Use firewall rules to restrict access to management servers only
- Consider using VPN in addition to SSH tunnels for additional security
- Regularly audit SSH access logs and connection attempts
- Use non-standard SSH ports when possible

### Access Control

- Use dedicated user accounts for remote access
- Implement proper sudo policies limiting remote user capabilities
- Monitor and log all remote access attempts
- Disable password authentication on both ends

### SSH Security Tools

ViewTouch provides security management tools:

```bash
# Run full security audit
sudo vt_ssh_security audit

# Rotate SSH keys
sudo vt_ssh_security rotate-key /usr/viewtouch/ssh/reverse_ssh_key

# Harden SSH server configuration
sudo vt_ssh_security harden-ssh

# Check and fix file permissions
sudo vt_ssh_security check-perms
sudo vt_ssh_security fix-perms

# Generate secure SSH client configuration
sudo vt_ssh_security gen-mgmt-config
```

### Best Practices

1. **Key Rotation**: Rotate SSH keys regularly, especially after security incidents
2. **Access Monitoring**: Monitor SSH logs for unauthorized access attempts
3. **Firewall Rules**: Restrict SSH access to known management servers only
4. **Updates**: Keep SSH software updated with latest security patches
5. **Backup Keys**: Maintain secure backups of SSH keys
6. **Principle of Least Privilege**: Limit what remote users can do on the system

## Troubleshooting

### Common Issues

1. **Connection Refused**
   - Check if SSH service is running on management server
   - Verify firewall rules
   - Confirm SSH key is properly installed

2. **Authentication Failed**
   - Verify SSH key is correct
   - Check file permissions (600 for private key)
   - Ensure authorized_keys has correct permissions

3. **Tunnel Not Establishing**
   - Check network connectivity
   - Verify management server hostname/IP
   - Review daemon logs for error messages

### Log Files

- ViewTouch integrated: `/var/log/viewtouch/viewtouch.log`
- Standalone daemon: `/var/log/viewtouch/reverse_ssh_daemon.log`
- System logs: `journalctl -u reverse-ssh-daemon`

### Debug Mode

Run daemon in foreground for debugging:

```bash
sudo vt_reverse_ssh stop
sudo reverse_ssh_daemon -f -c /etc/viewtouch/reverse_ssh.conf
```

## Advanced Configuration

### Custom SSH Options

For advanced SSH configurations, modify the SSH command generation in the source code or create custom wrapper scripts.

### Multiple Tunnels

Configure multiple reverse SSH daemons for different services (SSH, HTTP, etc.) by creating separate configuration files and systemd services.

### Port Forwarding

The reverse SSH tunnel can forward additional ports:

```bash
# Forward additional ports in the SSH command
ssh -R 2222:localhost:22 -R 8080:localhost:80 user@management-server
```

## API Integration

The reverse SSH service can be integrated with monitoring systems via the status reporting functionality. The service provides health status and tunnel information that can be queried programmatically.

## Multiple Location Management

### Identifying Different Locations

When multiple ViewTouch systems connect to your management computer, you need ways to distinguish them:

#### 1. **Location-Based Configuration Files**
```bash
# Each location gets its own config file
/etc/viewtouch/reverse_ssh.conf          # Default
/etc/viewtouch/reverse_ssh_downtown.conf # Downtown location
/etc/viewtouch/reverse_ssh_mall.conf     # Mall location
/etc/viewtouch/reverse_ssh_airport.conf  # Airport location
```

#### 2. **Port-Based Separation**
Each location uses a different port on your management server:
- Downtown: `ssh -p 2222 localhost`
- Mall: `ssh -p 2223 localhost`
- Airport: `ssh -p 2224 localhost`

#### 3. **Location Names in Logs**
```bash
sudo vt_reverse_ssh_setup status
# Output:
# Ngrok tunnel active for downtown: tcp://0.tcp.ngrok.io:2222
# Ngrok tunnel active for mall: tcp://0.tcp.ngrok.io:2223
# Ngrok tunnel active for airport: tcp://0.tcp.ngrok.io:2224
```

#### 4. **Custom Hostnames (Advanced)**
For dedicated servers, use different subdomains:
- `downtown.yourcompany.com:22`
- `mall.yourcompany.com:22`
- `airport.yourcompany.com:22`

### Best Practices for Multiple Locations

1. **Use descriptive names**: `downtown_main`, `mall_foodcourt`, `airport_terminal2`
2. **Document port assignments**: Keep a list of which port belongs to which location
3. **Regular monitoring**: Use `vt_reverse_ssh_setup status` to check all tunnels
4. **Organized configs**: Each location's config file contains its specific settings
5. **Backup configurations**: Save your tunnel configurations for quick recovery

### Example Multi-Location Workflow

```bash
# Initial setup for three locations
sudo vt_reverse_ssh_setup ngrok downtown_restaurant
sudo vt_reverse_ssh_setup ngrok mall_foodcourt
sudo vt_reverse_ssh_setup ngrok airport_terminal

# Daily monitoring
sudo vt_reverse_ssh_setup status

# Troubleshooting specific location
sudo vt_reverse_ssh_setup stop downtown_restaurant
sudo vt_reverse_ssh_setup ngrok downtown_restaurant

# Emergency access
ssh -p 2222 localhost  # Downtown
ssh -p 2223 localhost  # Mall
ssh -p 2224 localhost  # Airport
```

## Alternative Approaches for Personal Use

### When You Don't Have a Management Server

#### Option 1: Use Your Own Computer
- Turn your personal computer into a temporary management server
- Requires your computer to be accessible from the internet
- Best for technical users who can configure their own SSH server

#### Option 2: Public Tunneling Services
**Serveo** (Free, SSH-based):
```bash
# Start tunnel (gives you subdomain.serveo.net:port)
ssh -R 80:localhost:22 serveo.net
```

**Ngrok** (Free tier available):
```bash
# Download and start
./ngrok tcp 22
# Gives you: tcp://0.tcp.ngrok.io:port
```

**LocalTunnel** or **Cloudflare Tunnel** are other alternatives.

#### Option 3: Commercial Services
- **Tailscale** or **ZeroTier** for mesh networking
- **AWS Lightsail**, **DigitalOcean Droplet** as personal management server
- **VPN services** with port forwarding

### Quick Personal Setup (Automated)

ViewTouch provides an automated setup script that supports **multiple locations**:

```bash
# Easy setup with ngrok (recommended)
sudo vt_reverse_ssh_setup ngrok

# For multiple locations, specify location names:
sudo vt_reverse_ssh_setup ngrok downtown_restaurant
sudo vt_reverse_ssh_setup ngrok mall_location
sudo vt_reverse_ssh_setup ngrok airport_store

# Each location gets its own tunnel and configuration

# Alternative: Use your own computer as server
sudo vt_reverse_ssh_setup personal downtown_restaurant
sudo vt_reverse_ssh_setup personal mall_location

# Check status for all locations
sudo vt_reverse_ssh_setup status

# Stop specific location
sudo vt_reverse_ssh_setup stop downtown_restaurant

# Stop all locations
sudo vt_reverse_ssh_setup stop
```

### Managing Multiple Locations

When you have multiple ViewTouch systems connecting to your computer:

1. **Use descriptive location names** for each setup
2. **Each location gets separate config files** (`/etc/viewtouch/reverse_ssh_[location].conf`)
3. **Monitor all tunnels** with `vt_reverse_ssh_setup status`
4. **Connect to specific locations** using their assigned ports

#### Example Multi-Location Setup:

```bash
# Terminal 1: Setup downtown restaurant
sudo vt_reverse_ssh_setup ngrok downtown
# Gets port 2222, config: reverse_ssh_downtown.conf

# Terminal 2: Setup mall location
sudo vt_reverse_ssh_setup ngrok mall
# Gets port 2223, config: reverse_ssh_mall.conf

# Check all tunnels
sudo vt_reverse_ssh_setup status
# Shows: downtown (port 2222), mall (port 2223)

# Connect to downtown location
ssh -p 2222 localhost

# Connect to mall location
ssh -p 2223 localhost
```

### Manual Setup (Advanced)

If you prefer manual setup:

```bash
# 1. Install ngrok
wget https://bin.equinox.io/c/bNyj1mQVY4c/ngrok-v3-stable-linux-amd64.tgz
tar xvf ngrok-v3-stable-linux-amd64.tgz

# 2. Start ngrok tunnel
./ngrok tcp 22 &
# Note the assigned address: tcp://0.tcp.ngrok.io:XXXXX

# 3. Configure ViewTouch
sudo nano /etc/viewtouch/reverse_ssh.conf
# Set: management_server=0.tcp.ngrok.io
# Set: management_port=XXXXX (the port ngrok gave you)

# 4. Start ViewTouch
sudo /usr/viewtouch/bin/vtpos

# 5. Connect back
ssh -p XXXXX localhost
```

## Support

For support with reverse SSH tunnel configuration:

1. Check the logs for error messages
2. Verify network connectivity
3. Test SSH connection manually
4. Review firewall configurations
5. Contact ViewTouch support with log excerpts
