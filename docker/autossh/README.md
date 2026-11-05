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

## Troubleshooting

### Port `42222` Already In Use On `s160`

If another tunnel or stale SSH session already owns port `42222`, every
connection attempt logs:

```text
Warning: remote port forwarding failed for listen port 42222
```

The container keeps reporting `Up` because `AUTOSSH_GATETIME=0` makes autossh
retry forever, but the tunnel stays down while the warnings repeat. Check with:

```sh
docker logs autossh-tunnel
```

The previous systemd service is the most likely holder, since it forwards the
same port. On this Docker host:

```sh
systemctl status autossh-tunnel.service
sudo systemctl disable --now autossh-tunnel.service
```

Otherwise the holder is a stale `sshd` session on `s160` that was never
cleaned up. Find its PID there and kill it:

```sh
sudo ss -tlnp | grep 42222
sudo kill <pid>
```

Then restart this Compose service and confirm the warning stops:

```sh
docker compose restart autossh-tunnel
docker logs -f autossh-tunnel
```

On `s160`, the forward is back when `sshd` listens on `42222` again:

```sh
ss -tln | grep 42222
```

