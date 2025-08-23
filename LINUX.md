# High-Performance HTTP Proxy Server - Linux/Unix Guide

This guide provides Linux/Unix specific instructions for building, running, and testing the HTTP proxy server.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Quick Start](#quick-start)
3. [Installation](#installation)
4. [Usage](#usage)
5. [Testing](#testing)
6. [Performance Testing](#performance-testing)
7. [Troubleshooting](#troubleshooting)

## Prerequisites

- C compiler (GCC, Clang)
- Make (for automated builds)
- curl (for testing)
- Standard POSIX libraries

### Installing Prerequisites

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential gcc make curl
```

#### CentOS/RHEL/Fedora
```bash
# CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install gcc make curl

# Fedora
sudo dnf groupinstall "Development Tools"
sudo dnf install gcc make curl
```

#### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Or using Homebrew
brew install gcc make curl
```

## Quick Start

```bash
# 1. Build the server
make clean && make

# 2. Start the server
./proxy_server 8080

# 3. Test the server (in new terminal)
curl -x localhost:8080 http://example.com
```

## Installation

### Building from Source

#### Option 1: Use Makefile (Recommended)
```bash
# Clean and build
make clean && make

# Build only
make
```

#### Option 2: Manual Compilation
```bash
gcc -o proxy_server src/proxy_server.c src/components/cache.c src/components/connection_pool.c src/components/http_parser.c src/components/platform.c src/components/proxy_server.c src/components/thread_pool.c -I include -lpthread
```

#### Option 3: Debug Build
```bash
# Build with debug symbols
make debug

# Or manually with debug flags
gcc -g -O0 -DDEBUG -o proxy_server_debug src/proxy_server.c src/components/cache.c src/components/connection_pool.c src/components/http_parser.c src/components/platform.c src/components/proxy_server.c src/components/thread_pool.c -I include -lpthread
```

### Installation (System-wide)
```bash
# Build first
make

# Install to /usr/local/bin (requires sudo)
sudo cp proxy_server /usr/local/bin/

# Make it executable
sudo chmod +x /usr/local/bin/proxy_server

# Now you can run from anywhere
proxy_server 8080
```

## Usage

### Starting the Server

```bash
# Start on default port 8080
./proxy_server 8080

# Start on custom port
./proxy_server 3128

# Run in background
./proxy_server 8080 &

# Run with output redirection
./proxy_server 8080 > proxy.log 2>&1 &

# Run as daemon
nohup ./proxy_server 8080 > proxy.log 2>&1 &
```

### Stopping the Server

```bash
# Find the process
ps aux | grep proxy_server

# Kill by process ID
kill <PID>

# Force kill if needed
kill -9 <PID>

# Kill all proxy_server processes
pkill proxy_server
```

### Client Configuration

#### System-wide Proxy (Environment Variables)
```bash
# Set proxy for current session
export http_proxy=http://localhost:8080
export HTTP_PROXY=http://localhost:8080

# Test with curl (will use proxy automatically)
curl http://example.com

# Unset proxy
unset http_proxy HTTP_PROXY
```

#### Browser Configuration
- **HTTP Proxy**: localhost
- **Port**: 8080 (or your chosen port)
- **HTTPS Proxy**: Leave blank (HTTP only)

#### Testing with Curl
```bash
# Basic test
curl -x localhost:8080 http://example.com

# Test with verbose output
curl -v -x localhost:8080 http://example.com

# Test with headers
curl -H "User-Agent: TestClient" -x localhost:8080 http://httpbin.org/headers

# Test cache performance
time curl -x localhost:8080 http://example.com  # First request (cache miss)
time curl -x localhost:8080 http://example.com  # Second request (cache hit)
```

#### Testing with wget
```bash
# Basic test
wget --proxy=on --http-proxy=localhost:8080 http://example.com

# Test cache performance
time wget --proxy=on --http-proxy=localhost:8080 -O /dev/null http://example.com  # First request
time wget --proxy=on --http-proxy=localhost:8080 -O /dev/null http://example.com  # Second request
```

## Testing

### Manual Testing

#### Basic Functionality
```bash
# Test different websites
curl -x localhost:8080 http://example.com
curl -x localhost:8080 http://httpbin.org/get
curl -x localhost:8080 http://neverssl.com

# Test with different HTTP methods
curl -X GET -x localhost:8080 http://httpbin.org/get
curl -X POST -x localhost:8080 http://httpbin.org/post -d "test=data"
```

#### Cache Testing
```bash
# Test cache miss/hit cycle
echo "=== First request (Cache Miss) ==="
time curl -s -x localhost:8080 http://example.com > /dev/null

echo "=== Second request (Cache Hit) ==="
time curl -s -x localhost:8080 http://example.com > /dev/null

echo "=== Third request (Cache Hit) ==="
time curl -s -x localhost:8080 http://example.com > /dev/null
```

#### Concurrent Testing
```bash
# Test concurrent requests
for i in {1..10}; do
    curl -x localhost:8080 http://httpbin.org/delay/1 &
done
wait
```

### Automated Testing

#### Create a Test Script
```bash
# Create test script
cat > test_proxy_linux.sh << 'EOF'
#!/bin/bash

echo "================================================================"
echo "HTTP PROXY SERVER - LINUX TEST SUITE"
echo "================================================================"

# Check if proxy is running
if ! nc -z localhost 8080; then
    echo "ERROR: Proxy server is not running on port 8080"
    echo "Please start the proxy server first: ./proxy_server 8080"
    exit 1
fi

echo "SUCCESS: Proxy server is running"

# Test sites
sites=("http://example.com" "http://httpbin.org/get" "http://neverssl.com")

for site in "${sites[@]}"; do
    echo ""
    echo "Testing: $site"
    
    # First request (cache miss)
    echo -n "  First request (cache miss): "
    start_time=$(date +%s%N)
    if curl -s -x localhost:8080 "$site" > /dev/null; then
        end_time=$(date +%s%N)
        time_ms=$(( (end_time - start_time) / 1000000 ))
        echo "${time_ms}ms"
        
        # Second request (cache hit)
        echo -n "  Second request (cache hit): "
        start_time=$(date +%s%N)
        if curl -s -x localhost:8080 "$site" > /dev/null; then
            end_time=$(date +%s%N)
            time_ms2=$(( (end_time - start_time) / 1000000 ))
            echo "${time_ms2}ms"
            
            # Calculate speedup
            if [ $time_ms2 -gt 0 ]; then
                speedup=$(( time_ms / time_ms2 ))
                echo "  Cache speedup: ${speedup}x faster"
            fi
        else
            echo "FAILED"
        fi
    else
        echo "FAILED"
    fi
done

echo ""
echo "================================================================"
echo "Test completed"
echo "================================================================"
EOF

# Make executable
chmod +x test_proxy_linux.sh

# Run tests
./test_proxy_linux.sh
```

## Performance Testing

### Benchmarking with Apache Bench
```bash
# Install apache bench
sudo apt install apache2-utils  # Ubuntu/Debian
sudo yum install httpd-tools     # CentOS/RHEL

# Test proxy performance
ab -n 100 -c 10 -X localhost:8080 http://example.com/

# Compare with direct connection
ab -n 100 -c 10 http://example.com/
```

### Load Testing
```bash
# Simple load test
for i in {1..100}; do
    time curl -s -x localhost:8080 http://httpbin.org/get > /dev/null &
done
wait
```

### Memory Usage Monitoring
```bash
# Monitor proxy server memory usage
watch -n 1 'ps -p $(pgrep proxy_server) -o pid,rss,vsz,pcpu,pmem,cmd'

# Or with htop (install with apt install htop)
htop -p $(pgrep proxy_server)
```

## Troubleshooting

### Common Issues

#### "Command not found" when running proxy_server
```bash
# Use explicit path
./proxy_server 8080

# Or add to PATH
export PATH=$PATH:$(pwd)
proxy_server 8080
```

#### "Permission denied" error
```bash
# Make executable
chmod +x proxy_server

# Check file permissions
ls -la proxy_server
```

#### "Address already in use" error
```bash
# Find process using port 8080
sudo netstat -tulpn | grep :8080
# or
sudo ss -tulpn | grep :8080

# Kill the process
sudo kill -9 <PID>

# Or use a different port
./proxy_server 8081
```

#### Build errors
```bash
# Install missing dependencies
sudo apt install build-essential

# Check compiler version
gcc --version

# Clean and rebuild
make clean
make
```

### Debugging

#### Enable Debug Output
```bash
# Build debug version
make debug

# Run with debug output
./proxy_server_debug 8080

# Or redirect to file
./proxy_server_debug 8080 > debug.log 2>&1
```

#### Network Debugging
```bash
# Monitor network connections
netstat -an | grep :8080

# Use tcpdump to monitor traffic
sudo tcpdump -i lo port 8080

# Test with telnet
telnet localhost 8080
```

### Log Analysis
```bash
# View proxy logs
tail -f proxy.log

# Search for errors
grep -i error proxy.log

# Count cache hits/misses
grep -c "CACHE HIT" proxy.log
grep -c "CACHE MISS" proxy.log
```

## Advanced Usage

### Running as Service (systemd)
```bash
# Create service file
sudo cat > /etc/systemd/system/proxy-server.service << 'EOF'
[Unit]
Description=HTTP Proxy Server
After=network.target

[Service]
Type=simple
User=nobody
ExecStart=/usr/local/bin/proxy_server 8080
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd and start service
sudo systemctl daemon-reload
sudo systemctl enable proxy-server
sudo systemctl start proxy-server

# Check status
sudo systemctl status proxy-server
```

### Performance Tuning
```bash
# Increase file descriptor limits
ulimit -n 65536

# Set CPU affinity (if available)
taskset -c 0,1 ./proxy_server 8080

# Run with higher priority
sudo nice -n -10 ./proxy_server 8080
```

---

## Notes

- This proxy server supports HTTP only (not HTTPS)
- Default cache size is 1024 entries
- Default thread pool size is 4 workers
- Default connection pool size is 20 connections
- All timeouts are set to 5 seconds

For Windows-specific instructions, see the main [README.md](README.md) file.
