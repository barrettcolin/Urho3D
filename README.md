* Builds for x86 Windows (Visual Studio 2013), x86 Linux and Raspberry Pi; follow Urho3D build instructions to set up your environment, then run either cmake_vs2013_q2.bat or cmake_gcc_q2.sh (these helper scripts create a minimal Urho3D config which compiles faster; the Urho3D scripts also work however)
* Build the 'Q2App' target
* Copy 'maps.lst', 'pak0.pak', 'pak1.pak', 'pak2.pak' and 'video' directory from 'baseq2' in your Quake 2 installation directory into 'Bin/baseq2' (alternatively, at least under Visual Studio, you can set debug working directory to your Quake 2 installation directory)
* Run 'Bin/Q2App' (command line arguments for Q2App are passed to Urho3D; Quake 2 command line arguments can be changed by editing Bin/Quake2Data/CommandLine.txt)
* 'Q2App -w' will start Urho3D windowed, 'Q2App -v' will start fullscreen with vsync

Raspberry Pi NFS root notes: assuming a Linux PC (notes are using Debian Wheezy) with a wlan connection to your network and a direct eth connection to your Raspberry Pi (no crossover cable required; the Pi autosenses a direct connection)

PC eth
* set up a static address for PC eth (not the same network as your lan address; I used 10.0.0.0 as the network)
* in Gnome Network Connections, edit Wired Connection, IPv4 settings 'Manual', address=10.0.0.1, netmask=255.255.255.0, gateway=0.0.0.0

PC NAT
* sudo echo 1 > /proc/sys/net/ipv4/ip_forward
* sudo iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE

Raspi root
* get an official Raspbian image (I used 2015-01-31)
* unzip 2015-01-31-raspbian.zip
* sudo fdisk -lu 2015-01-31-raspbian.img (start is 122880, so offset is 122880 * 512 = 62914560)
* sudo losetup -o 62914560 /dev/loop0 2015-01-31-raspbian.img
* sudo mount /dev/loop0 /mnt
* sudo cp -rav /mnt/ /home/user/raspi-nfsroot/
* sudo umount /mnt
* sudo losetup -d /dev/loop0
* sudo nano /home/user/raspi-nfsroot/etc/fstab and comment out line that mounts '/' from SD card

PC NFS
* set up NFS (instructions for Debian: https://wiki.debian.org/NFSServerSetup)
* add Raspi root export (to /etc/exports): `/home/user/raspi-nfsroot 10.0.0.10(rw,sync,no_root_squash,no_subtree_check)`
* refresh NFS with: sudo service nfs-kernel-server restart

Raspi SD card
* write the Raspbian image to SD card (instructions as per Raspberry Pi downloads page)
* edit '/boot/cmdline.txt' on the SD card: `dwc_otg.lpm_enable=0 console=ttyAMA0,115200 console=tty1 ip=10.0.0.10::10.0.0.1:255.255.255.0:raspi:eth0:off root=/dev/nfs rootfstype=nfs nfsroot=10.0.0.1:/home/user/raspi-nfsroot,udp,vers=3 smsc95xx.turbo_mode=N elevator=deadline rootwait`
* boot Raspberry Pi from SD card

Raspi network config: edit /etc/network/interfaces on Pi

`#iface eth0 inet dhcp
iface eth0 inet static
address 10.0.0.10
netmask 255.255.255.0
network 10.0.0.0
broadcast 10.0.0.255
gateway 10.0.0.1`

Reboot Pi and you should be able to ping network/internet from the Pi
