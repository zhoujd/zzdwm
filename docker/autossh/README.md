# AutoSSH Reverse Tunnel

This Compose project maintains the same reverse tunnel as the original systemd
service:

```sh
autossh -M 0 -N -R 42222:localhost:22 s160
```

The container uses host networking, so `localhost` refers to this Docker host.
After the tunnel starts, SSH connections to port `42222` on `s160` reach SSH on
this host.

## Requirements

- Docker Engine with Compose v2.
- A private key for `zachary.zhou@s160` in `$HOME/.ssh/id_rsa`.
- An SSH config entry matching:

```sshconfig
Host s160
  HostName 10.11.60.160
  User zachary.zhou
```

- On `s160`, externally usable reverse tunnels require:

```sshconfig
GatewayPorts yes
```

The setting belongs in `/etc/ssh/sshd_config`. After changing it, validate and
restart SSH on `s160`:

```sh
sudo sshd -t
sudo systemctl restart ssh
```

If another tunnel or stale SSH process already owns port `42222`, the new
container logs `remote port forwarding failed for listen port 42222`. Stop the
old tunnel first, then restart this Compose service.

## Start And Stop

```sh
docker compose up -d --build
docker compose ps
docker logs -f autossh-tunnel
docker compose down
```

The service uses `restart: always` and starts automatically when Docker starts.

## Connect Through The Tunnel

From a machine that can reach `s160`:

```sh
ssh -p 42222 <user>@10.11.60.160
```

The destination `<user>` is an account on this Docker host, not necessarily the
`zachary.zhou` account used to maintain the tunnel.
