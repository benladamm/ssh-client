# Modern SSH Client

A modern, lightweight SSH client built with C and GTK4.

## Features

- **SSH Terminal**: Full-featured terminal emulation using VTE.
- **SFTP Support**: File transfer capabilities.
- **Host Management**: Save and organize your SSH connections (SQLite backend).
- **Modern UI**: Clean interface built with GTK4.
- **Themes**: Support for Light and Dark themes.

## Prerequisites

To build this project, you need the following dependencies installed on your system:

- C Compiler (GCC/Clang)
- CMake (3.17+)
- GTK4
- libssh
- vte-2.91-gtk4
- sqlite3
- pkg-config

### Installing dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential cmake libgtk-4-dev libssh-dev libvte-2.91-gtk4-dev libsqlite3-dev
```

## Build Instructions

1. Clone the repository:
   ```bash
   git clone https://github.com/benladamm/ssh-client.git
   cd ssh-client
   ```

2. Create a build directory and compile:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. Run the application:
   ```bash
   ./modern_ssh
   ```

## Usage

- **Home**: Quick access to recent connections.
- **Hosts**: Manage your saved servers. Add, edit, or delete hosts.
- **Terminal**: Connect to a server to open a terminal session.
- **SFTP**: Transfer files between your local machine and the server.
- **Settings**: Change application theme and view about information.
