# WR80-ImageBuilder

The **WR80IMG Image Builder** is a tool for **WR80** that generates ROM images compatible with the **WROS WR80FS** file system.

---

## Build Instructions

Before building, make sure you have a compatible **C compiler** (such as GCC) and **GNU Make** installed on your system.

Use the provided **Makefile** to build, install, and manage the Image Builder utility.

| Command | Description |
|----------|--------------|
| `make` | Builds the Image Builder in the `/build` directory. |
| `make install` | Builds and installs the Image Builder system-wide, allowing it to be executed from any directory. |
| `make update` | Checks for updates in the WR80 development kit repository and rebuilds the tool if a newer version is available. |
| `make clear` | Removes all previously generated binaries and build artifacts. |
| `make uninstall` | Uninstalls the WR80IMG Image Builder from the system. |

---

## Notes

- The tool automatically verifies version information from the official WR80 repository when building.  
- Internet access is required for version checks and the `make update` command.  
- Built binaries are placed inside the `/build` directory by default.