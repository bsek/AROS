#!/usr/bin/env python3
"""
AROS .clangd Configuration Generator

This script generates a .clangd configuration file from the template,
allowing users to customize paths for their AROS build environment.
"""

import os
import re
import sys
import argparse
import tempfile
from pathlib import Path
from typing import Optional, Dict, List


def substitute_block(content: str, token: str, items: List[str]) -> str:
    """
    Replace the line containing ${token} with one YAML list entry per item,
    indented like the placeholder line. Drops the line entirely if items is
    empty, so no stray blank list entry is left behind.
    """
    placeholder = "${%s}" % token
    out = []
    for line in content.split('\n'):
        if placeholder not in line:
            out.append(line)
            continue
        indent = line[:len(line) - len(line.lstrip())]
        out.extend(f'{indent}- "{item}"' for item in items)
    return '\n'.join(out)


def validate_directory(path: str, name: str) -> bool:
    """Validate that a directory exists and is readable."""
    if not os.path.exists(path):
        print(f"Warning: {name} directory '{path}' does not exist.")
        return False
    if not os.path.isdir(path):
        print(f"Error: {name} path '{path}' is not a directory.")
        return False
    if not os.access(path, os.R_OK):
        print(f"Error: {name} directory '{path}' is not readable.")
        return False
    return True


def detect_source_directory() -> Optional[str]:
    """Try to auto-detect AROS source directory."""
    current_dir = Path.cwd()

    # Look for common AROS source indicators
    source_indicators = [
        "compiler/include/aros",
        "arch",
        "rom/exec"
    ]

    # Check current directory and parents
    for path in [current_dir] + list(current_dir.parents):
        for indicator in source_indicators:
            if (path / indicator).exists():
                return str(path)

    return None


def list_target_architectures(build_dir: str) -> List[str]:
    """
    List target architectures in a build directory. A target is any bin/<name>
    that carries an installed header tree; this excludes the host tools
    directory (bin/darwin-aarch64, bin/linux-x86_64 when cross compiling)
    without needing a hardcoded list of target names.
    """
    bin_dir = Path(build_dir) / "bin"
    if not bin_dir.is_dir():
        return []

    try:
        return sorted(d.name for d in bin_dir.iterdir()
                      if (d / "AROS" / "Developer" / "include").is_dir())
    except (OSError, PermissionError):
        return []


def detect_build_directory() -> Optional[str]:
    """Try to auto-detect AROS build directory."""
    current_dir = Path.cwd()

    # First check if we're already in a build directory
    for path in [current_dir] + list(current_dir.parents):
        if list_target_architectures(str(path)):
            return str(path)

    # If not found, try to find source directory and look for sibling build directories
    source_dir = detect_source_directory()
    if source_dir:
        source_path = Path(source_dir)
        parent_dir = source_path.parent

        # Look for sibling directories that might be build directories
        if parent_dir.exists():
            try:
                for sibling in parent_dir.iterdir():
                    if sibling.is_dir() and sibling != source_path:
                        # Check if this sibling looks like a build directory
                        if list_target_architectures(str(sibling)):
                            return str(sibling)

            except (OSError, PermissionError):
                pass

    return None


def print_directory_structure_help():
    """Print detailed help about AROS directory structures."""
    print("AROS Development Directory Structures")
    print("=" * 50)
    print()
    print("AROS development typically uses one of these directory structures:")
    print()
    print("1. SEPARATE SOURCE AND BUILD DIRECTORIES (Recommended)")
    print("   This keeps source code clean and allows multiple build configurations.")
    print()
    print("   Example structure:")
    print("   /home/user/aros/")
    print("   ├── aros-source/              # AROS source code")
    print("   │   ├── configure             # Build configuration script")
    print("   │   ├── mmakefile            # Main makefile")
    print("   │   ├── compiler/")
    print("   │   │   └── include/          # AROS headers")
    print("   │   ├── arch/                 # Architecture-specific code")
    print("   │   └── rom/                  # ROM modules")
    print("   │")
    print("   └── aros-build/               # Build output directory")
    print("       ├── bin/")
    print("       │   └── linux-x86_64/    # Target-specific builds")
    print("       │       └── AROS/")
    print("       │           └── Developer/")
    print("       │               └── include/  # Generated headers")
    print("       └── gen/                 # Generated files")
    print()
    print("2. BUILD WITHIN SOURCE DIRECTORY")
    print("   Everything is contained within the source directory.")
    print()
    print("   Example structure:")
    print("   /home/user/aros-source/")
    print("   ├── configure")
    print("   ├── mmakefile")
    print("   ├── compiler/")
    print("   ├── bin/                     # Build output")
    print("   │   └── linux-x86_64/")
    print("   │       └── AROS/")
    print("   │           └── Developer/")
    print("   │               └── include/")
    print("   └── gen/")
    print()
    print("DETECTION PROCESS:")
    print("- The script first looks for source indicators")
    print("- Then looks for build indicators")
    print("- If source found but no build, checks sibling directories for build")
    print()
    print("USAGE RECOMMENDATIONS:")
    print("- Run this script from your source directory for best auto-detection")
    print("- Use --interactive mode for guided setup")
    print("- The generated .clangd file should be placed in your source root")
    print()


def detect_target_architecture(build_dir: str) -> Optional[str]:
    """Try to auto-detect target architecture from build directory."""
    targets = list_target_architectures(build_dir)
    return targets[0] if targets else None


# AROS target directories are named <platform>-<cpu>: raspi-aarch64,
# raspi-arm, pc-x86_64, linux-x86_64, amiga-m68k, opensbi-riscv64.
# The cpu suffix is what decides the triple.
CPU_TRIPLES: Dict[str, str] = {
    "aarch64": "aarch64-unknown-aros",
    "arm": "arm-unknown-aros",
    "armeb": "armeb-unknown-aros",
    "i386": "i386-unknown-aros",
    "m68k": "m68k-unknown-aros",
    "ppc": "powerpc-unknown-aros",
    "riscv": "riscv32-unknown-aros",
    "riscv64": "riscv64-unknown-aros",
    "x86_64": "x86_64-unknown-aros",
}

# Extra flags clang needs per cpu to match how AROS is actually built.
CPU_FLAGS: Dict[str, List[str]] = {
    # Every live AROS arm target (raspi, efika) is hard-float; without this
    # clang warns "unknown platform, assuming -mfloat-abi=soft".
    "arm": ["-mfloat-abi=hard"],
}


def get_target_cpu(target_arch: str) -> str:
    """Extract the cpu part of an AROS target name (raspi-aarch64 -> aarch64)."""
    return target_arch.rsplit("-", 1)[-1]


def get_target_triple(target_arch: str) -> str:
    """Convert AROS target architecture to clang target triple."""
    cpu = get_target_cpu(target_arch)
    if cpu not in CPU_TRIPLES:
        print(f"Warning: unknown cpu '{cpu}' in target '{target_arch}'; "
              f"falling back to {cpu}-unknown-aros.")
        print(f"         Known cpus: {', '.join(sorted(CPU_TRIPLES))}")
    return CPU_TRIPLES.get(cpu, f"{cpu}-unknown-aros")


def discover_kernel_dirs(source_dir: str, cpu: str) -> List[str]:
    """
    Find arch/<dir>/kernel directories that apply to this cpu. Data driven
    rather than a hardcoded list, so new ports (riscv64-opensbi and friends)
    are picked up without touching this script.
    """
    arch_dir = Path(source_dir) / "arch"
    if not arch_dir.is_dir():
        return []

    try:
        candidates = sorted(d.name for d in arch_dir.iterdir() if d.is_dir())
    except (OSError, PermissionError):
        return []

    # riscv64 ports also carry generic riscv code, and every native port
    # shares arch/all-native.
    prefixes = [f"{cpu}-"]
    if cpu == "riscv64":
        prefixes.append("riscv-")
    prefixes.append("all-native")

    return [name for name in candidates
            if any(name.startswith(p) or name == p.rstrip("-") for p in prefixes)
            and (arch_dir / name / "kernel").is_dir()]


def get_kernel_pathmatch(cpu: str) -> str:
    """PathMatch regex covering kernel-ish sources for this cpu."""
    alts = ["rom/kernel", f"arch/{cpu}-[^/]*", "arch/all-native", "bootstrap"]
    if cpu == "riscv64":
        alts.insert(2, "arch/riscv-[^/]*")
    return "(.*/)?(" + "|".join(alts) + ")/.*"


def generate_config(template_path: str, source_dir: str, build_dir: str,
                   target_arch: str, output_path: str) -> bool:
    """Generate .clangd config from template."""

    if not os.path.exists(template_path):
        print(f"Error: Template file '{template_path}' not found.")
        return False

    try:
        with open(template_path, 'r', encoding='utf-8') as f:
            template_content = f.read()
    except (OSError, UnicodeDecodeError) as e:
        print(f"Error reading template file: {e}")
        return False

    cpu = get_target_cpu(target_arch)

    # Multi-line blocks first: these replace a whole placeholder line.
    kernel_includes = [f"-I{os.path.join(source_dir, 'arch', name, 'kernel')}"
                       for name in discover_kernel_dirs(source_dir, cpu)]
    config_content = substitute_block(template_content,
                                      "AROS_KERNEL_INCLUDES", kernel_includes)
    config_content = substitute_block(config_content,
                                      "AROS_CPU_FLAGS", CPU_FLAGS.get(cpu, []))

    # Plain scalar substitutions.
    for token, value in (
        ("AROS_SOURCE_DIR", source_dir),
        ("AROS_BUILD_DIR", build_dir),
        ("AROS_TARGET_ARCH", target_arch),
        ("AROS_TARGET_TRIPLE", get_target_triple(target_arch)),
        ("AROS_TARGET_CPU", cpu),
        ("AROS_KERNEL_PATHMATCH", get_kernel_pathmatch(cpu)),
    ):
        config_content = config_content.replace("${%s}" % token, value)

    leftovers = re.findall(r"\$\{[A-Z_]+\}", config_content)
    if leftovers:
        print(f"Error: unsubstituted template variables: {', '.join(sorted(set(leftovers)))}")
        return False

    # Write via a temporary file so a failure cannot leave a truncated .clangd.
    try:
        out_dir = os.path.dirname(os.path.abspath(output_path))
        with tempfile.NamedTemporaryFile('w', encoding='utf-8', dir=out_dir,
                                         delete=False) as f:
            f.write(config_content)
            tmp_path = f.name
        os.replace(tmp_path, output_path)
        print(f"Generated .clangd configuration: {output_path}")
        return True
    except OSError as e:
        print(f"Error writing config file: {e}")
        return False


def interactive_mode():
    """Interactive mode for gathering user input."""
    print("AROS .clangd Configuration Generator")
    print("=" * 40)

    # Try to auto-detect source and build directories
    detected_source = detect_source_directory()
    detected_build = detect_build_directory()

    if detected_source:
        print(f"✓ Auto-detected AROS source directory: {detected_source}")
        use_detected_source = input("Use this source directory? [Y/n]: ").strip().lower()
        if use_detected_source in ('', 'y', 'yes'):
            source_dir = detected_source
        else:
            source_dir = input("Enter AROS source directory path: ").strip()
    else:
        print("❌ Could not auto-detect AROS source directory.")
        source_dir = input("Enter AROS source directory path: ").strip()

    source_dir = os.path.abspath(os.path.expanduser(source_dir))
    if not validate_directory(source_dir, "AROS source"):
        sys.exit(1)

    if detected_build:
        print(f"✓ Auto-detected AROS build directory: {detected_build}")
        use_detected = input("Use this build directory? [Y/n]: ").strip().lower()
        if use_detected in ('', 'y', 'yes'):
            build_dir = detected_build
        else:
            build_dir = input("Enter AROS build directory path: ").strip()
    else:
        print("❌ Could not auto-detect AROS build directory.")
        if detected_source:
            source_parent = str(Path(detected_source).parent)
            print(f"💡 Hint: Build directory is often a sibling of source directory.")
            print(f"   Source found at: {detected_source}")
            print(f"   Check parent directory: {source_parent}")
            print(f"   Common build dir names: aros-build, abiv1, build, out")
        print()
        build_dir = input("Enter AROS build directory path: ").strip()

    # Expand user path
    build_dir = os.path.expanduser(build_dir)

    # Validate build directory
    if not validate_directory(build_dir, "AROS build"):
        sys.exit(1)

    # Try to auto-detect target architecture
    detected_target = detect_target_architecture(build_dir)
    if detected_target:
        print(f"Auto-detected target architecture: {detected_target}")
        use_detected_target = input("Use this target? [Y/n]: ").strip().lower()
        if use_detected_target in ('', 'y', 'yes'):
            target_arch = detected_target
        else:
            target_arch = input("Enter target architecture (e.g., linux-x86_64): ").strip()
    else:
        print("Could not auto-detect target architecture.")
        print("Common targets: raspi-aarch64, raspi-arm, pc-x86_64, pc-i386,")
        print("                linux-x86_64, amiga-m68k, opensbi-riscv64")
        target_arch = input("Enter target architecture: ").strip()

    # Validate target directory
    target_path = os.path.join(build_dir, "bin", target_arch)
    if not validate_directory(target_path, f"Target ({target_arch})"):
        print("Warning: Target directory structure may not be complete.")
        print(f"Expected structure: {build_dir}/bin/{target_arch}/AROS/")

        available = list_target_architectures(build_dir)
        if available:
            print(f"Available targets: {', '.join(available)}")

    # Output file
    default_output = ".clangd"
    output_path = input(f"Output file [{default_output}]: ").strip()
    if not output_path:
        output_path = default_output

    return source_dir, build_dir, target_arch, output_path


def main():
    parser = argparse.ArgumentParser(
        description="Generate .clangd configuration for AROS development",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Common Directory Structures:
  Structure 1 (Separate directories):
    /home/user/aros/
    ├── aros-source/     # Source code (configure, mmakefile, etc.)
    └── aros-build/      # Build output (bin/, gen/, etc.)

  Structure 2 (Build within source):
    /home/user/aros-source/
    ├── configure        # Source files
    ├── mmakefile
    └── bin/             # Build output

Examples:
  %(prog)s --interactive
  %(prog)s --build-dir /home/user/aros-build --target raspi-aarch64
  %(prog)s -b ~/abiv1 -t pc-x86_64 -o custom.clangd
  %(prog)s --help-structure  # Show detailed directory structure info
        """
    )

    parser.add_argument(
        "--interactive", "-i",
        action="store_true",
        help="Run in interactive mode"
    )

    parser.add_argument(
        "--source-dir", "-s",
        help="AROS source directory path (default: auto-detected)"
    )

    parser.add_argument(
        "--build-dir", "-b",
        help="AROS build directory path"
    )

    parser.add_argument(
        "--target", "-t",
        help="Target architecture (e.g., raspi-aarch64, pc-x86_64)"
    )

    parser.add_argument(
        "--output", "-o",
        default=".clangd",
        help="Output file path (default: .clangd)"
    )

    parser.add_argument(
        "--template",
        default="scripts/.clangd.template",
        help="Template file path (default: .clangd.template)"
    )

    parser.add_argument(
        "--help-structure",
        action="store_true",
        help="Show detailed information about AROS directory structures"
    )

    args = parser.parse_args()

    # Show directory structure help
    if args.help_structure:
        print_directory_structure_help()
        return

    # Interactive mode
    if args.interactive or (not args.build_dir and not args.target):
        source_dir, build_dir, target_arch, output_path = interactive_mode()
    else:
        if not args.build_dir or not args.target:
            parser.error("--build-dir and --target are required in non-interactive mode")

        source_dir = args.source_dir or detect_source_directory()
        if not source_dir:
            parser.error("could not auto-detect the AROS source directory; "
                         "pass --source-dir")
        source_dir = os.path.abspath(os.path.expanduser(source_dir))
        if not validate_directory(source_dir, "AROS source"):
            sys.exit(1)

        build_dir = os.path.expanduser(args.build_dir)
        target_arch = args.target
        output_path = args.output

        # Validate directories
        if not validate_directory(build_dir, "AROS build"):
            print("Build directory validation failed.")
            print("Expected structure:")
            print("  build-directory/")
            print("    bin/")
            print("      <target-arch>/")
            print("        AROS/")
            print("          Developer/")
            print("            include/")
            sys.exit(1)

        target_path = os.path.join(build_dir, "bin", target_arch)
        if not validate_directory(target_path, f"Target ({target_arch})"):
            print("Warning: Target directory structure may not be complete.")
            print(f"Expected: {target_path}/AROS/Developer/include/")

            # Show available targets
            available = list_target_architectures(build_dir)
            if available:
                print(f"Available targets: {', '.join(available)}")

    # Generate configuration
    success = generate_config(
        args.template,
        source_dir,
        build_dir,
        target_arch,
        output_path
    )

    if success:
        print(f"\nConfiguration generated successfully!")
        print(f"Source directory: {source_dir}")
        print(f"Build directory: {build_dir}")
        print(f"Target architecture: {target_arch}")
        print(f"Target triple: {get_target_triple(target_arch)}")

        # Show key paths that will be used
        developer_path = os.path.join(build_dir, "bin", target_arch, "AROS", "Developer", "include")
        if os.path.exists(developer_path):
            print(f"✓ AROS headers found: {developer_path}")
        else:
            print(f"⚠ AROS headers not found at: {developer_path}")

        print(f"\nYou can now use clangd with your AROS project.")
        print("Place the generated .clangd file in your source directory root.")
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
