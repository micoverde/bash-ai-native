# ANBS Installation Guide

Complete installation instructions for AI-Native Bash Shell on Linux and macOS.

## System Requirements

### Minimum Requirements
- **OS**: Linux (Ubuntu 18.04+, Debian 10+, CentOS 8+) or macOS (10.15+)
- **RAM**: 512MB available memory
- **Disk**: 100MB free space
- **CPU**: x86_64 or ARM64 architecture
- **Network**: Internet connection for AI agent communication

### Recommended Requirements
- **RAM**: 2GB available memory for optimal AI performance
- **Disk**: 1GB free space for development tools
- **Terminal**: Modern terminal with 256 colors and Unicode support

## Quick Installation

### One-Line Install (Recommended)
```bash
curl -fsSL https://install.anbs.dev | bash
```

### Manual Installation

1. **Clone Repository**
   ```bash
   git clone https://github.com/micoverde/bash-ai-native.git
   cd bash-ai-native
   ```

2. **Setup Development Environment**
   ```bash
   ./scripts/setup-dev.sh
   ```

3. **Build ANBS**
   ```bash
   ./scripts/build-anbs.sh
   ```

4. **Install System-Wide (Optional)**
   ```bash
   ./scripts/build-anbs.sh --install
   ```

## Platform-Specific Instructions

### Ubuntu/Debian

1. **Update Package Manager**
   ```bash
   sudo apt update && sudo apt upgrade -y
   ```

2. **Install Dependencies**
   ```bash
   sudo apt install -y \
     build-essential \
     autoconf \
     automake \
     libtool \
     pkg-config \
     libncurses5-dev \
     libncursesw5-dev \
     libwebsockets-dev \
     python3-dev \
     python3-pip \
     git \
     curl \
     wget
   ```

3. **Continue with Manual Installation Steps**

### CentOS/RHEL/Fedora

1. **Install Dependencies**
   ```bash
   # CentOS/RHEL 8+
   sudo dnf install -y \
     gcc \
     gcc-c++ \
     make \
     autoconf \
     automake \
     libtool \
     pkgconfig \
     ncurses-devel \
     libwebsockets-devel \
     python3-devel \
     git \
     curl \
     wget

   # CentOS/RHEL 7
   sudo yum install -y \
     gcc \
     gcc-c++ \
     make \
     autoconf \
     automake \
     libtool \
     pkgconfig \
     ncurses-devel \
     python3-devel \
     git \
     curl \
     wget
   ```

2. **Install libwebsockets (if not available)**
   ```bash
   git clone https://github.com/warmcat/libwebsockets.git
   cd libwebsockets
   mkdir build && cd build
   cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
   make && sudo make install
   cd ../..
   ```

### macOS

1. **Install Homebrew** (if not installed)
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

2. **Install Dependencies**
   ```bash
   brew install \
     autoconf \
     automake \
     libtool \
     pkg-config \
     ncurses \
     libwebsockets \
     python3 \
     git \
     curl \
     wget
   ```

3. **Continue with Manual Installation Steps**

### Arch Linux

1. **Install Dependencies**
   ```bash
   sudo pacman -S \
     base-devel \
     autoconf \
     automake \
     libtool \
     pkg-config \
     ncurses \
     libwebsockets \
     python \
     git \
     curl \
     wget
   ```

## Advanced Installation Options

### Development Build

For contributors and developers:

```bash
# Clone with full history
git clone --recurse-submodules https://github.com/micoverde/bash-ai-native.git
cd bash-ai-native

# Install development dependencies
./scripts/setup-dev.sh

# Build with debug symbols
./scripts/build-anbs.sh --debug

# Run tests
./scripts/test-ai-features.sh
```

### Custom Build Configuration

```bash
# Configure with specific options
cd bash-5.2
./configure \
  --prefix=/opt/anbs \
  --enable-debugger \
  --enable-net-redirections \
  --with-installed-readline \
  --enable-progcomp \
  --enable-extended-glob

# Build with custom flags
export CFLAGS="-O3 -march=native -DANBS_OPTIMIZED"
make -j$(nproc)
```

### Docker Installation

Run ANBS in a containerized environment:

```bash
# Pull official image
docker pull anbs/ai-native-bash:latest

# Run interactive container
docker run -it --rm anbs/ai-native-bash:latest

# Build from source in container
docker build -t anbs-local .
docker run -it --rm anbs-local
```

## Configuration

### Environment Variables

Set these in your `~/.bashrc` or `~/.zshrc`:

```bash
# ANBS Configuration
export ANBS_AI_ENDPOINT="wss://api.vertex.ai/v1/chat"
export ANBS_API_KEY="your_api_key_here"
export ANBS_MEMORY_SIZE="1000"  # Number of conversation entries to keep
export ANBS_RESPONSE_TIMEOUT="30"  # AI response timeout in seconds
export ANBS_LAYOUT="split"  # Layout: split, minimal, full
```

### Configuration File

Create `~/.anbs/config.toml`:

```toml
[ai]
# AI service configuration
endpoint = "wss://api.vertex.ai/v1/chat"
api_key = "your_api_key_here"
timeout = 30
max_retries = 3

[memory]
# Conversation memory settings
max_entries = 1000
retention_days = 30
export_format = "json"

[display]
# Interface configuration
layout = "split"
terminal_ratio = 0.6
ai_panel_ratio = 0.4
theme = "dark"
font_size = 12

[security]
# Security settings
sandbox_mode = true
allowed_dirs = ["/home", "/tmp", "/opt"]
blocked_dirs = ["/etc", "/root", "/boot"]
audit_log = true
```

### API Key Setup

1. **Get API Key** from your AI service provider
2. **Set Environment Variable**:
   ```bash
   export ANBS_API_KEY="your_api_key_here"
   echo 'export ANBS_API_KEY="your_api_key_here"' >> ~/.bashrc
   ```
3. **Or use Configuration File** (more secure):
   ```bash
   mkdir -p ~/.anbs
   echo 'api_key = "your_api_key_here"' > ~/.anbs/config.toml
   chmod 600 ~/.anbs/config.toml
   ```

## Verification

### Test Installation

```bash
# Check ANBS version
anbs --version

# Test basic functionality
anbs -c 'echo "Hello, ANBS!"'

# Test AI commands (requires API key)
anbs -c '@vertex "test connection"'

# Check split-screen interface
anbs  # Then resize terminal to see split-screen
```

### Run Test Suite

```bash
# Basic functionality tests
./scripts/test-ai-features.sh

# Performance benchmarks
./scripts/perf-test.sh

# Security validation
./scripts/security-test.sh
```

## Troubleshooting

### Common Issues

#### "Command not found: anbs"
**Solution**: Add ANBS to your PATH or use full path:
```bash
# Temporary (current session)
export PATH="/usr/local/anbs/bin:$PATH"

# Permanent (add to ~/.bashrc)
echo 'export PATH="/usr/local/anbs/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

#### "NCurses library not found"
**Solution**: Install NCurses development libraries:
```bash
# Ubuntu/Debian
sudo apt install libncurses5-dev libncursesw5-dev

# CentOS/RHEL
sudo dnf install ncurses-devel

# macOS
brew install ncurses
```

#### "WebSocket connection failed"
**Solution**: Check network connectivity and API key:
```bash
# Test network connectivity
ping api.vertex.ai

# Verify API key
anbs -c '@vertex --health'

# Check firewall settings
sudo ufw status  # Ubuntu
sudo firewall-cmd --list-all  # CentOS
```

#### "Permission denied" errors
**Solution**: Check file permissions and ownership:
```bash
# Fix permissions
chmod +x /usr/local/anbs/bin/anbs

# Check ownership
ls -la /usr/local/anbs/bin/anbs

# Reinstall with correct permissions
sudo ./scripts/build-anbs.sh --install
```

### Log Files

Check these log files for debugging:

- **Build logs**: `logs/build.log`
- **Runtime logs**: `~/.anbs/anbs.log`
- **AI communication**: `~/.anbs/ai-comm.log`
- **System logs**: `/var/log/anbs.log` (if installed system-wide)

### Debug Mode

Run ANBS with debugging enabled:

```bash
# Enable debug logging
export ANBS_DEBUG=1
anbs

# Verbose AI communication logging
export ANBS_AI_DEBUG=1
anbs -c '@vertex "debug test"'

# Memory debugging (requires debug build)
valgrind --tool=memcheck --leak-check=full anbs
```

## Uninstallation

### Remove ANBS

```bash
# If installed system-wide
sudo rm -rf /usr/local/anbs

# Remove from PATH (edit ~/.bashrc)
# Remove line: export PATH="/usr/local/anbs/bin:$PATH"

# Remove configuration
rm -rf ~/.anbs

# Remove environment variables (edit ~/.bashrc)
# Remove lines starting with: export ANBS_
```

### Clean Build Environment

```bash
# Remove build artifacts
rm -rf build/ logs/

# Clean Bash build
cd bash-5.2
make clean
cd ..

# Remove downloaded dependencies (optional)
rm -rf venv/
```

## Performance Tuning

### Optimize for Speed

```bash
# Build with optimizations
export CFLAGS="-O3 -march=native -flto"
./scripts/build-anbs.sh --clean

# Increase memory limits
export ANBS_MEMORY_SIZE="5000"
export ANBS_CACHE_SIZE="100MB"
```

### Optimize for Memory

```bash
# Build with size optimizations
export CFLAGS="-Os -ffunction-sections -fdata-sections"
export LDFLAGS="-Wl,--gc-sections"
./scripts/build-anbs.sh --clean

# Reduce memory usage
export ANBS_MEMORY_SIZE="100"
export ANBS_CACHE_SIZE="10MB"
```

## Next Steps

After successful installation:

1. **Read the User Guide**: `docs/USER_GUIDE.md`
2. **Try Examples**: `examples/ai-commands.demo`
3. **Join Community**: GitHub Discussions
4. **Contribute**: `docs/DEVELOPER_GUIDE.md`

---

*For additional help, visit our [GitHub Issues](https://github.com/micoverde/bash-ai-native/issues) or [Discussions](https://github.com/micoverde/bash-ai-native/discussions).*