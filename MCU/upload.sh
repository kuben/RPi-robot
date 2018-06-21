hex="${1:-driver.hex}"
echo "$hex"
set -e
make $hex
scp $hex alarm@192.168.0.120: && \
ssh -t alarm@192.168.0.120 "/usr/local/bin/p14 program $hex"
