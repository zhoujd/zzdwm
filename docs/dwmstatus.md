dwmstatus
=========

## URLs

```
## https://dwm.suckless.org/status_monitor/
## https://git.suckless.org/dwmstatus/
```

## Time Zone

```
## TZ: Asia/Shanghai
## https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
```

## Volume Status

```bash
## Install ALSA dependece
$ sudo apt install -y libasound2-dev

## Set volume to 50%
$ amixer -D pulse sset Master 50%

## Increase volume by 5%
$ amixer -D pulse sset Master 5%+
## Decrease volume by 5%
$ amixer -D pulse sset Master 5%-

## Toggle sound
$ amixer -D pulse set Master toggle
```
