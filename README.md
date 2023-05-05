# Memory Deallocation and Safety for TIP
## Overview

This project contains an extension of the TIP compiler to support memory deallocation as well as an LLVM pass to address the memory safety violations that can arise from deallocation. This repository is adapted based on the [tipc](https://github.com/matthewbdwyer/tipc) compiler, written by Matthew Dwyer and course staff for the `CS 6620: Compilers` course at the University of Virginia.

## Getting Started for TIPC
The follwing instructions are taken from the original tipc repository, and will produce a working version of the TIP compiler. 

### Dependencies

`tipc` is implemented in C++17 and depends on a number of tools and packages, e.g., [ANTLR4](https://www.antlr.org), [Catch2](https://github.com/catchorg/Catch2), [CMake](https://cmake.org/), [Doxygen](https://www.doxygen.nl/), [loguru](https://github.com/emilk/loguru), [Java](https://www.java.com), [LLVM](https://www.llvm.org).  To simplify dependency management the project provides a [bootstrap](bin/bootstrap.sh) script to install all of the required dependencies on linux ubuntu and mac platforms.

### Building tipc

The project uses [GitHub Actions](https://docs.github.com/en/actions) for building and testing and [CodeCov](https://codecov.io) for reporting code and documentation coverage.  The [build-and-test.yml](.github/workflows/build-and-test.yml) file provides details of this process.  If you would prefer to build and test manually then read on.

After cloning this repository you can build the compiler by moving to into the top-level directory and issuing these commands:
  1. `./bin/bootstrap.sh`
  2. `. ~/.bashrc`
  3. `mkdir build`
  4. `cd build`
  5. `cmake ..`
  6. `make`

The build process will download an up to date version of ANTLR4 if needed, build the C++ target for ANTLR4, and then build all of `tipc` including its substantial body of unit tests.  This may take some time - to speed it up use multiple threads in the `make` command, e.g., `make -j6`.

You may see some warnings, e.g., CMake policy warnings, due to some of the packages we use in the project.  As those projects are updated, to avoid CMake feature deprecation, these will go away.

When finished the `tipc` executable will be located in `build/src/`.  You can copy it to a more convenient location if you like, but a number of scripts in the project expect it to be in this location so don't move it.

The project includes more than 300 unit tests grouped into several executables. The project also includes more than 90 system tests. These are TIP programs that have built in test oracles that check for the expected results. For convenience, there is a `runtests.sh` script provided in the `bin` directory.  You can run this script to invoke the entire collection of tests. See the `README` in the bin directory for more information.  

All of the tests should pass.

#### Ubuntu Linux

Our continuous integration process builds on both Ubuntu 18.04 and 20.04, so these are well-supported.  We do not support other linux distributions, but we know that people in the past have ported `tipc` to different distributions. 

#### Mac OS

Our continuous integration process builds on Mac OS 12, so modern versions of Mac OS are well-supported.  `tipc` builds on both Intel and Apple Silicon, i.e., Apple's M1 ARM processor.  

#### Windows Subsystem for Linux

If you are using a Windows machine, tipc can be built in the Windows Subsystem for Linux (WSL). [Here](https://docs.microsoft.com/en-us/windows/wsl/install-win10#update-to-wsl-2) are instructions to install WSL and upgrade to WSL2. It is highly recommended to upgrade to WSL2. Once installed, you should install
[Ubuntu 20.04](https://docs.microsoft.com/en-us/windows/wsl/install-win10#update-to-wsl-2). Once finished, you can open a virtual instance of Ubuntu and follow 
the instructions above to build tipc. 

You may recieve an error saying "No CMAKE_CXX_COMPILER could be found" when running `cmake ..`. If this is the case, you should install g++ with the command: `sudo apt-get install g++`.


## Getting Started for LLVM Pass
MemorySafetyPass, as well as a number of scripts and tests are provided under the directory `detection_passes`. The scripts can be used as follows:
- `detection_passes/rebuild.sh` takes no argument, and will compile the pass into a dylib.
- `detection_passes/compile_and_disassemble_tip.sh` takes a TIP file as argument, and will compile it into LLVM IR, then disassemble it into a human-readable format. Both of which are stored in the same directory as the TIP file.
- `detection_passes/test_on_bc.sh` takes a .bc file as argument, and will run the pass on it. The output will be printed to stdout, where the memory safety violations are listed at the end of the passes's output. 

The tests are located under `detection_passes/tests`. Each test is a TIP program written to produce a specific type of violation. 

