dvtm
====

## URLs

```
## https://github.com/martanne/dvtm
```

## Clone

```
## Init Commit 7bcf43f
$ git clone https://github.com/martanne/dvtm
```

## Patch

```
## https://waxandwane.org/dvtm.html
$ wget https://waxandwane.org/dvtm/dvtm-v0.15-52-g7bcf43f-pertag.diff
$ patch -p1 < dvtm-v0.15-52-g7bcf43f-pertag.diff
```

## Build static

```
$ make LDFLAGS=-static
```
