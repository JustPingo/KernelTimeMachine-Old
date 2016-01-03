# KernelTimeMachine
![Project Status](https://img.shields.io/badge/status-in%20progress-yellow.svg)

Kernel Time Machine is a work in progress tool to downgrade firmware on the 3DS safely using TPP files.

Visit [this GBAtemp thread](https://gbatemp.net/threads/clarification-thread-what-is-going-on.407074/) to learn more on what this project is all about.

## This is still a WIP.
Please note that there might be some typos and mistakes due to the fact that I'm not using an IDE and did not try to compile it yet. It shouldn't work yet. Please note that the main file is pretty big for now, and will probably get splitted up later on.

## Building
> This is an **unstable** work in progress.  Use at your own risk.

The program requires [devkitARM](http://devkitpro.org/)

Run the `make` command on the KernelTimeMachine main folder. (If you are on Windows, you can use [MinGW](http://www.mingw.org/) or [Cygwin](http://www.cygwin.com) and on Mac use Xcode or install make [manually](http://stackoverflow.com/questions/2556444/install-make-command-without-already-having-make-mac-os-10-5))

## Installing
After building, place the files created by make in the 3ds folder on your SD card and launch it through the [homebrew launcher](http://smealum.github.io/3ds/).
