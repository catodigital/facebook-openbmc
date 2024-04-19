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

networkdir=/etc/network
interfaces=$networkdir/interfaces
persistent=/mnt/data

create_interfaces() {
  cat > $persistent$interfaces<< EOF
auto lo
iface lo inet loopback

auto eth0
iface eth0 inet manual
  pre-up modprobe 8021q
  up ip link set eth0 up
  up dhclient -4 -d -pf /var/run/dhclient.eth0.pid eth0 > /dev/null 2>&1 &
EOF
}

start() {
   if [ ! -L $interfaces ];then
        if [ ! -f $persistent$interfaces ];then
            mkdir -p $networkdir
            create_interfaces
        fi
        ln -sf $persistent$interfaces $interfaces
   fi
}

################################################################################
# Source function library.
. /etc/init.d/functions

set -euo pipefail
#set +x

case "$1" in
    start)
    echo "start"
       start
       ;;
    stop)
       ;;
    restart)
       start
       ;;
    status)
       if [ -L /etc/network/interfaces ];then
            echo "init-interfaces OK"
       else
            echo "init-interfaces didn't run"
       fi
       ;;
    *)
       echo "Usage: $0 {start|stop|status|restart}"
esac

exit 0
