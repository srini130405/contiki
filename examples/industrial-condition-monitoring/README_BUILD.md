# Industrial Condition Monitoring — Build and Flash Guide

This guide explains how to set up the Windows development environment, build the coordinator and node firmware, flash JN5168 dongles, and view their serial output. 

## 1. Project overview

The project is a wireless industrial condition-monitoring network built on **Contiki**, **NXP JN5168** USB dongles, and **TSCH** (Time-Slotted Channel Hopping), with IPv6/RPL networking and UDP application traffic.

The intended application behavior is:

- **Normal operation:** each machine's sensor node processes vibration measurements locally and periodically sends compact features or status information to the coordinator.
- **Abnormal operation:** when edge inference detects a potentially critical condition, the node sends an alert and transmits diagnostic data so the coordinator can inspect the event.

The current codebase is the networking foundation for this application. Check the companion project documentation and current source code to distinguish features already implemented from application behavior that the team still needs to develop.

## 2. Clone the repository

Install Git for Windows, open PowerShell, and run:

```powershell
git clone --branch industrial-monitoring --single-branch https://github.com/srini130405/contiki.git
cd contiki
```

The project is located at:

```text
examples/industrial-condition-monitoring/
```

Run the build commands from that directory, inside the Cygwin or WSL environment described below.

## 3. Install the NXP tools and SDK

Obtain the following NXP packages supplied for this project. Package names can be easy to confuse, so use the indicated archive names:

| Archive | Purpose |
|---|---|
| `jn-sw-4107.zip` | NXP JN51xx Production Flash Programmer / flashing utility |
| `jn-sw-4141.zip` | `ba-elf-gcc` compiler toolchain |
| `jn-sw-4163.zip` | JN516x IEEE 802.15.4 SDK and headers/libraries used by the target platform |

Extract each archive to a known location. Keep track of the paths you choose; the commands below use example paths and must be adjusted if your installation differs.

### 3.1 Add the compiler to `PATH`

The build system must be able to find `ba-elf-gcc`. If you extracted the compiler to `/usr/ba-elf-gcc` in your Cygwin environment, run this in the Cygwin terminal:

```bash
export PATH="/usr/ba-elf-gcc/bin:$PATH"
```

Check that the compiler can be found:

```bash
which ba-elf-gcc
ba-elf-gcc --version
```

If `which` returns nothing, update the path to match the directory where you extracted the toolchain. To retain the setting for future Cygwin sessions, add the `export PATH=...` line to `~/.bashrc` and reopen the terminal.

### 3.2 SDK location

The SDK must remain available at the location expected by the project/build configuration. In the development setup used for this project, the extracted SDK was located under:

```text
/usr/jn516x-sdk/JN-SW-4163
```

If you choose another location, check the project's build configuration and update any SDK path references accordingly. Do not assume that extracting the SDK alone automatically configures the compiler or the build system.

## 4. Install Cygwin or use WSL

Contiki's Makefiles need a Unix-like shell and `make`. On Windows, use **Cygwin** or **WSL**.

### Option A: Cygwin

Install Cygwin and ensure that the installation includes the `make` utility and the shell tools required by Contiki's build system. Open the Cygwin terminal and navigate to the cloned repository. Windows paths are exposed in Cygwin under `/cygdrive`, for example:

```bash
cd /cygdrive/c/Users/<your-Windows-username>/Documents/contiki/examples/industrial-condition-monitoring
```

Then set the compiler `PATH` as described above.

### Option B: WSL

Install a WSL distribution and the required build utilities, including `make`. Clone the repository into the WSL filesystem or navigate to its mounted Windows location. Ensure the NXP compiler is executable and compatible with the environment in which you run the build. A Windows-native compiler/toolchain path is not automatically available as a Linux path; configure the toolchain for the same environment as `make`.

> **Important:** Cygwin and WSL paths are different. Use the paths appropriate to the environment in which you run `make`; do not copy `/usr/...` paths blindly between them.

## 5. Build the firmware

Open the Cygwin/WSL terminal and move into the project directory:

```bash
cd /path/to/contiki/examples/industrial-condition-monitoring
```

Build the coordinator firmware:

```bash
make -f Makefile.coord
```

Build the node firmware:

```bash
make -f Makefile.node
```

Resolve any missing-tool or path errors before flashing. The expected firmware images are:

```text
coord.jn516x.bin
node.jn516x.bin
```

They should be in the project directory unless the Makefiles specify a different output location. If the output filenames or locations differ, use the actual generated paths in the flash commands below.

## 6. Identify the dongle's COM port

Connect one JN5168 dongle to the Windows PC. Use Windows Device Manager to identify its COM port (for example, `COM5`). Repeat for each dongle, since each device may be assigned a different port.

Use one dongle as the **coordinator** and the remaining dongles as **nodes**. Flash the correct firmware to each device.

## 7. Flash the firmware

The following examples use the NXP programmer at:

```text
C:\NXP\ProductionFlashProgrammer\JN51xxProgrammer.exe
```

If your programmer is installed elsewhere, change that path. Run these commands in **PowerShell**, not in Cygwin/WSL. Replace `COM5` with the COM port for the dongle you are flashing, and adjust the firmware path to your checkout.

### 7.1 Flash the coordinator

```powershell
& "C:\NXP\ProductionFlashProgrammer\JN51xxProgrammer.exe" `
    -V 10 -s COM5 -I 38400 -P 1000000 `
    -f "C:\Users\<your-Windows-username>\Documents\contiki\examples\industrial-condition-monitoring\coord.jn516x.bin" `
    -v
```

### 7.2 Flash a node

Use the same command with the node's COM port and the node firmware image:

```powershell
& "C:\NXP\ProductionFlashProgrammer\JN51xxProgrammer.exe" `
    -V 10 -s COM6 -I 38400 -P 1000000 `
    -f "C:\Users\<your-Windows-username>\Documents\contiki\examples\industrial-condition-monitoring\node.jn516x.bin" `
    -v
```

Repeat the node command for every node, changing the COM port each time. The `-v` option requests verification after programming. Do not flash the coordinator image to a node or vice versa.

## 8. Monitor serial output with PuTTY

1. Install and open PuTTY.
2. Select **Connection type: Serial**.
3. Set **Serial line** to the dongle's COM port, such as `COM5`.
4. Set **Speed** to `1000000` baud.
5. Under **Connection → Serial**, use **8 data bits, 1 stop bit, no parity, no flow control** (8-N-1, no flow control).
6. Open the serial session. Reset or reconnect the dongle if needed to see its startup messages.

The coordinator should print its startup/network messages and indicate that it is listening for UDP traffic on port `8185`. Nodes should print their TSCH/RPL join status and application messages. Exact output depends on the firmware currently built.

## 9. Suggested startup and smoke test

1. Flash and start the coordinator first.
2. Open PuTTY on the coordinator's COM port and confirm that the coordinator starts and listens on UDP port `8185`.
3. Flash and start the intermediate node, if testing the multi-hop setup.
4. Flash and start the leaf node.
5. Confirm that the nodes join the TSCH/RPL network and that UDP messages reach the coordinator.

The coordinator uses the fixed global IPv6 destination `bbbb::1` in the current codebase. Nodes should send application UDP packets to that address and port `8185`. A node joining the RPL DAG does not by itself prove that its application packets are reaching the coordinator; verify the coordinator's serial output as well.

## 10. Troubleshooting

- **`make: command not found`:** install `make` in Cygwin/WSL and reopen the correct terminal.
- **`ba-elf-gcc: command not found`:** add the compiler's `bin` directory to `PATH` in the same shell where you run `make`.
- **SDK/header/library errors:** verify that the JN-SW-4163 SDK was extracted and that project build paths point to the correct location.
- **Flash programmer cannot connect:** verify the COM port in Device Manager, close PuTTY or any other application using that port, and check the programmer installation path.
- **No readable serial output:** verify the COM port and set PuTTY to `1000000` baud, 8-N-1, no flow control.
- **Node joins but coordinator sees no UDP data:** verify the node's configured destination address (`bbbb::1`), UDP port (`8185`), and coordinator serial output.
- **Coordinator address differs from `bbbb::1`:** confirm that you built and flashed the current coordinator firmware and that the fixed-address initialization in `rpl_tools_init()` is present.

## 11. Before making changes

- Keep coordinator and node builds separate; they use different Makefiles and firmware images.
- Do not commit generated binaries unless the team explicitly decides to version them.
- Commit source/configuration changes to the `industrial-monitoring` branch and share the commit with the team.
- The companion README will explain the source-code architecture and the roles of TSCH, Orchestra, RPL, IPv6, and UDP in more detail.
