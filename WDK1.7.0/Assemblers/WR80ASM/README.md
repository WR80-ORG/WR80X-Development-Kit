# WR80-Emulator

The **WR80EMU Emulator** is a software emulator for WR80-based systems, capable of executing WR80 programs and simulating their CPU, memory, and hardware behavior.  
It works alongside the **WR80DBG** Debugger to provide runtime control, inspection, and testing capabilities.

---

## Build Instructions

Before building, ensure you have a compatible **C compiler** (such as GCC) and **GNU Make** installed on your system.

Use the provided **Makefile** to build, install, and manage the Emulator utility.

| Command | Description |
|----------|--------------|
| `make` | Builds the Emulator in the `/build` directory. |
| `make install` | Builds and installs the Emulator system-wide, allowing it to be executed from any directory. |
| `make update` | Checks for updates in the WR80 development kit repository and rebuilds the tool if a newer version is available. |
| `make clear` | Removes all previously generated binaries and build artifacts. |
| `make uninstall` | Uninstalls the WR80EMU Emulator from the system. |

---

## Notes

- The Emulator communicates with the **WR80DBG** Debugger over a local TCP connection to manage program execution and provide CPU state information.
- Internet access is required for version checks and the `make update` command.
- Built binaries are placed inside the `/build` directory by default.