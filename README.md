zzdwm
=====

## URLs

    ## https://dwm.suckless.org/
    ## https://dl.suckless.org/dwm/
    ## https://dl.suckless.org/dwm/dwm-6.5.tar.gz

## Build dependence

    $ sudo apt install libx11-dev libxft-dev libxinerama-dev libxrender-dev
    $ sudo apt install libasound2-dev
    $ sudo apt install libxfixes-dev libxi-dev libxext-dev
    $ sudo apt install liblibxrandr-dev
    $ sudo apt isntall libxcomposite-dev libxdamage-dev

## Build dwm source

    $ cd src
    $ make
    $ sudo make install

## Init dwm

    $ ./init.sh
    $ ./init.sh all
