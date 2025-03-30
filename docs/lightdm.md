lightdm
=======

## Install lightdm

```
$ sudo apt install lightdm
$ sudo systemctl enable lightdm
$ sudo systemctl restart lightdm
```

## Setup lightdm

```
$ sudo tee /etc/lightdm/lightdm-gtk-greeter.conf <<EOF
[greeter]
theme-name = Adwaita-dark
icon-theme-name = Adwaita
clock-format = %a %d %b %I:%M %p
indicators = ~host;~spacer;~session;~clock;~power
EOF
```
