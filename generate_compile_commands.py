#!/usr/bin/env python3
"""
Generate compile_commands.json for AROS cross-compilation project.
This script creates a compilation database for clangd to understand AROS project structure.
"""

import json
import os
import sys
import glob
import subprocess
from pathlib import Path

# AROS target configurations
AROS_TARGETS = {
    'linux-x86_64': {
        'target': 'x86_64-aros',
        'arch_dir': 'x86_64-all',
        'platform_dir': 'x86_64-pc',
        'defines': ['-D__x86_64__', '-DAROS_ARCHITECTURE="linux-x86_64"', '-DHOST_OS_linux', '-DHOST_ARCH_x86_64']
    },
    'linux-i386': {
        'target': 'i686-aros',
        'arch_dir': 'i386-all',
        'platform_dir': 'i386-pc',
        'defines': ['-D__i386__', '-DAROS_ARCHITECTURE="linux-i386"', '-DHOST_OS_linux', '-DHOST_ARCH_i386']
    },
    'pc-x86_64': {
        'target': 'x86_64-pc-aros',
        'arch_dir': 'x86_64-all',
        'platform_dir': 'x86_64-pc',
        'defines': ['-D__x86_64__', '-DAROS_ARCHITECTURE="pc-x86_64"', '-DHOST_OS_none', '-DHOST_ARCH_x86_64']
    },
    'pc-i386': {
        'target': 'i686-pc-aros',
        'arch_dir': 'i386-all',
        'platform_dir': 'i386-pc',
        'defines': ['-D__i386__', '-DAROS_ARCHITECTURE="pc-i386"', '-DHOST_OS_none', '-DHOST_ARCH_i386']
    },
    'amiga-m68k': {
        'target': 'm68k-amiga-aros',
        'arch_dir': 'm68k-all',
        'platform_dir': 'm68k-amiga',
        'defines': ['-D__m68k__', '-DAROS_ARCHITECTURE="amiga-m68k"', '-DHOST_OS_none', '-DHOST_ARCH_m68k']
    },
    'raspi-armhf': {
        'target': 'arm-aros-gnueabihf',
        'arch_dir': 'arm-all',
        'platform_dir': 'arm-raspi',
        'defines': ['-D__arm__', '-DAROS_ARCHITECTURE="raspi-armhf"', '-DHOST_OS_none', '-DHOST_ARCH_arm']
    }
}

def get_aros_root():
    """Get AROS source root directory"""
    return Path(__file__).parent.absolute()

def get_build_dir(target):
    """Get build directory for target"""
    aros_root = get_aros_root().parent
    return aros_root / f"abiv1/bin/{target}/AROS"

def get_common_includes(aros_root, target_config, build_dir):
    """Get common include paths for AROS compilation"""
    includes = [
        # Build-generated includes (if they exist)
        f"-I{build_dir}/Development/include",
        f"-I{build_dir}/Development/include/aros",
        f"-I{build_dir}/Development/include/aros/{target_config['arch_dir'].split('-')[0]}",

        # Source tree includes
        f"-I{aros_root}/compiler/include",
        f"-I{aros_root}/compiler/arosinclude",
        f"-I{aros_root}/compiler/clib/include",
        f"-I{aros_root}/arch/common/include",
        f"-I{aros_root}/arch/all-unix/devs/filesys/emul_handler",
        f"-I{aros_root}/arch/{target_config['arch_dir']}/include",
        f"-I{aros_root}/arch/{target_config['platform_dir']}/include",
        f"-I{aros_root}/arch/all-pc/include",
        f"-I{aros_root}/arch/all-native/include",

        # ROM includes
        f"-I{aros_root}/rom/exec",
        f"-I{aros_root}/rom/dos",
        f"-I{aros_root}/rom/utility",
        f"-I{aros_root}/rom/graphics",
        f"-I{aros_root}/rom/intuition",
        f"-I{aros_root}/rom/workbench",

        # Workbench includes
        f"-I{aros_root}/workbench/libs",
        f"-I{aros_root}/workbench/c",
        f"-I{aros_root}/workbench/system",
    ]

    # Only include paths that exist
    return [inc for inc in includes if os.path.exists(inc[2:])]

def get_common_flags(target_config):
    """Get common compilation flags"""
    return [
        f"--target={target_config['target']}",
        "-nostdinc",
        "-nostdlib",
        "-fno-builtin",
        "-fno-strict-aliasing",
        "-fomit-frame-pointer",
        "-finline-functions",
        "-Wall",
        "-Wno-pointer-sign",
        "-Wno-unused-but-set-variable",
        "-Wno-uninitialized",
        "-Wno-parentheses",
        "-Wno-format",
        "-Wno-implicit-function-declaration",

        # AROS standard defines
        "-D__AROS__",
        "-D_LARGEFILE_SOURCE",
        "-D_LARGEFILE64_SOURCE",
        "-D_FILE_OFFSET_BITS=64",
        "-DHAVE_WORKING_FORK",
        "-DHAVE_SA_LEN",
        "-DHAVE_RESOLV_H",
        "-DUSE_INLINE_STDARG",
    ] + target_config['defines']

def find_source_files(aros_root):
    """Find all C/C++ source files in the AROS tree"""
    source_extensions = ['*.c', '*.cpp', '*.cc', '*.cxx', '*.C']
    source_files = []

    # Key directories to scan
    scan_dirs = [
        'arch',
        'rom',
        'workbench',
        'compiler',
        'external',
        'tools'
    ]

    for scan_dir in scan_dirs:
        dir_path = aros_root / scan_dir
        if dir_path.exists():
            for ext in source_extensions:
                pattern = str(dir_path / '**' / ext)
                source_files.extend(glob.glob(pattern, recursive=True))

    return [Path(f).relative_to(aros_root) for f in source_files]

def create_compile_command(source_file, aros_root, target_config, build_dir):
    """Create a compile command entry for a source file"""
    includes = get_common_includes(aros_root, target_config, build_dir)
    flags = get_common_flags(target_config)

    # Determine if this is C or C++
    if source_file.suffix in ['.cpp', '.cc', '.cxx', '.C']:
        compiler = 'clang++'
        language_flags = ['-std=c++14']
    else:
        compiler = 'clang'
        language_flags = ['-std=c99']

    command = [compiler] + language_flags + flags + includes + ['-c', str(source_file)]

    return {
        'directory': str(aros_root),
        'command': ' '.join(command),
        'file': str(source_file)
    }

def generate_compile_commands(target='linux-x86_64'):
    """Generate compile_commands.json for the specified target"""
    if target not in AROS_TARGETS:
        print(f"Error: Unknown target '{target}'")
        print(f"Available targets: {', '.join(AROS_TARGETS.keys())}")
        return False

    aros_root = get_aros_root()
    target_config = AROS_TARGETS[target]
    build_dir = get_build_dir(target)

    print(f"Generating compile_commands.json for target: {target}")
    print(f"AROS root: {aros_root}")
    print(f"Build directory: {build_dir}")

    # Find all source files
    print("Scanning for source files...")
    source_files = find_source_files(aros_root)
    print(f"Found {len(source_files)} source files")

    # Generate compile commands
    compile_commands = []
    for source_file in source_files:
        try:
            cmd = create_compile_command(source_file, aros_root, target_config, build_dir)
            compile_commands.append(cmd)
        except Exception as e:
            print(f"Warning: Failed to process {source_file}: {e}")

    # Write compile_commands.json
    output_file = aros_root / 'compile_commands.json'
    with open(output_file, 'w') as f:
        json.dump(compile_commands, f, indent=2)

    print(f"Generated {output_file} with {len(compile_commands)} entries")
    return True

def main():
    if len(sys.argv) > 1:
        target = sys.argv[1]
    else:
        # Try to detect target from existing build
        aros_root = get_aros_root()
        bin_dir = aros_root / 'bin'

        if bin_dir.exists():
            targets = [d.name for d in bin_dir.iterdir() if d.is_dir()]
            if targets:
                target = targets[0]
                print(f"Auto-detected target: {target}")
            else:
                target = 'linux-x86_64'
                print(f"No build detected, using default target: {target}")
        else:
            target = 'linux-x86_64'
            print(f"No bin directory found, using default target: {target}")

    success = generate_compile_commands(target)
    if not success:
        sys.exit(1)

    print("\nNext steps:")
    print("1. Make sure clangd is installed")
    print("2. Configure your editor to use clangd")
    print("3. The .clangd file in the project root provides additional configuration")
    print(f"4. If you change target architecture, re-run: {sys.argv[0]} <target>")

if __name__ == '__main__':
    main()
