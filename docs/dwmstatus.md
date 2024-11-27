dwmstatus
=========

## URLs

    ## https://dwm.suckless.org/status_monitor/
    ## https://git.suckless.org/dwmstatus/

## Time Zone

    ## TZ: Asia/Shanghai
    ## https://en.wikipedia.org/wiki/List_of_tz_database_time_zones

## Volume Status

    ## Install ALSA dependece
    $ sudo apt install -y libasound2-dev

    ## Increase volume by 5%
    $ amixer sset Master 5%+
    ## Decrease volume by 5%
    $ amixer sset Master 5%-
    ## Set volume to 50%
    $ amixer sset Master 50%
    ## Toggle sound
    $ amixer set Master toggle
