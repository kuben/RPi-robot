host="${2:-192.168.0.106}"
hex="${1:-driver.hex}"
echo "$hex"
set -e
make $hex
scp $hex pi@$host: && \
ssh -t pi@$host "/usr/local/bin/p14 program $hex"
