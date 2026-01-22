#!/usr/bin/env python3

import os
import subprocess
import sys
import argparse

def main():
    parser = argparse.ArgumentParser(description="Build the library using CMake.")
    # Keep arguments for backward compatibility but ignore them (or warn)
    parser.add_argument('--configFileName', type=str, help="Deprecated/Ignored")
    parser.add_argument('--libname', type=str, help="Deprecated/Ignored")
    args = parser.parse_args()

    print("--- Configuring with CMake ---")
    # Generate build files in 'build' directory
    # -S . : source directory is current directory
    # -B build : build directory
    cmd_config = ["cmake", "-S", ".", "-B", "build"]
    print(f"Executing: {' '.join(cmd_config)}")
    try:
        subprocess.run(cmd_config, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Error referencing CMake configuration: {e}")
        sys.exit(1)

    print("\n--- Building with CMake ---")
    # Build the project
    cmd_build = ["cmake", "--build", "build"]
    print(f"Executing: {' '.join(cmd_build)}")
    try:
        subprocess.run(cmd_build, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Error during build: {e}")
        sys.exit(1)
        
    print("\nOrder 66 executed: Build successful.")

if __name__ == "__main__":
    # Change to script directory to ensure relative paths work if run from elsewhere
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)
    main()