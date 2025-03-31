lightdm
=======

## Install lightdm

```
$ sudo apt install lightdm
$ sudo systemctl enable lightdm
$ sudo tee /etc/lightdm/lightdm-gtk-greeter.conf <<EOF
[greeter]
theme-name = Adwaita-dark
icon-theme-name = Adwaita
clock-format = %a %d %b %I:%M %p
indicators = ~host;~spacer;~session;~clock;~power
EOF
$ sudo systemctl restart lightdm
```

## Set a default user in lightdm

```
## In /etc/lightdm/lightdm.conf Go down the file
$ sudo vim /etc/lightdm/lightdm.conf
greeter-hide-users=false
$ sudo systemctl restart lightdm
```
