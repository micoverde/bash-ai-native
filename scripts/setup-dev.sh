#!/bin/bash
# AI-Native Bash Shell - Development Environment Setup
# Sets up all dependencies needed for ANBS development

set -e

echo "🚀 AI-Native Bash Shell (ANBS) - Development Environment Setup"
echo "=============================================================="

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

log() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
if [[ $EUID -eq 0 ]]; then
   error "This script should not be run as root for security reasons"
   exit 1
fi

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
else
    error "Unsupported OS: $OSTYPE"
    exit 1
fi

log "Detected OS: $OS"

# Update package manager
log "Updating package manager..."
if [[ "$OS" == "linux" ]]; then
    sudo apt-get update -qq
elif [[ "$OS" == "macos" ]]; then
    brew update
fi

# Install build dependencies
log "Installing build dependencies..."

PACKAGES=""
if [[ "$OS" == "linux" ]]; then
    PACKAGES="build-essential autoconf automake libtool pkg-config git curl wget"
elif [[ "$OS" == "macos" ]]; then
    PACKAGES="autoconf automake libtool pkg-config git curl wget"
fi

for pkg in $PACKAGES; do
    if [[ "$OS" == "linux" ]]; then
        if ! dpkg -l | grep -q "^ii  $pkg "; then
            log "Installing $pkg..."
            sudo apt-get install -y "$pkg"
        else
            log "$pkg already installed"
        fi
    elif [[ "$OS" == "macos" ]]; then
        if ! brew list | grep -q "^$pkg$"; then
            log "Installing $pkg..."
            brew install "$pkg"
        else
            log "$pkg already installed"
        fi
    fi
done

# Install NCurses for split-screen interface
log "Installing NCurses development libraries..."
if [[ "$OS" == "linux" ]]; then
    sudo apt-get install -y libncurses5-dev libncursesw5-dev
elif [[ "$OS" == "macos" ]]; then
    brew install ncurses
fi

# Install WebSocket library for AI communication
log "Installing WebSocket development libraries..."
if [[ "$OS" == "linux" ]]; then
    sudo apt-get install -y libwebsockets-dev
elif [[ "$OS" == "macos" ]]; then
    brew install libwebsockets
fi

# Install Python for AI/ML components
log "Installing Python development environment..."
if [[ "$OS" == "linux" ]]; then
    sudo apt-get install -y python3 python3-dev python3-pip python3-venv
elif [[ "$OS" == "macos" ]]; then
    brew install python3
fi

# Create Python virtual environment for AI dependencies
log "Setting up Python virtual environment..."
if [[ ! -d "venv" ]]; then
    python3 -m venv venv
    log "Created virtual environment: venv/"
else
    log "Virtual environment already exists"
fi

# Activate virtual environment and install AI dependencies
log "Installing Python AI/ML dependencies..."
source venv/bin/activate
pip install --upgrade pip

# AI/ML packages for vector memory and embeddings
pip install \
    numpy \
    scikit-learn \
    sentence-transformers \
    websockets \
    asyncio \
    requests

# Install development tools
log "Installing development tools..."
if [[ "$OS" == "linux" ]]; then
    sudo apt-get install -y \
        gdb \
        valgrind \
        clang-format \
        cppcheck \
        doxygen
elif [[ "$OS" == "macos" ]]; then
    brew install \
        gdb \
        clang-format \
        cppcheck \
        doxygen
fi

# Verify installations
log "Verifying installations..."

# Check basic build tools
for tool in gcc make autoconf automake libtool pkg-config; do
    if command -v "$tool" &> /dev/null; then
        VERSION=$(eval "$tool --version 2>/dev/null | head -1" || echo "unknown")
        log "✓ $tool: $VERSION"
    else
        error "✗ $tool: not found"
    fi
done

# Check NCurses
if pkg-config --exists ncurses; then
    NCURSES_VERSION=$(pkg-config --modversion ncurses)
    log "✓ NCurses: $NCURSES_VERSION"
else
    warn "NCurses pkg-config not found (may still be available)"
fi

# Check libwebsockets
if pkg-config --exists libwebsockets; then
    WS_VERSION=$(pkg-config --modversion libwebsockets)
    log "✓ libwebsockets: $WS_VERSION"
else
    warn "libwebsockets pkg-config not found (may still be available)"
fi

# Check Python and packages
if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version)
    log "✓ $PYTHON_VERSION"

    # Check AI packages
    source venv/bin/activate
    for pkg in numpy sklearn transformers websockets; do
        if python3 -c "import $pkg" 2>/dev/null; then
            log "✓ Python package: $pkg"
        else
            warn "Python package missing: $pkg"
        fi
    done
else
    error "Python3 not found"
fi

# Create directory for build artifacts
mkdir -p build/
mkdir -p logs/

log ""
log "🎉 Development environment setup complete!"
log ""
log "Next steps:"
log "1. Run: ./scripts/build-anbs.sh to build ANBS"
log "2. Run: ./scripts/test-ai-features.sh to test AI features"
log ""
log "Development workspace:"
log "- Source code: bash-5.2/"
log "- AI modules: bash-5.2/ai_core/"
log "- Build output: build/"
log "- Logs: logs/"
log "- Virtual env: venv/ (activate with: source venv/bin/activate)"
log ""
log "Documentation:"
log "- Architecture: docs/ARCHITECTURE.md"
log "- Installation: docs/INSTALLATION.md"
log "- User Guide: docs/USER_GUIDE.md"