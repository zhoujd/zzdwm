zzdwm
=====

## URLs

```
## https://dwm.suckless.org/
## https://dl.suckless.org/dwm/
## https://dl.suckless.org/dwm/dwm-6.5.tar.gz
## https://git.suckless.org/dwm/
## https://git.suckless.org/dwm/files.html
```

## Build dependence

```bash
$ sudo apt install make gcc g++
$ sudo apt install libx11-dev libxft-dev libxinerama-dev libxrender-dev
$ sudo apt install libasound2-dev
$ sudo apt install libxfixes-dev libxi-dev libxext-dev
$ sudo apt install liblibxrandr-dev
$ sudo apt install libxcomposite-dev libxdamage-dev
```

## Build dwm source

```bash
$ cd src
$ make
$ sudo make install
```

## Init dwm

```bash
$ ./init.sh
$ ./init.sh all
```
