# WR80-Emulator

The **WR80EMU** is the official emulator for the **WR80 architecture**.

---

## Build Instructions

Before building, make sure you have **GCC** and **GNU Make** installed on your system.

Use the provided **Makefile** to manage the build process.

| Command | Description |
|----------|-------------|
| `make` | Builds the emulator in the `/build` directory. |
| `make install` | Installs the emulator system-wide. |
| `make update` | Updates from the official WR80 repository and rebuilds. |
| `make clear` | Removes build artifacts and binaries. |
| `make uninstall` | Uninstalls the WR80EMU binary. |

---

## Notes

- Communicates with **WR80DBG** via TCP (`127.0.0.1:8080`).  
- Internet access is required for version checks and the `make update` command.
- Built binaries are stored in the `/build` directory by default.