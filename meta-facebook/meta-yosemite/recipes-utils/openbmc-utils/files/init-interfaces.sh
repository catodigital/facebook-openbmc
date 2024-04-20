#!/bin/sh
#Create symlink for interfaces file in persistent storage
### BEGIN INIT INFO
# Provides:          init-interfaces
# Required-Start:
# Required-Stop:
# Default-Start:     S
# Default-Stop:
# Short-Description: Symlink for interfaces
### END INIT INFO

target=/mnt/data/etc/network
create_interfaces() {
cat > ${target}/interfaces<< EOF
auto lo
iface lo inet loopback

auto eth0
iface eth0 inet manual
  up ip link set eth0 up
  up dhclient -4 -d -pf /var/run/dhclient.eth0.pid eth0 > /dev/null 2>&1 &
EOF

#append_vlan
}

append_vlan() {
cat >> ${target}/interfaces<< EOF
auto vlan42
iface vlan42 inet static
  vlan-raw-device eth0
  address   10.1.2.3
  netmask   255.255.255.0
EOF
}

# Source function library.
. /etc/init.d/functions

start() {
    if [ ! -f ${target}/interfaces ];then
        mkdir -p ${target}
        create_interfaces
    fi
}
stop() {
    #Don't need to do anything on stop
    echo "Stopping init-interfaces.sh"
}
case "$1" in 
    start)
       start
       ;;
    stop)
       stop
       ;;
    restart)
       stop
       start
       ;;
    status)
       if [ -f ${target}/interfaces ];then
            echo "init-interfaces OK"
       else
            echo "init-interfaces didn't run"
       fi
       ;;
    *)
       echo "Usage: $0 {start|stop|status|restart}"
esac

exit 0 
