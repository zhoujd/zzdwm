# syntax=docker/dockerfile:1

ARG VARIANT=3.20
FROM alpine:$VARIANT

RUN apk add --no-cache openssh-client

RUN addgroup -g 1000 tunnel \
    && adduser -D -h /home/tunnel -s /bin/sh -u 1000 -G tunnel tunnel

COPY bin/autossh /usr/local/bin/autossh
COPY autossh-entrypoint.sh /usr/local/bin/autossh-entrypoint

ENTRYPOINT ["/usr/local/bin/autossh-entrypoint"]
