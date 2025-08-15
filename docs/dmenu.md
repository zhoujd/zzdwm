dmenu
=====

## URLs

```
## https://tools.suckless.org/dmenu/
## https://dl.suckless.org/tools/dmenu-5.3.tar.gz
## https://tools.suckless.org/dmenu/patches/center/
```

## Scripts

```
## https://tools.suckless.org/dmenu/scripts/
## https://github.com/jukil/dmenu-scripts-collection
## https://github.com/aario/dmenu
```

## Options

```bash
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
```

## keysym for Shift + Tab

```
## https://www.cl.cam.ac.uk/~mgk25/ucs/keysymdef.h
## XK_ISO_Left_Tab
```

## Enter sudo password

```bash
## apply dmenu-password-5.0.diff
$ wget https://tools.suckless.org/dmenu/patches/password/dmenu-password-5.0.diff
$ patch -p1 < dmenu-password-5.0.diff

## enter password via dmenu
password=$(echo -n | dmenu -P -p "Enter password:")
if [ -n "$password" ]; then
    echo $password | sudo -S reboot
fi
```

## Run desktop

```bash
## sed
$ exec $(grep '^Exec' $full | tail -1 | \
               sed 's/^Exec=//' | sed 's/%.//' | \
               sed 's/^"//g' | sed 's/" *$//g')
## awk
$ exec $(awk -F= '/^Exec/ {print $2; exit}' $full | awk '{print $1}')
```

## Indexing selection

```bash
## Options 1
options=( "1 Foo" "2 Bar" "3 Baz" )
selection=$(printf '%s\n' "${options[@]}" | dmenu)
index=${selection%% *}
printf 'item #%d selected\n' "$index"

## Options 2
options=( "Foo" "Bar" "Baz" )
selection=$(printf '%s\n' "${options[@]}" | dmenu)
index=1
for item in "${options[@]}"; do
    if [[ $item == $selection ]]; then
        break
    fi
    ((index++))
done
printf 'item #%d selected\n' "$index"
```

## Print index

```
## https://tools.suckless.org/dmenu/patches/printindex/dmenu-printindex-5.0.diff
## Pass the -ix flag to dmenu to enable index printing.
```
## Default theme

```
static const char *colors[SchemeLast][2] = {
    /*     fg         bg       */
    [SchemeNorm] = { "#bbbbbb", "#222222" },
    [SchemeSel] = { "#eeeeee", "#005577" },
    [SchemeSelHighlight] = { "#ffc978", "#005577" },
    [SchemeNormHighlight] = { "#ffc978", "#222222" },
    [SchemeOut] = { "#000000", "#00ffff" },
    [SchemeOutHighlight] = { "#ffc978", "#00ffff" },
};
```

## Read input

```bash
$ dmenu < /dev/null
```

```c
errno = 0; // popen(3p) says on failure it "may" set errno
if(!(f = popen("dmenu < /dev/null", "r"))) {
    fprintf(stderr, "dwm: popen 'dmenu < /dev/null' failed%s%s\n",
            errno ? ": " : "", errno ? strerror(errno) : "");
    return;
}
if (!(p = fgets(name, MAX_TAGLEN, f)) && (i = errno) && ferror(f))
    fprintf(stderr, "dwm: fgets failed: %s\n", strerror(i));
if (pclose(f) < 0)
    fprintf(stderr, "dwm: pclose failed: %s\n", strerror(errno));
```
