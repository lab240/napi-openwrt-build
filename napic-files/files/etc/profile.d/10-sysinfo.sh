#!/bin/sh

echo ""
for iface in /sys/class/net/eth*; do
    name=$(basename "$iface")
    ip=$(ip -4 addr show "$name" 2>/dev/null | awk '/inet /{print $2}')
    state=$(cat "$iface/operstate" 2>/dev/null)
    [ -z "$ip" ] && ip="no address"
    echo " $name: $ip ($state)"
done
echo ""
