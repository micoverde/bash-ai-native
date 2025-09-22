#!/bin/bash
# AI-Native Bash Shell - Build Script
# Builds ANBS with AI modules integrated into GNU Bash 5.2

set -e

echo "🔨 AI-Native Bash Shell (ANBS) - Build Script"
echo "=============================================="

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

log() {
    echo -e "${GREEN}[BUILD]${NC} $1"
}

info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Configuration
BUILD_DIR="build"
BASH_DIR="bash-5.2"
LOG_DIR="logs"
INSTALL_PREFIX="/usr/local/anbs"

# Parse command line arguments
CLEAN_BUILD=false
DEBUG_BUILD=false
INSTALL_BUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --debug)
            DEBUG_BUILD=true
            shift
            ;;
        --install)
            INSTALL_BUILD=true
            shift
            ;;
        --prefix=*)
            INSTALL_PREFIX="${1#*=}"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --clean     Clean build (remove all build artifacts)"
            echo "  --debug     Build with debug symbols and no optimization"
            echo "  --install   Install ANBS to system (requires sudo)"
            echo "  --prefix=PATH   Install prefix (default: /usr/local/anbs)"
            echo "  -h, --help  Show this help message"
            exit 0
            ;;
        *)
            error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Create directories
mkdir -p "$BUILD_DIR" "$LOG_DIR"

# Clean build if requested
if [[ "$CLEAN_BUILD" == true ]]; then
    log "Cleaning build artifacts..."
    rm -rf "$BUILD_DIR"/*
    rm -rf "$LOG_DIR"/*
    if [[ -f "$BASH_DIR/Makefile" ]]; then
        cd "$BASH_DIR"
        make clean &>/dev/null || true
        cd ..
    fi
    log "Clean complete"
fi

# Check dependencies
log "Checking dependencies..."

MISSING_DEPS=""

# Check build tools
for tool in gcc make autoconf automake pkg-config; do
    if ! command -v "$tool" &> /dev/null; then
        MISSING_DEPS="$MISSING_DEPS $tool"
    fi
done

# Check development libraries
if ! pkg-config --exists ncurses 2>/dev/null; then
    if [[ ! -f /usr/include/ncurses.h ]] && [[ ! -f /usr/local/include/ncurses.h ]]; then
        MISSING_DEPS="$MISSING_DEPS libncurses-dev"
    fi
fi

if ! pkg-config --exists libwebsockets 2>/dev/null; then
    if [[ ! -f /usr/include/libwebsockets.h ]] && [[ ! -f /usr/local/include/libwebsockets.h ]]; then
        warn "libwebsockets not found - AI communication features will be disabled"
    fi
fi

if [[ -n "$MISSING_DEPS" ]]; then
    error "Missing dependencies:$MISSING_DEPS"
    error "Run ./scripts/setup-dev.sh to install dependencies"
    exit 1
fi

log "Dependencies check passed"

# Configure Bash build
log "Configuring GNU Bash 5.2 with AI extensions..."

cd "$BASH_DIR"

# Generate configure script if needed
if [[ ! -f configure ]]; then
    log "Generating configure script..."
    autoconf
fi

# Configure options
CONFIGURE_OPTS=""
if [[ "$DEBUG_BUILD" == true ]]; then
    CONFIGURE_OPTS="$CONFIGURE_OPTS --enable-debugger --with-debug --enable-debug-malloc"
    export CFLAGS="-g -O0 -DDEBUG"
    export CXXFLAGS="-g -O0 -DDEBUG"
else
    export CFLAGS="-O2 -DNDEBUG"
    export CXXFLAGS="-O2 -DNDEBUG"
fi

# AI-specific configure options
CONFIGURE_OPTS="$CONFIGURE_OPTS --enable-net-redirections --enable-readline"

# Add AI module includes and libraries
if pkg-config --exists ncurses; then
    export CFLAGS="$CFLAGS $(pkg-config --cflags ncurses)"
    export LDFLAGS="$LDFLAGS $(pkg-config --libs ncurses)"
fi

if pkg-config --exists libwebsockets; then
    export CFLAGS="$CFLAGS $(pkg-config --cflags libwebsockets) -DANBS_WEBSOCKETS_ENABLED"
    export LDFLAGS="$LDFLAGS $(pkg-config --libs libwebsockets)"
fi

# Add AI core module paths
export CFLAGS="$CFLAGS -I./ai_core -I./ai_core/security -DANBS_AI_ENABLED"

log "Configure options: $CONFIGURE_OPTS"
log "CFLAGS: $CFLAGS"
log "LDFLAGS: $LDFLAGS"

# Run configure
if ./configure --prefix="$INSTALL_PREFIX" $CONFIGURE_OPTS > "../$LOG_DIR/configure.log" 2>&1; then
    log "Configure completed successfully"
else
    error "Configure failed. Check $LOG_DIR/configure.log for details"
    exit 1
fi

# Build ANBS
log "Building AI-Native Bash Shell..."

# Calculate number of parallel jobs
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
log "Using $JOBS parallel build jobs"

if make -j"$JOBS" > "../$LOG_DIR/build.log" 2>&1; then
    log "Build completed successfully"
else
    error "Build failed. Check $LOG_DIR/build.log for details"
    cd ..
    exit 1
fi

cd ..

# Copy binary to build directory
cp "$BASH_DIR/bash" "$BUILD_DIR/anbs"
if [[ -f "$BASH_DIR/bashbug" ]]; then
    cp "$BASH_DIR/bashbug" "$BUILD_DIR/anbsbug"
fi

# Create version info
"$BUILD_DIR/anbs" --version > "$BUILD_DIR/version.txt" 2>&1

log "ANBS binary created: $BUILD_DIR/anbs"

# Run basic tests
log "Running basic functionality tests..."

if "$BUILD_DIR/anbs" -c 'echo "ANBS Test: Hello World"' > "$LOG_DIR/test-basic.log" 2>&1; then
    log "✓ Basic shell functionality working"
else
    warn "✗ Basic shell test failed - check $LOG_DIR/test-basic.log"
fi

# Test AI commands (when implemented)
if "$BUILD_DIR/anbs" -c '@vertex --version' > "$LOG_DIR/test-ai.log" 2>&1; then
    log "✓ AI commands available"
else
    info "AI commands not yet implemented (expected in early development)"
fi

# Install if requested
if [[ "$INSTALL_BUILD" == true ]]; then
    log "Installing ANBS to $INSTALL_PREFIX..."

    if [[ $EUID -ne 0 ]] && [[ "$INSTALL_PREFIX" == /usr/* ]]; then
        warn "Installation to $INSTALL_PREFIX requires sudo privileges"
        sudo mkdir -p "$INSTALL_PREFIX/bin"
        sudo cp "$BUILD_DIR/anbs" "$INSTALL_PREFIX/bin/"
        if [[ -f "$BUILD_DIR/anbsbug" ]]; then
            sudo cp "$BUILD_DIR/anbsbug" "$INSTALL_PREFIX/bin/"
        fi
    else
        mkdir -p "$INSTALL_PREFIX/bin"
        cp "$BUILD_DIR/anbs" "$INSTALL_PREFIX/bin/"
        if [[ -f "$BUILD_DIR/anbsbug" ]]; then
            cp "$BUILD_DIR/anbsbug" "$INSTALL_PREFIX/bin/"
        fi
    fi

    log "ANBS installed to $INSTALL_PREFIX/bin/anbs"
fi

# Generate build report
log "Generating build report..."

cat > "$BUILD_DIR/build-report.txt" << EOF
AI-Native Bash Shell (ANBS) Build Report
========================================

Build Date: $(date)
Build Host: $(hostname)
Build User: $(whoami)

Configuration:
- Debug Build: $DEBUG_BUILD
- Install Build: $INSTALL_BUILD
- Install Prefix: $INSTALL_PREFIX
- Parallel Jobs: $JOBS

Dependencies:
- GCC Version: $(gcc --version | head -1)
- Make Version: $(make --version | head -1)
- NCurses: $(pkg-config --modversion ncurses 2>/dev/null || echo "system")
- WebSockets: $(pkg-config --modversion libwebsockets 2>/dev/null || echo "not found")

Build Artifacts:
- ANBS Binary: $BUILD_DIR/anbs
- Size: $(du -h "$BUILD_DIR/anbs" | cut -f1)
- Version: $(cat "$BUILD_DIR/version.txt")

Logs:
- Configure: $LOG_DIR/configure.log
- Build: $LOG_DIR/build.log
- Basic Test: $LOG_DIR/test-basic.log
- AI Test: $LOG_DIR/test-ai.log
EOF

log ""
log "🎉 ANBS build complete!"
log ""
log "Build artifacts:"
log "- Binary: $BUILD_DIR/anbs"
log "- Report: $BUILD_DIR/build-report.txt"
log "- Logs: $LOG_DIR/"
log ""
log "Next steps:"
log "1. Test: $BUILD_DIR/anbs"
log "2. Run AI tests: ./scripts/test-ai-features.sh"
if [[ "$INSTALL_BUILD" != true ]]; then
    log "3. Install: ./scripts/build-anbs.sh --install"
fi
log ""

# Final status
if [[ -x "$BUILD_DIR/anbs" ]]; then
    info "✅ ANBS build successful!"
    exit 0
else
    error "❌ ANBS build failed!"
    exit 1
fi