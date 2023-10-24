# Secured Mailbox

A university project implementing a simple message exchange system using a Linux character device driver and a multithreaded C application on Raspberry Pi 2.

The project covers communication between user space and kernel space, POSIX threads, semaphores, device drivers, bitwise operations and direct GPIO register access.

## How it works

The user application consists of three worker threads and an additional thread for user input.

1. **Thread 1 - Sender:** Every two seconds, generates a random message of up to 50 characters and writes it to the character device in encryption mode.
2. **Thread 2 - Transmission:** Reads the encrypted message from the device, prints it and forwards it back to the driver in decryption mode. Message corruption can be simulated through error injection.
3. **Thread 3 - Receiver:** Reads the processed message from the device and prints the result.

POSIX semaphores coordinate the execution order of the threads, while a mutex is used to protect shared data during parts of the processing.

The application communicates with the kernel driver through `/dev/secure_mailbox`, using `open()`, `read()`, `write()` and `ioctl()`.

## Kernel Driver

The Linux character device driver provides two operating modes, selected through `ioctl()`:

- **Encryption mode (1):** Transforms message bytes using bit rotation and bitwise NOT operations.
- **Decryption mode (0):** Provides the corresponding inverse transformation.

The encryption algorithm supports left or right bit rotation by a configurable number of positions, followed by bitwise inversion. Decryption reverses these operations.

The driver uses `copy_from_user()` and `copy_to_user()` to transfer data between user space and kernel space.

## Raspberry Pi GPIO

The driver also includes direct GPIO register manipulation for controlling the Raspberry Pi's green activity LED (GPIO 47).

The GPIO register address space is mapped into kernel virtual memory using `ioremap()`. Register access is performed through `ioread32()` and `iowrite32()`.

The LED is controlled using the GPIO set/clear registers, and the mapped memory is released using `iounmap()` when the module is unloaded.

## Implementation Notes

- **Language:** C
- **Platform:** Raspberry Pi 2, Linux
- **Kernel interface:** Character device driver, `ioctl`, `read`, `write`
- **Concurrency:** POSIX threads, semaphores, mutex
- **Hardware interface:** Memory-mapped GPIO registers
- **Build:** Makefiles and shell scripts

The project was developed as part of university coursework. It demonstrates the interaction between a multithreaded user-space application, a Linux kernel module and Raspberry Pi hardware.

The encryption is a simple reversible bitwise transformation intended for demonstration, not cryptographic security.
