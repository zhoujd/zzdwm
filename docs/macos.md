MacOS
=====

## Docker images

```
## https://github.com/sickcodes/docker-osx
$ docker pull sickcodes/docker-osx:latest
$ docker run -it \
    --device /dev/kvm \
    -p 50922:10022 \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -e "DISPLAY=${DISPLAY:-:0.0}" \
    -e SHORTNAME=high-sierra \
    sickcodes/docker-osx:latest
```
