host="${1:-alarmpi}"
hex="${1:-driver.hex}"
echo "$hex"
set -e
make $hex
scp $hex alarm@$host: && \
ssh -t alarm@$host "/usr/local/bin/p14 program $hex"
