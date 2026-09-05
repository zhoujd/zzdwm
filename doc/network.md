Network
=======

## Linux VM VPN network route issue via NetworkManager

```
# 1. Set the NAT interface metric to a highly prioritized 100
sudo nmcli connection modify "Wired connection 1" ipv4.route-metric 100

# 2. Set the Host-Only interface metric to a lower priority 500
sudo nmcli connection modify "Wired connection 2" ipv4.route-metric 500

# 3. Reload the connections to instantly update the kernel routing table
sudo nmcli connection up "Wired connection 1"
sudo nmcli connection up "Wired connection 2"
```

## Linux VM VPN network route issue via systemd-networkd (default Debain)

```
## 1. Locate your Network Configurations
$ ip route
default via 10.0.2.2 dev enp0s3
default via 10.0.2.2 dev enp0s3 proto dhcp src 10.0.2.15 metric 1024
10.0.2.0/24 dev enp0s3 proto kernel scope link src 10.0.2.15
10.0.2.2 dev enp0s3 proto dhcp scope link src 10.0.2.15 metric 1024
10.0.2.3 dev enp0s3 proto dhcp scope link src 10.0.2.15 metric 1024
192.168.56.0/24 dev enp0s8 proto kernel scope link src 192.168.56.102 metric 1024

$ networkctl
IDX LINK        TYPE     OPERATIONAL SETUP
  1 lo          loopback carrier     unmanaged
  2 enp0s3      ether    routable    configured
  3 enp0s8      ether    routable    configured

$ ls -l /etc/systemd/network/
total 12
-rw-r--r-- 1 root root 40 Mar 25  2025 05-enp0s3.network
-rw-r--r-- 1 root root 41 Mar 25  2025 05-enp0s8.network

## 2. Configure the NAT Adapter (enp0s3) Priority
$ sudo nano /etc/systemd/network/10-enp0s3.network
[Match]
Name=enp0s3

[Network]
DHCP=ipv4

[DHCPv4]
RouteMetric=100

## 3. Configure the Host-Only Adapter (enp0s8) Priority
$ sudo nano /etc/systemd/network/20-enp0s8.network
[Match]
Name=enp0s8

[Network]
DHCP=ipv4

[DHCPv4]
RouteMetric=500

## 4. Restart Networkd to Apply Changes
$ sudo systemctl restart systemd-networkd
```
