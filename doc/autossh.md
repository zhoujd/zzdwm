# AutoSSH

## Reverse SSH Tunnel with systemd

Create an SSH config entry for the tunnel server:

```sshconfig
Host s160
  HostName 10.11.60.160
  User zachary.zhou
```

Create the service definition:

```bash
$ sudo nano /etc/systemd/system/autossh-tunnel.service
```

```ini
[Unit]
Description=AutoSSH Remote Tunnel Service
After=network.target

[Service]
Type=simple
User=zach
Environment="AUTOSSH_GATETIME=0"
ExecStart=/usr/local/bin/autossh -M 0 -N -R 42222:localhost:22 s160 -i /home/zach/.ssh/id_rsa
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Reload systemd, enable the service at boot, and start it:

```bash
$ sudo systemctl daemon-reload
$ sudo systemctl enable --now autossh-tunnel.service
```

Check the service and tunnel:

```bash
$ systemctl status autossh-tunnel.service
$ journalctl -u autossh-tunnel.service -f
```

## Remote SSH Server Configuration

On `s160`, externally reachable reverse tunnels require `GatewayPorts yes` in
`/etc/ssh/sshd_config`:

```sshconfig
GatewayPorts yes
```

Validate the SSH daemon configuration and restart it:

```bash
$ sudo sshd -t
$ sudo systemctl restart ssh
```

The tunnel listens on `s160` port `42222` and forwards connections to SSH on
the local machine:

```bash
$ ssh -p 42222 <user>@10.11.60.160
```
