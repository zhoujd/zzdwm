abduco
======

## URLs

```
https://github.com/martanne/abduco
```

## Build abduco having detach feature of screen -static

```
$ git clone https://github.com/martanne/abduco
$ cd abduco
$ make LDFLAGS=-static
```

## Usage guide

```
## Create a new session
$ abduco -c demo
$ abduco -c session-name your-application
$ abduco

## Attach sesion
$ abduco -a demo
$ abduco -a session-name
```

## Detach / reattach functionality

```
$ abduco -c dvtm-session

## Detach using CTRL-\ (default) and later reattach with
## -e <char>  Set the detach character to <char>, defaults to ^\.
$ abduco -a dvtm-session
```

## PID

```
## https://github.com/martanne/abduco/commit/884e3bb2ca3cbdb0e23799c10cce7d55139d1f1c
```
