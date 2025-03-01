# Diary-A

A simple local logbook/diary. Read carefully.

Unfortunatelly for now, both for development and usage, the project is only available for Linux machines.

## License

This project is licensed under the GNU GPL-3.0 license.

Main purpose is to provide a free and open-source software for enthusiasts. Feel free to use the source code. Referring to the repository would be very much appreciated.

## Setup

### Development

The project is written in C++ 17, using GCC 14.2.1 compiler.

Main dependencies are QT6, ssl and crypto packages. For the rest, the Make rules are handling most parts.

After installing the dependencies, you can clone the repository and run the following commands:

```bash
$ make
```

It should compile the project and create an executable file named `diary.a` and run it.

Due to the current simplicity of the idea, the entire code is written in `src/main.cpp` file. It may change along the way.

### Installation

If you want to install the project, the only dependency is the openssl package. After installing it, you can clone the repository and run the following commands:

```bash
$ make install
```

It will directly have the latest stable version of the project installed on your machine.

In the process, it will ask for a RSA password, which will be used to encrypt the entries. It is important to remember this password, as it will be required to access the entries.

## The Project

The main idea is to have a simple logbook in your machine to easily note down your thoughts.

The main feature is the encryption of the entries, which is done using RSA encryption. The password is required to access the entries, and it is not stored in the machine.

User data is by default stored in the `~/.diary_a` directory. After the installation, public & private keys are generated in this directory. The entries are stored in the `entries` file, which is encrypted using the public key.

## To-Do

- [x] Adding RSA encryption for the entries, requiring first-time user to set a password.
- [x] Creating a basic setup for Linux machines, with config file for variables.
- [ ] Sorting buttons for the entries.
- [ ] Adding a search bar for the entries.
- [ ] Cleaning the scripts for a better C++ structure.