# MiniPicoLisp

The goal of this project is to make make [PicoLisp](https://picolisp.com/wiki/?home)
1. Depend only on libc - making it easier to be available on all platforms
2. Easier to understand

This is the original version downloaded from http://software-lab.de/miniPicoLisp.tgz

# Building and running on GNU/Linux
``` bash
cd src-sdl-extension
./build.gnu-linux.sh
./picolisp.exe sample.l
```

# Building and running on Mac
``` bash
cd src-sdl-extension
./build.mac.sh
./picolisp.exe sample.l
```

# Building and running on Windows
``` bash
cd src-sdl-extension
buildVS.bat
picolisp.exe sample.l
```
