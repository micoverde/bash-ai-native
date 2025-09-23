# ANBS Troubleshooting Guide

## Table of Contents
1. [Quick Diagnostics](#quick-diagnostics)
2. [Installation Issues](#installation-issues)
3. [Display Problems](#display-problems)
4. [AI Service Issues](#ai-service-issues)
5. [Memory System Problems](#memory-system-problems)
6. [Performance Issues](#performance-issues)
7. [Security and Permissions](#security-and-permissions)
8. [Network Connectivity](#network-connectivity)
9. [Database Issues](#database-issues)
10. [Log Analysis](#log-analysis)
11. [Recovery Procedures](#recovery-procedures)
12. [Getting Support](#getting-support)

## Quick Diagnostics

### Health Check Command
```bash
# Run comprehensive system health check
@vertex --health --verbose

# Expected output:
# ✅ AI Service: ONLINE (response time: 45ms)
# ✅ Memory System: HEALTHY (1247 entries, 78% hit rate)
# ✅ Display System: ACTIVE (3 panels, 60fps)
# ✅ Cache System: OPTIMAL (892 entries, 85% hit rate)
# ✅ Database: CONNECTED (WAL mode, 1.2MB)
# ✅ Network: STABLE (TLS 1.3, keep-alive active)
```

### System Status Overview
```bash
# Check ANBS system status
anbs status

# View recent errors
anbs logs --errors --tail 20

# Check resource usage
anbs system resources

# Test all major components
anbs test --all
```

### Emergency Diagnostics
```bash
# If ANBS is unresponsive, run from external shell:
ps aux | grep anbs
netstat -an | grep :443
tail -f ~/.config/anbs/debug.log
```

## Installation Issues

### Build Failures

#### Problem: Configure script fails
```bash
# Error: "configure: error: NCurses development library not found"
sudo apt-get install libncurses5-dev libncurses5-dev
# Or on Red Hat systems:
sudo dnf install ncurses-devel

# Re-run configure
cd bash-5.2
./configure --enable-ai-integration
```

#### Problem: Compilation errors with missing headers
```bash
# Error: "fatal error: curl/curl.h: No such file or directory"
sudo apt-get install libcurl4-openssl-dev

# Error: "fatal error: sqlite3.h: No such file or directory"
sudo apt-get install libsqlite3-dev

# Error: "fatal error: json-c/json.h: No such file or directory"
sudo apt-get install libjson-c-dev
```

#### Problem: Linker errors
```bash
# Error: "undefined reference to SSL_library_init"
sudo apt-get install libssl-dev

# Error: "undefined reference to pthread_create"
# Add -lpthread to LDFLAGS in Makefile.in
export LDFLAGS="$LDFLAGS -lpthread"
make clean && make
```

### Installation Verification

#### Post-Installation Checks
```bash
# Verify ANBS binary
which anbs
anbs --version

# Check required libraries
ldd $(which anbs) | grep -E "(ncurses|curl|sqlite|ssl|json)"

# Test basic functionality
echo '@vertex --health' | anbs --test-mode
```

#### Permission Issues
```bash
# If installation fails with permission errors
sudo chown -R $USER:$USER /usr/local/anbs
sudo chmod +x /usr/local/anbs/bin/anbs

# If configuration directory has wrong permissions
mkdir -p ~/.config/anbs
chmod 700 ~/.config/anbs
```

## Display Problems

### Split-Screen Not Working

#### Problem: Display shows only single panel
**Symptoms:**
- No split-screen interface
- Missing AI chat panel
- Health monitor not visible

**Diagnosis:**
```bash
# Check terminal size
echo "Terminal: ${COLUMNS}x${LINES}"

# Check if NCurses is properly initialized
@vertex config get display.split_mode
```

**Solutions:**
```bash
# Enable split-screen mode
@vertex config set display.split_mode true

# Reset display system
Ctrl+L (clear screen)
@vertex display reset

# Check minimum terminal size (requires 80x24)
resize -s 30 100  # Set to 100x30 if needed
```

#### Problem: Garbled or corrupted display
**Symptoms:**
- Text overlapping
- Missing borders
- Incorrect colors

**Solutions:**
```bash
# Reset terminal
reset
clear

# Restart ANBS display system
@vertex display restart

# Check TERM environment variable
echo $TERM
export TERM=xterm-256color  # If needed

# Check locale settings
locale
export LC_ALL=C.UTF-8  # If needed
```

### Panel Layout Issues

#### Problem: Incorrect panel sizes
```bash
# Check current ratios
@vertex config get display.terminal_ratio
@vertex config get display.ai_chat_ratio

# Reset to defaults
@vertex config set display.terminal_ratio 0.7
@vertex config set display.ai_chat_ratio 0.3

# Manual resize
Ctrl+R, then use arrow keys
```

#### Problem: Panels not responding to input
```bash
# Check which panel has focus
@vertex display focus

# Switch panel focus
Tab (or Ctrl+A)

# Reset input handling
@vertex display reset-input
```

### Color and Theme Issues

#### Problem: No colors or wrong colors
```bash
# Check color support
tput colors

# Enable 256-color mode
export TERM=xterm-256color

# Reset color pairs
@vertex config set display.theme dark
@vertex display refresh
```

## AI Service Issues

### Connection Problems

#### Problem: "AI service unavailable" errors
**Symptoms:**
```
Error: Failed to connect to AI service
Connection timeout after 30 seconds
```

**Diagnosis:**
```bash
# Test network connectivity
curl -I https://api.openai.com/v1/models
ping 8.8.8.8

# Check API configuration
echo $ANBS_API_KEY | wc -c  # Should be > 40 characters
@vertex config get ai.api_endpoint
```

**Solutions:**
```bash
# Set correct API key
export ANBS_API_KEY="your-actual-api-key-here"

# Check firewall settings
sudo ufw status
# If blocked, allow HTTPS
sudo ufw allow out 443

# Test with verbose logging
ANBS_DEBUG=1 @vertex "test query"
```

#### Problem: Authentication failures
**Symptoms:**
```
Error: Authentication failed (401)
Invalid API key
```

**Solutions:**
```bash
# Verify API key format
echo $ANBS_API_KEY | grep -E '^sk-[a-zA-Z0-9]{48}$'

# Rotate API key
@vertex config rotate-api-key

# Test with curl directly
curl -H "Authorization: Bearer $ANBS_API_KEY" \
     https://api.openai.com/v1/models
```

### Response Issues

#### Problem: Slow AI responses
**Symptoms:**
- Responses taking > 10 seconds
- Timeout errors
- Poor user experience

**Diagnosis:**
```bash
# Check response times
@vertex --benchmark

# Check cache hit rate
@vertex cache stats

# Monitor network latency
ping api.openai.com
```

**Solutions:**
```bash
# Increase cache size
@vertex config set cache.max_entries 20000

# Reduce timeout for testing
@vertex config set ai.timeout 15

# Enable compression
@vertex config set network.enable_compression true

# Use faster model for simple queries
@vertex --model gpt-3.5-turbo "simple question"
```

#### Problem: Malformed or empty responses
**Symptoms:**
```
Error: Empty response from AI service
Error: JSON parse error
Unexpected response format
```

**Solutions:**
```bash
# Check response format
ANBS_DEBUG=1 @vertex "test" 2>&1 | grep "Raw response"

# Validate API endpoint
@vertex config get ai.api_endpoint

# Test with minimal query
@vertex "Hi"

# Check model availability
@vertex config list-models
```

## Memory System Problems

### Database Issues

#### Problem: "Database locked" errors
**Symptoms:**
```
Error: Database is locked
Error: Cannot write to memory database
```

**Solutions:**
```bash
# Check database file permissions
ls -la ~/.config/anbs/memory.db*

# Kill any hanging database connections
@memory system reset

# Rebuild database if corrupted
@memory backup ~/anbs_backup.json
rm ~/.config/anbs/memory.db*
@memory restore ~/anbs_backup.json
```

#### Problem: Memory search returns no results
**Symptoms:**
- @memory search returns "No results found"
- Previously stored conversations missing

**Diagnosis:**
```bash
# Check database integrity
@memory stats

# Verify entries exist
@memory recent

# Check search index
@memory system check-index
```

**Solutions:**
```bash
# Rebuild search index
@memory system rebuild-index

# Optimize database
@memory system optimize

# Re-import conversations if needed
@memory import ~/conversation_backup.json
```

### Memory Performance

#### Problem: Slow memory searches
**Symptoms:**
- @memory search takes > 5 seconds
- System becomes unresponsive during search

**Solutions:**
```bash
# Check database size
@memory stats

# Clean old entries
@memory cleanup --older-than 90d

# Optimize database
@memory system optimize

# Increase cache size
@memory config set cache_size 20000
```

#### Problem: High memory usage
**Symptoms:**
- System running out of RAM
- ANBS consuming > 500MB memory

**Solutions:**
```bash
# Check memory breakdown
@memory system memory-usage

# Clear temporary data
@memory system clear-temp

# Reduce embedding cache
@memory config set embedding_cache_size 1000

# Enable memory compression
@memory config set enable_compression true
```

## Performance Issues

### General Slowness

#### Problem: ANBS response time > 5 seconds
**Diagnosis:**
```bash
# Run performance benchmark
@vertex --benchmark

# Check system resources
top -p $(pgrep anbs)

# Monitor I/O
iotop -p $(pgrep anbs)
```

**Solutions:**
```bash
# Enable all optimizations
@vertex optimize enable-all

# Increase cache size
@vertex config set cache.max_entries 15000

# Use SSD storage for database
mv ~/.config/anbs/memory.db /path/to/ssd/
ln -s /path/to/ssd/memory.db ~/.config/anbs/

# Increase thread pool
@vertex config set performance.max_threads 8
```

### High CPU Usage

#### Problem: ANBS consuming > 80% CPU
**Diagnosis:**
```bash
# Profile CPU usage
perf top -p $(pgrep anbs)

# Check for infinite loops
strace -p $(pgrep anbs) 2>&1 | head -20
```

**Solutions:**
```bash
# Reduce display refresh rate
@vertex config set display.refresh_rate 30

# Disable real-time monitoring
@vertex config set monitoring.real_time false

# Reduce thread count
@vertex config set performance.max_threads 4

# Enable CPU throttling
@vertex config set performance.cpu_throttle true
```

### Memory Leaks

#### Problem: ANBS memory usage keeps growing
**Diagnosis:**
```bash
# Monitor memory over time
watch -n 5 'ps -o pid,vsz,rss,comm -p $(pgrep anbs)'

# Check for leaks
valgrind --leak-check=full anbs
```

**Solutions:**
```bash
# Enable memory debugging
export ANBS_DEBUG_MEMORY=1

# Restart ANBS periodically
# Add to crontab:
# 0 6 * * * pkill anbs; /usr/local/bin/anbs --daemon

# Update to latest version (may have leak fixes)
anbs update
```

## Security and Permissions

### Sandbox Issues

#### Problem: "Sandbox creation failed" errors
**Symptoms:**
```
Error: Cannot create sandbox environment
Permission denied: chroot operation failed
```

**Solutions:**
```bash
# Check if running as root (for chroot)
whoami

# Create sandbox directory
sudo mkdir -p /tmp/anbs_sandbox
sudo chown $USER:$USER /tmp/anbs_sandbox

# Disable sandbox temporarily
@vertex config set security.sandbox_enabled false

# Check seccomp support
grep CONFIG_SECCOMP /boot/config-$(uname -r)
```

#### Problem: Permission denied for file operations
**Symptoms:**
```
Error: Permission denied: /path/to/file
Cannot analyze file: access denied
```

**Solutions:**
```bash
# Check file permissions
ls -la /path/to/file

# Add user to appropriate groups
sudo usermod -a -G anbs-users $USER

# Check RBAC settings
@vertex permissions list

# Temporarily elevate permissions
@vertex permissions elevate --temporary
```

### API Key Security

#### Problem: API key exposure in logs
**Solutions:**
```bash
# Clear logs containing keys
@vertex logs clear --secure

# Rotate API key immediately
@vertex config rotate-api-key

# Enable key encryption
@vertex config set security.encrypt_api_key true

# Check for key in environment
env | grep -i api | grep -v ANBS_API_KEY
```

## Network Connectivity

### WebSocket Issues

#### Problem: WebSocket connection failures
**Symptoms:**
```
Error: WebSocket handshake failed
Connection closed unexpectedly
```

**Diagnosis:**
```bash
# Test WebSocket connectivity
wscat -c wss://api.example.com/ws

# Check proxy settings
echo $HTTP_PROXY $HTTPS_PROXY

# Test with curl
curl --include --no-buffer \
     --header "Connection: Upgrade" \
     --header "Upgrade: websocket" \
     --header "Sec-WebSocket-Key: SGVsbG8sIHdvcmxkIQ==" \
     --header "Sec-WebSocket-Version: 13" \
     wss://api.example.com/ws
```

**Solutions:**
```bash
# Disable proxy for WebSocket
@vertex config set network.disable_proxy_for_websocket true

# Use alternative transport
@vertex config set network.fallback_to_http true

# Increase connection timeout
@vertex config set network.websocket_timeout 30
```

### TLS/SSL Issues

#### Problem: Certificate verification failures
**Symptoms:**
```
Error: SSL certificate verification failed
Error: Unable to get local issuer certificate
```

**Solutions:**
```bash
# Update CA certificates
sudo apt-get update && sudo apt-get install ca-certificates

# Temporarily disable certificate verification (NOT recommended for production)
@vertex config set network.verify_certificates false

# Add custom CA certificate
sudo cp custom-ca.crt /usr/local/share/ca-certificates/
sudo update-ca-certificates

# Check certificate chain
openssl s_client -connect api.openai.com:443 -showcerts
```

## Database Issues

### SQLite Problems

#### Problem: Database corruption
**Symptoms:**
```
Error: Database disk image is malformed
Error: Database schema error
```

**Recovery:**
```bash
# Backup current database
cp ~/.config/anbs/memory.db ~/.config/anbs/memory.db.corrupt

# Try to recover
sqlite3 ~/.config/anbs/memory.db ".dump" | \
sqlite3 ~/.config/anbs/memory_recovered.db

# If recovery fails, restore from backup
@memory restore ~/last_backup.json

# Rebuild from scratch if needed
rm ~/.config/anbs/memory.db
@memory system init
```

#### Problem: Database file locked
**Solutions:**
```bash
# Find processes using database
lsof ~/.config/anbs/memory.db

# Kill hanging processes
pkill -f "anbs.*memory"

# Clear WAL and SHM files
rm ~/.config/anbs/memory.db-wal
rm ~/.config/anbs/memory.db-shm

# Restart database connection
@memory system restart
```

### Performance Issues

#### Problem: Slow database queries
**Diagnosis:**
```bash
# Enable query logging
@memory config set log_slow_queries true
@memory config set slow_query_threshold 100

# Check database size
du -h ~/.config/anbs/memory.db

# Analyze query performance
@memory system analyze-performance
```

**Solutions:**
```bash
# Optimize database
@memory system optimize

# Rebuild indexes
@memory system rebuild-indexes

# Enable WAL mode
@memory config set journal_mode WAL

# Increase cache size
@memory config set cache_size 20000
```

## Log Analysis

### Understanding Log Levels

#### Log Level Hierarchy
```
TRACE: Detailed execution flow
DEBUG: Development information
INFO:  General information
WARN:  Warning conditions
ERROR: Error conditions
FATAL: System unusable
```

### Key Log Files

#### Main Log Files
```bash
# Main application log
tail -f ~/.config/anbs/anbs.log

# Security events
tail -f ~/.config/anbs/security.log

# Performance metrics
tail -f ~/.config/anbs/performance.log

# Debug output (when enabled)
tail -f ~/.config/anbs/debug.log
```

### Log Analysis Commands

#### Search for Specific Issues
```bash
# Find all errors in last hour
@vertex logs --level ERROR --since "1h"

# Search for specific error patterns
@vertex logs --grep "connection timeout"

# Find performance issues
@vertex logs --grep "slow query" --since "24h"

# Security events
@vertex logs --file security.log --since "7d"
```

#### Log Correlation
```bash
# Find related events by session ID
@vertex logs --session-id abc123

# Timeline view of events
@vertex logs --timeline --since "1h"

# Export logs for analysis
@vertex logs --export /tmp/anbs_logs_$(date +%Y%m%d).tar.gz
```

### Common Log Patterns

#### Error Patterns to Watch For
```bash
# Connection issues
grep -E "(connection.*failed|timeout|refused)" ~/.config/anbs/anbs.log

# Memory issues
grep -E "(out of memory|allocation failed|leak detected)" ~/.config/anbs/anbs.log

# Performance issues
grep -E "(slow query|high latency|timeout)" ~/.config/anbs/performance.log

# Security issues
grep -E "(permission denied|unauthorized|security violation)" ~/.config/anbs/security.log
```

## Recovery Procedures

### Emergency Recovery

#### Complete System Reset
```bash
# Stop ANBS
pkill anbs

# Backup current configuration
cp -r ~/.config/anbs ~/.config/anbs.backup.$(date +%Y%m%d)

# Reset to defaults
rm -rf ~/.config/anbs
anbs --init

# Restore data from backup
@memory restore ~/.config/anbs.backup.*/memory_backup.json
```

#### Partial Recovery

##### Reset Display System Only
```bash
@vertex display reset --force
@vertex config set display.split_mode true
@vertex display restart
```

##### Reset Memory System Only
```bash
@memory system backup ~/emergency_backup.json
@memory system reset
@memory system init
@memory restore ~/emergency_backup.json
```

##### Reset Network Configuration
```bash
@vertex config reset network
@vertex network restart
@vertex --health
```

### Data Recovery

#### Recover Conversation History
```bash
# Find backup files
find ~ -name "*anbs*backup*" -type f

# Recover from automatic backups
@memory list-backups
@memory restore --backup latest

# Recover from log files
@vertex logs extract-conversations > recovered_conversations.json
@memory import recovered_conversations.json
```

#### Recover Configuration
```bash
# Find configuration backups
find ~/.config/anbs -name "*.backup" -o -name "config.*.bak"

# Restore specific configuration section
@vertex config restore display < config.backup
@vertex config restore security < config.backup

# Reset specific configuration section
@vertex config reset performance
@vertex config reload
```

### Backup and Restore Procedures

#### Regular Backup Commands
```bash
# Full system backup
@vertex backup create ~/anbs_full_backup_$(date +%Y%m%d).tar.gz

# Configuration only
@vertex config export > ~/anbs_config_$(date +%Y%m%d).json

# Memory/conversations only
@memory export ~/anbs_memory_$(date +%Y%m%d).json

# Automated backup script
#!/bin/bash
DATE=$(date +%Y%m%d_%H%M%S)
@vertex backup create ~/backups/anbs_${DATE}.tar.gz
@memory export ~/backups/memory_${DATE}.json
find ~/backups -name "anbs_*" -mtime +30 -delete
```

#### Restore Procedures
```bash
# Full system restore
@vertex backup restore ~/anbs_full_backup_20240101.tar.gz

# Selective restore
@vertex config import < ~/anbs_config_20240101.json
@memory import ~/anbs_memory_20240101.json

# Verify restore
@vertex --health
@memory stats
@vertex test --all
```

## Getting Support

### Before Contacting Support

#### Collect Diagnostic Information
```bash
# Generate comprehensive diagnostic report
@vertex diagnostic create --output ~/anbs_diagnostic_$(date +%Y%m%d).tar.gz

# The diagnostic includes:
# - System information
# - Configuration files
# - Log files (last 48 hours)
# - Performance metrics
# - Memory usage reports
# - Network connectivity tests
```

#### Minimal Reproduction Case
```bash
# Create minimal test case
echo "Steps to reproduce:" > ~/bug_report.txt
echo "1. Start ANBS" >> ~/bug_report.txt
echo "2. Run: @vertex 'test query'" >> ~/bug_report.txt
echo "3. Observe: [describe issue]" >> ~/bug_report.txt

# Include system information
@vertex system info >> ~/bug_report.txt
```

### Support Channels

#### Community Support
- GitHub Issues: https://github.com/user/bash-ai-native/issues
- Discussion Forum: https://github.com/user/bash-ai-native/discussions
- Discord Server: https://discord.gg/anbs-community

#### Bug Reporting Template
```markdown
## Bug Report

**ANBS Version**: [run `anbs --version`]
**OS**: [e.g., Ubuntu 22.04, macOS 13.0]
**Terminal**: [e.g., GNOME Terminal, iTerm2]

**Description**:
Brief description of the issue

**Steps to Reproduce**:
1. Step one
2. Step two
3. Step three

**Expected Behavior**:
What should happen

**Actual Behavior**:
What actually happens

**Logs**:
```
[Paste relevant log entries here]
```

**Additional Context**:
Any other relevant information

**Diagnostic Archive**:
[Attach the file generated by `@vertex diagnostic create`]
```

#### Feature Request Template
```markdown
## Feature Request

**Summary**:
Brief description of the feature

**Motivation**:
Why this feature would be useful

**Detailed Description**:
Comprehensive description of the feature

**Use Cases**:
Specific scenarios where this would be used

**Implementation Ideas**:
Any thoughts on how this could be implemented

**Alternatives**:
Alternative solutions you've considered
```

### Self-Help Resources

#### Documentation Links
- User Guide: `/docs/USER_GUIDE.md`
- Developer Guide: `/docs/DEVELOPER_GUIDE.md`
- API Reference: `/docs/API_REFERENCE.md`
- Security Guide: `/docs/SECURITY.md`
- Performance Guide: `/docs/PERFORMANCE.md`

#### Online Resources
- Official Documentation: https://anbs-docs.example.com
- Video Tutorials: https://youtube.com/anbs-tutorials
- Community Wiki: https://wiki.anbs-community.org

#### Debug Mode Usage
```bash
# Enable comprehensive debug logging
export ANBS_DEBUG=1
export ANBS_LOG_LEVEL=TRACE

# Run problematic command
@vertex "problematic query"

# Review debug logs
less ~/.config/anbs/debug.log

# Disable debug mode
unset ANBS_DEBUG
export ANBS_LOG_LEVEL=INFO
```

This troubleshooting guide covers the most common issues and provides systematic approaches to diagnosing and resolving problems in ANBS. When in doubt, start with the health check command and work through the diagnostic steps systematically.