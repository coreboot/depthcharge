# Depthcharge

Depthcharge is a bootloader for ChromeOS devices. It is responsible for
initializing hardware, setting up the boot environment, and loading the
operating system.

## Key Features

*   **Hardware Initialization:** Initializes and configures the system's
    hardware components, including memory, storage, and peripherals.
*   **Boot Environment Setup:** Sets up the necessary environment for the
    operating system to boot, including memory, storage, and other peripherals.
*   **Booting the Operating System:** Loads and executes the operating system
    kernel.
*   **Security Features:** Implements various security features, such as secure
    boot and verified boot, using the vboot library.

## Project Structure

*   `src/`: Contains the source code for the depthcharge bootloader.
*   `board/`: Contains the board-specific configuration files.
*   `src/drivers/`: Contains the device drivers.

## Build Instructions

To build depthcharge, you will need a ChromeOS development environment.
Within the chroot:
```bash
emerge-$BOARD sys-boot/depthcharge

Remember to build it before you build `chromeos-bootimage`!

