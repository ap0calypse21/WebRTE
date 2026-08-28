#!/bin/bash
# Sends the payload to GoldHEN's receiver.
#
#   ./send.sh 192.168.1.50
#   ./send.sh 192.168.1.50 path/to/webrte.bin
#
# The old version of this script sent ps4debug.bin to port 9020, which is
# ps4debug's own receiver -- this repo builds webrte.bin, and GoldHEN listens
# on 9090. send.py does the same thing plus a check that the payload came up,
# and needs nothing but Python 3.

IP="${1:?usage: ./send.sh <console-ip> [payload]}"
BIN="${2:-webrte.bin}"
PORT=9090

if [ ! -f "$BIN" ]; then
    echo "no such file: $BIN"
    exit 2
fi

echo "sending $BIN ($(wc -c < "$BIN") bytes) to $IP:$PORT"

if command -v socat >/dev/null 2>&1; then
    socat -u "FILE:$BIN" "TCP:$IP:$PORT"
elif command -v nc >/dev/null 2>&1; then
    nc "$IP" "$PORT" < "$BIN"
else
    echo "neither socat nor nc found - use: python send.py $IP"
    exit 2
fi

echo "sent. WebRTE should now be listening on 771:"
echo "  curl http://$IP:771/list"
