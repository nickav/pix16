# pix16

A fantasy console written in C from scratch.

## Getting Started

### Windows

Install `cl.exe` from Microsoft Visual Studio Community Edition (recommended version: [2019](https://visualstudio.microsoft.com/vs/older-downloads/)).
Go through the install flow and install `C++ compiler tools`.

Then, in a Command Prompt run the following script:

```batch
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
```

This will set up the environment variables to compile for x64 devices (for this session only).

Now you should be able to run `build.bat` in the project root.

## Building

### Windows

To build the project run:

```batch
build.bat
```

This will create a debug build in the `build` directory.

## Release Builds

### Windows

To build the project for release, run:

```batch
dist.bat
```

This will create a release build in the `build` directory and a distributable `pix16_win32.zip` file.


## Other Platforms

The project has only been set up for Windows so far. Other operating systems can be added by creating an entry point file and build scripts for other platforms.

You could even add a backend that targets [SDL2](https://www.libsdl.org/) to support all the operating systems that it supports!