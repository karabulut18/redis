#!/opt/homebrew/bin/python3

import os
import argparse
import sys
import subprocess


class Compiler:
    def __init__(self, configFileName, libName=None):
        self.configFileName = configFileName
        self.libName = libName
        self.sourceFiles = []

    def readConfig(self):
        with open(self.configFileName, "r") as configFile:
            for line in configFile:
                sourceFile = line.strip()
                if not line or line.startswith("#"):
                    continue
                
                if line.startswith("libname"):
                    self.libName = line.split("=")[1].strip()
                    continue
                elif line.startswith("sourceFiles"):
                    fileStart = line.find("{")
                    fileEnd = line.find("}")
                    if fileStart != -1 and fileEnd != -1:
                        sourceFilesStr = line[fileStart+1:fileEnd].strip()
                        self.sourceFiles = [file.strip() for file in sourceFilesStr.split(",")]
                    else:
                        print(f"Warning: 'sourceFiles' line in config file '{self.configFileName}' is malformed. Expected format: sourceFiles = {{file1.cpp, file2.cpp}}", file=sys.stderr)
                        exit(1)


    def compile(self):
        objectFiles = []
        print("--- Compiling source files ---")
        try:
            for sourceFile in self.sourceFiles:
                objectFile = sourceFile.replace(".cpp", ".o")
                # Use subprocess for better security and error handling.
                # Added -std=c++17 and -I. for robustness.
                compileCommand = [
                    "clang++", "-std=c++17", "-I.", "-c", sourceFile, "-o", objectFile
                ]
                print(f"Executing: {' '.join(compileCommand)}")
                # check=True will raise an exception if the command fails.
                subprocess.run(compileCommand, check=True)
                objectFiles.append(objectFile)

            libraryName = "lib" + self.libName + ".a"
            archiveCommand = ["ar", "rcs", libraryName] + objectFiles
            print(f"\n--- Creating static library: {libraryName} ---")
            print(f"Executing: {' '.join(archiveCommand)}")
            subprocess.run(archiveCommand, check=True)
            print(f"\nSuccessfully created {libraryName}")


        except FileNotFoundError as e:
            print(f"Error: Command not found. Is clang++ or ar in your PATH? {e}", file=sys.stderr)
            sys.exit(1)
        except subprocess.CalledProcessError as e:
            print(f"Error: A step failed with exit code {e.returncode}. Aborting.", file=sys.stderr)
            sys.exit(1)
        finally:
            # Ensure temporary object files are always cleaned up.
            if objectFiles:
                print("\n--- Cleaning up object files ---")
                for objectFile in objectFiles:
                    if os.path.exists(objectFile):
                        os.remove(objectFile)

    def run(self):
        self.readConfig()
        self.compile()




if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compiles C++ source files into a static library.")
    parser.add_argument('--configFileName', type=str, required=False, default="libsource.cfg", help="Path to a file listing the source files.")
    parser.add_argument('--libname', type=str, required=False, default=None, help="Name of the static library to create.")
    args = parser.parse_args()

    compiler = Compiler(args.configFileName, args.libname)
    compiler.run()