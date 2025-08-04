#!/bin/bash

# AROS clangd Configuration Setup Script
# This script helps configure clangd for AROS cross-compilation development

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values (can be overridden by environment variables or command line)
AROS_SOURCE_DIR="${AROS_SOURCE_DIR:-$(pwd)}"
AROS_BUILD_DIR="${AROS_BUILD_DIR:-}"
AROS_TARGET_ARCH="${AROS_TARGET_ARCH:-linux-x86_64}"
AROS_TOOLCHAIN_DIR="${AROS_TOOLCHAIN_DIR:-}"

show_help() {
    cat << EOF
AROS clangd Configuration Setup Script

Usage: $0 [OPTIONS]

This script configures clangd for AROS cross-compilation development by:
1. Detecting your AROS build configuration
2. Generating a proper .clangd configuration file
3. Validating that include paths exist

OPTIONS:
    -h, --help              Show this help message
    -s, --source-dir DIR    AROS source directory (default: current directory)
    -b, --build-dir DIR     AROS build directory (will attempt to auto-detect)
    -t, --target ARCH       Target architecture (default: linux-x86_64)
    -c, --toolchain DIR     Toolchain directory (will attempt to auto-detect)
    -f, --force             Overwrite existing .clangd file
    -v, --verbose           Enable verbose output

ENVIRONMENT VARIABLES:
    AROS_SOURCE_DIR         Override source directory
    AROS_BUILD_DIR          Override build directory
    AROS_TARGET_ARCH        Override target architecture
    AROS_TOOLCHAIN_DIR      Override toolchain directory

EXAMPLES:
    # Auto-detect configuration
    $0

    # Specify custom build directory
    $0 -b /home/user/aros-build

    # Specify different target
    $0 -t pc-i386

    # Force overwrite existing configuration
    $0 -f

EOF
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse command line arguments
FORCE=0
VERBOSE=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -s|--source-dir)
            AROS_SOURCE_DIR="$2"
            shift 2
            ;;
        -b|--build-dir)
            AROS_BUILD_DIR="$2"
            shift 2
            ;;
        -t|--target)
            AROS_TARGET_ARCH="$2"
            shift 2
            ;;
        -c|--toolchain)
            AROS_TOOLCHAIN_DIR="$2"
            shift 2
            ;;
        -f|--force)
            FORCE=1
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Verbose logging function
log_verbose() {
    if [[ $VERBOSE -eq 1 ]]; then
        echo -e "${BLUE}[VERBOSE]${NC} $1"
    fi
}

# Validate source directory
if [[ ! -d "$AROS_SOURCE_DIR" ]]; then
    log_error "Source directory does not exist: $AROS_SOURCE_DIR"
    exit 1
fi

if [[ ! -f "$AROS_SOURCE_DIR/configure" ]] || [[ ! -d "$AROS_SOURCE_DIR/compiler" ]]; then
    log_error "Directory does not appear to be an AROS source tree: $AROS_SOURCE_DIR"
    exit 1
fi

log_info "AROS source directory: $AROS_SOURCE_DIR"

# Auto-detect build directory if not specified
if [[ -z "$AROS_BUILD_DIR" ]]; then
    log_info "Auto-detecting build directory..."

    # Common build directory patterns
    CANDIDATES=(
        "$(dirname "$AROS_SOURCE_DIR")/abiv1"
        "$(dirname "$AROS_SOURCE_DIR")/aros-build"
        "$(dirname "$AROS_SOURCE_DIR")/build"
        "$AROS_SOURCE_DIR/../abiv1"
        "$AROS_SOURCE_DIR/../aros-build"
        "$AROS_SOURCE_DIR/../build"
    )

    for candidate in "${CANDIDATES[@]}"; do
        if [[ -d "$candidate/bin/$AROS_TARGET_ARCH" ]]; then
            AROS_BUILD_DIR="$candidate"
            log_success "Found build directory: $AROS_BUILD_DIR"
            break
        fi
    done

    if [[ -z "$AROS_BUILD_DIR" ]]; then
        log_error "Could not auto-detect build directory. Please specify with -b option."
        log_info "Looking for directories containing: bin/$AROS_TARGET_ARCH"
        exit 1
    fi
fi

# Validate build directory
if [[ ! -d "$AROS_BUILD_DIR" ]]; then
    log_error "Build directory does not exist: $AROS_BUILD_DIR"
    exit 1
fi

TARGET_BIN_DIR="$AROS_BUILD_DIR/bin/$AROS_TARGET_ARCH"
if [[ ! -d "$TARGET_BIN_DIR" ]]; then
    log_error "Target binary directory does not exist: $TARGET_BIN_DIR"
    log_info "Available targets in $AROS_BUILD_DIR/bin/:"
    if [[ -d "$AROS_BUILD_DIR/bin" ]]; then
        ls -1 "$AROS_BUILD_DIR/bin/" | grep -v "host" || true
    fi
    exit 1
fi

log_info "AROS build directory: $AROS_BUILD_DIR"
log_info "Target architecture: $AROS_TARGET_ARCH"

# Auto-detect toolchain directory if not specified
if [[ -z "$AROS_TOOLCHAIN_DIR" ]]; then
    log_info "Auto-detecting toolchain directory..."

    TOOLCHAIN_CANDIDATES=(
        "$(dirname "$AROS_BUILD_DIR")/abiv1-toolchain"
        "$(dirname "$AROS_BUILD_DIR")/aros-toolchain"
        "$(dirname "$AROS_BUILD_DIR")/toolchain"
        "$AROS_BUILD_DIR/../abiv1-toolchain"
        "$AROS_BUILD_DIR/../aros-toolchain"
        "$AROS_BUILD_DIR/../toolchain"
    )

    for candidate in "${TOOLCHAIN_CANDIDATES[@]}"; do
        if [[ -d "$candidate" ]] && ls "$candidate"/*-aros-gcc &>/dev/null; then
            AROS_TOOLCHAIN_DIR="$candidate"
            log_success "Found toolchain directory: $AROS_TOOLCHAIN_DIR"
            break
        fi
    done

    if [[ -z "$AROS_TOOLCHAIN_DIR" ]]; then
        log_warning "Could not auto-detect toolchain directory. This is optional."
    fi
fi

# Check if .clangd already exists
CLANGD_FILE="$AROS_SOURCE_DIR/.clangd"
if [[ -f "$CLANGD_FILE" ]] && [[ $FORCE -eq 0 ]]; then
    log_warning ".clangd file already exists. Use -f to overwrite."
    read -p "Overwrite existing .clangd file? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "Aborted."
        exit 0
    fi
fi

# Validate key include paths
SYSROOT_INCLUDE="$TARGET_BIN_DIR/AROS/Developer/include"
if [[ ! -d "$SYSROOT_INCLUDE" ]]; then
    log_error "Sysroot include directory not found: $SYSROOT_INCLUDE"
    log_info "This suggests the AROS build is incomplete or the paths are incorrect."
    exit 1
fi

# Detect clang builtin include paths
log_info "Detecting clang builtin include paths..."
CLANG_BUILTIN_PATHS=""
for clang_version in 18 17 16 15 14 13 12; do
    clang_path="/usr/lib/clang/${clang_version}/include"
    if [[ -d "$clang_path" ]]; then
        CLANG_BUILTIN_PATHS="$CLANG_BUILTIN_PATHS    - \"-isystem\"
    - \"$clang_path\"
"
        log_verbose "Found clang builtin path: $clang_path"
    fi
done

if [[ -z "$CLANG_BUILTIN_PATHS" ]]; then
    log_warning "No clang builtin include paths found"
    CLANG_BUILTIN_PATHS="    # No clang builtin paths detected"
fi

# Generate .clangd configuration
log_info "Generating .clangd configuration..."

cat > "$CLANGD_FILE" << EOF
# AROS clangd Configuration
# Generated by setup_clangd.sh on $(date)
# Build directory: $AROS_BUILD_DIR
# Target architecture: $AROS_TARGET_ARCH

CompileFlags:
  # Remove problematic GCC-specific flags that clangd doesn't understand
  Remove:
    - "-mcmodel=large"
    - "-mno-red-zone"
    - "-mno-ms-bitfields"
    - "-fno-omit-frame-pointer"
    - "-fno-common"
    - "--sysroot"
    - "-Wno-pointer-sign"
    - "-Wno-parentheses"

  # Add essential compiler flags for AROS
  Add:
    # Target architecture
    - "-target"
    - "x86_64-unknown-linux-gnu"

    # Essential AROS defines
    - "-D__AROS__"
    - "-DAROS_BUILD_TYPE=AROS_BUILD_TYPE_PERSONAL"
    - "-D__STDC_LIMIT_MACROS"
    - "-D__STDC_FORMAT_MACROS"
    - "-D__STDC_CONSTANT_MACROS"
    - "-D_GNU_SOURCE"

    # Source tree includes (relative paths from AROS source root)
    - "-Icompiler/include"
    - "-Icompiler/include/aros"
    - "-Icompiler/include/exec"
    - "-Icompiler/include/proto"
    - "-Icompiler/include/inline"
    - "-Icompiler/include/defines"
    - "-Icompiler/include/clib"
    - "-Icompiler/include/libraries"
    - "-Icompiler/include/devices"
    - "-Icompiler/include/resources"
    - "-Icompiler/include/graphics"
    - "-Icompiler/include/intuition"
    - "-Icompiler/include/utility"
    - "-Icompiler/include/workbench"
    - "-Icompiler/include/dos"
    - "-Icompiler/include/prefs"
    - "-Icompiler/include/gadgets"
    - "-Icompiler/include/datatypes"
    - "-Icompiler/include/hardware"
    - "-Icompiler/include/SDI"

    # Generated includes from build directory
    # Main sysroot includes
    - "-I$TARGET_BIN_DIR/AROS/Developer/include"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/aros"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/exec"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/proto"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/inline"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/defines"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/clib"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/libraries"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/devices"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/resources"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/graphics"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/intuition"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/utility"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/workbench"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/dos"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/prefs"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/gadgets"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/datatypes"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/hardware"
    - "-I$TARGET_BIN_DIR/AROS/Developer/include/SDI"

    # Generated config and include paths
    - "-I$TARGET_BIN_DIR/gen/config"
    - "-I$TARGET_BIN_DIR/gen/include"

    # Standard C library compatibility for host system
    - "-isystem"
    - "/usr/include"
    - "-isystem"
    - "/usr/include/x86_64-linux-gnu"

    # Current directory includes
    - "-I."
    - "-I.."

# Diagnostics configuration
Diagnostics:
  # Suppress common AROS-specific warnings that clangd might not understand
  Suppress:
    - "unknown-pragmas"
    - "gnu-statement-expression"
    - "gnu-zero-variadic-macro-arguments"
    - "deprecated-declarations"
    - "pointer-sign"

  # ClangTidy configuration
  ClangTidy:
    Add:
      - "readability-*"
      - "bugprone-*"
      - "performance-*"
    Remove:
      - "readability-magic-numbers"
      - "readability-function-cognitive-complexity"
      - "readability-identifier-length"
      - "readability-non-const-parameter"

# Index configuration for better performance
Index:
  # Skip indexing standard library to improve performance
  StandardLibrary: No

  # Background indexing priority
  Background: Low

# Completion configuration
Completion:
  # Enable all-scopes completion for better AROS API discovery
  AllScopes: Yes
EOF

log_success "Generated .clangd configuration file"

# Validate some key include paths
log_info "Validating include paths..."

VALIDATION_PATHS=(
    "$AROS_SOURCE_DIR/compiler/include"
    "$SYSROOT_INCLUDE"
    "$TARGET_BIN_DIR/gen/config"
)

MISSING_PATHS=0
for path in "${VALIDATION_PATHS[@]}"; do
    if [[ ! -d "$path" ]]; then
        log_warning "Include path does not exist: $path"
        MISSING_PATHS=$((MISSING_PATHS + 1))
    else
        log_verbose "Validated path: $path"
    fi
done

if [[ $MISSING_PATHS -gt 0 ]]; then
    log_warning "$MISSING_PATHS include paths are missing. Some features may not work correctly."
fi

# Provide usage instructions
log_success "Configuration complete!"
echo
log_info "Next steps:"
echo "1. Restart your LSP client/editor to pick up the new configuration"
echo "2. Open an AROS source file to test clangd functionality"
echo "3. If you encounter issues, check the clangd logs in your editor"
echo
log_info "Configuration summary:"
echo "- Source directory: $AROS_SOURCE_DIR"
echo "- Build directory: $AROS_BUILD_DIR"
echo "- Target architecture: $AROS_TARGET_ARCH"
echo "- Configuration file: $CLANGD_FILE"

if [[ -n "$AROS_TOOLCHAIN_DIR" ]]; then
    echo "- Toolchain directory: $AROS_TOOLCHAIN_DIR"
fi

echo
log_info "To reconfigure later, run: $0 -f"
