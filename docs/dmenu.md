dmenu
=====

## URLs

    ## https://tools.suckless.org/dmenu/
    ## https://dl.suckless.org/tools/dmenu-5.3.tar.gz
    ## https://tools.suckless.org/dmenu/patches/center/

## Scripts

    ## https://tools.suckless.org/dmenu/scripts/
    ## https://github.com/jukil/dmenu-scripts-collection
    ## https://github.com/aario/dmenu

## Options

    $ cat > ~/.dmenurc <<EOF
    ## define the font for dmenu to be used
    DMENU_FN="SF Mono-11"
    ## background colour for unselected menu-items
    DMENU_NB="#4a4a4a"
    ## textcolour for unselected menu-items
    DMENU_NF="#F9FAF9"
    ## background colour for selected menu-items
    DMENU_SB="#eb564d"
    ## textcolour for selected menu-items
    DMENU_SF="#F9FAF9"
    ## command for the terminal application to be used:
    TERMINAL_CMD="st -e"
    ## export our variables
    DMENU_OPTIONS="-fn $DMENU_FN -nb $DMENU_NB -nf $DMENU_NF -sf $DMENU_SF -sb $DMENU_SB"
    export DMENU_FN DMENU_NB DMENU_NF DMENU_SB DMENU_SF TERMINAL_CMD DMENU_OPTIONS
    EOF

## keysym for Shift + Tab

    ## https://www.cl.cam.ac.uk/~mgk25/ucs/keysymdef.h
    ## XK_ISO_Left_Tab

## Enter sudo password

    ## apply dmenu-password-5.0.diff
    $ wget https://tools.suckless.org/dmenu/patches/password/dmenu-password-5.0.diff
    $ patch -p1 < dmenu-password-5.0.diff

    ## enter password via dmenu
    password=$(echo -n | dmenu -i -P -p "Enter password:")
    echo $password | sudo reboot

    ## sudocmd
    sudocmd() {
        password=$(echo -n | dmenu -i -P -p "Enter password:")
        if [ -n "$password" ]; then
            echo $password | sudo -S $@
        fi
    }
    sudocmd shutdown -h now
    sudocmd reboot
