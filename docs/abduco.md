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

## Detach using CTRL-\ and later reattach with
$ abduco -a dvtm-session
```
