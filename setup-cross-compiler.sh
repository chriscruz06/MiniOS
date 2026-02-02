#!/bin/bash
#
# Cross-Compiler Setup Script for i686-elf target
# For Debian/Ubuntu systems (including WSL)
# Based on OSDev Wiki: https://wiki.osdev.org/GCC_Cross-Compiler
#
# This script builds a GCC cross-compiler for OS development.
# It targets i686-elf, which is suitable for 32-bit x86 bare metal development.
#

set -e  # Exit on any error

# =============================================================================
# Configuration - Modify these as needed
# =============================================================================

export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

# Versions - Check https://ftp.gnu.org/gnu/ for latest versions
BINUTILS_VERSION="2.43"
GCC_VERSION="14.2.0"

# Build directories
SRC_DIR="$HOME/src"
BUILD_DIR="$HOME/build"

# Number of parallel jobs (adjust based on your CPU cores)
JOBS=$(nproc)

# =============================================================================
# Colors for output
# =============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# =============================================================================
# Step 1: Install Dependencies
# =============================================================================

install_dependencies() {
    info "Installing dependencies..."
    
    sudo apt update
    sudo apt install -y \
        build-essential \
        bison \
        flex \
        libgmp3-dev \
        libmpc-dev \
        libmpfr-dev \
        texinfo \
        libisl-dev \
        wget \
        tar \
        gzip \
        xz-utils

    info "Dependencies installed successfully!"
}

# =============================================================================
# Step 2: Create directories
# =============================================================================

create_directories() {
    info "Creating directories..."
    
    mkdir -p "$SRC_DIR"
    mkdir -p "$BUILD_DIR"
    mkdir -p "$PREFIX"
    
    info "Directories created!"
}

# =============================================================================
# Step 3: Download source code
# =============================================================================

download_sources() {
    info "Downloading source code..."
    
    cd "$SRC_DIR"
    
    # Download Binutils
    if [ ! -f "binutils-${BINUTILS_VERSION}.tar.xz" ]; then
        info "Downloading Binutils ${BINUTILS_VERSION}..."
        wget "https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz"
    else
        info "Binutils archive already exists, skipping download."
    fi
    
    # Download GCC
    if [ ! -f "gcc-${GCC_VERSION}.tar.xz" ]; then
        info "Downloading GCC ${GCC_VERSION}..."
        wget "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz"
    else
        info "GCC archive already exists, skipping download."
    fi
    
    info "Downloads complete!"
}

# =============================================================================
# Step 4: Extract source code
# =============================================================================

extract_sources() {
    info "Extracting source code..."
    
    cd "$SRC_DIR"
    
    # Extract Binutils
    if [ ! -d "binutils-${BINUTILS_VERSION}" ]; then
        info "Extracting Binutils..."
        tar -xf "binutils-${BINUTILS_VERSION}.tar.xz"
    else
        info "Binutils already extracted."
    fi
    
    # Extract GCC
    if [ ! -d "gcc-${GCC_VERSION}" ]; then
        info "Extracting GCC..."
        tar -xf "gcc-${GCC_VERSION}.tar.xz"
    else
        info "GCC already extracted."
    fi
    
    info "Extraction complete!"
}

# =============================================================================
# Step 5: Build Binutils
# =============================================================================

build_binutils() {
    info "Building Binutils for ${TARGET}..."
    
    cd "$BUILD_DIR"
    
    # Clean previous build if it exists
    rm -rf "build-binutils"
    mkdir -p "build-binutils"
    cd "build-binutils"
    
    # Configure
    info "Configuring Binutils..."
    "$SRC_DIR/binutils-${BINUTILS_VERSION}/configure" \
        --target="$TARGET" \
        --prefix="$PREFIX" \
        --with-sysroot \
        --disable-nls \
        --disable-werror
    
    # Build
    info "Compiling Binutils (this may take a while)..."
    make -j"$JOBS"
    
    # Install
    info "Installing Binutils..."
    make install
    
    info "Binutils built and installed successfully!"
}

# =============================================================================
# Step 6: Build GCC
# =============================================================================

build_gcc() {
    info "Building GCC for ${TARGET}..."
    
    # Verify binutils is in PATH
    if ! which -- "$TARGET-as" > /dev/null 2>&1; then
        error "$TARGET-as is not in PATH. Binutils may not have been installed correctly."
    fi
    
    cd "$BUILD_DIR"
    
    # Clean previous build if it exists
    rm -rf "build-gcc"
    mkdir -p "build-gcc"
    cd "build-gcc"
    
    # Configure
    info "Configuring GCC..."
    "$SRC_DIR/gcc-${GCC_VERSION}/configure" \
        --target="$TARGET" \
        --prefix="$PREFIX" \
        --disable-nls \
        --enable-languages=c,c++ \
        --without-headers \
        --disable-hosted-libstdcxx
    
    # Build GCC
    info "Compiling GCC (this will take a while - go get coffee!)..."
    make -j"$JOBS" all-gcc
    
    # Build libgcc
    info "Compiling libgcc..."
    make -j"$JOBS" all-target-libgcc
    
    # Build libstdc++ (for C++ support)
    info "Compiling libstdc++..."
    make -j"$JOBS" all-target-libstdc++-v3 || warn "libstdc++ build failed (optional)"
    
    # Install
    info "Installing GCC..."
    make install-gcc
    make install-target-libgcc
    make install-target-libstdc++-v3 2>/dev/null || warn "libstdc++ install skipped"
    
    info "GCC built and installed successfully!"
}

# =============================================================================
# Step 7: Verify installation
# =============================================================================

verify_installation() {
    info "Verifying installation..."
    
    echo ""
    echo "Cross-compiler location: $PREFIX"
    echo ""
    
    # Check binutils
    if [ -f "$PREFIX/bin/$TARGET-as" ]; then
        echo -e "${GREEN}✓${NC} $TARGET-as found"
        "$PREFIX/bin/$TARGET-as" --version | head -1
    else
        echo -e "${RED}✗${NC} $TARGET-as NOT found"
    fi
    
    if [ -f "$PREFIX/bin/$TARGET-ld" ]; then
        echo -e "${GREEN}✓${NC} $TARGET-ld found"
        "$PREFIX/bin/$TARGET-ld" --version | head -1
    else
        echo -e "${RED}✗${NC} $TARGET-ld NOT found"
    fi
    
    # Check GCC
    if [ -f "$PREFIX/bin/$TARGET-gcc" ]; then
        echo -e "${GREEN}✓${NC} $TARGET-gcc found"
        "$PREFIX/bin/$TARGET-gcc" --version | head -1
    else
        echo -e "${RED}✗${NC} $TARGET-gcc NOT found"
    fi
    
    echo ""
}

# =============================================================================
# Step 8: Print usage instructions
# =============================================================================

print_usage() {
    echo ""
    echo "============================================================================="
    echo -e "${GREEN}Cross-compiler installation complete!${NC}"
    echo "============================================================================="
    echo ""
    echo "Your cross-compiler is installed at: $PREFIX"
    echo ""
    echo "To use it, add this to your PATH (or add to ~/.bashrc for permanent use):"
    echo ""
    echo "    export PATH=\"$PREFIX/bin:\$PATH\""
    echo ""
    echo "Then you can compile with:"
    echo ""
    echo "    $TARGET-gcc -c myfile.c -o myfile.o"
    echo "    $TARGET-as myfile.s -o myfile.o"
    echo "    $TARGET-ld myfile.o -o mykernel"
    echo ""
    echo "For OS development, use the -ffreestanding flag:"
    echo ""
    echo "    $TARGET-gcc -ffreestanding -c kernel.c -o kernel.o"
    echo ""
    echo "============================================================================="
}

# =============================================================================
# Main script
# =============================================================================

main() {
    echo ""
    echo "============================================================================="
    echo "  Cross-Compiler Setup Script for i686-elf"
    echo "  Target: $TARGET"
    echo "  Prefix: $PREFIX"
    echo "  Binutils: $BINUTILS_VERSION"
    echo "  GCC: $GCC_VERSION"
    echo "============================================================================="
    echo ""
    
    # Check if we're on a Debian-based system
    if ! command -v apt &> /dev/null; then
        warn "This script is designed for Debian/Ubuntu. You may need to modify it."
    fi
    
    # Run all steps
    install_dependencies
    create_directories
    download_sources
    extract_sources
    build_binutils
    build_gcc
    verify_installation
    print_usage
}

# Run main function
main "$@"
