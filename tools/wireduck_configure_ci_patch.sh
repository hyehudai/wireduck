#!/bin/bash
set -eux

# Define version and paths
docker pull wireshark/tshark
# Add alias to your shell temporarily (for current session)
#echo "alias tshark='$TSHARK_DIR/bin/tshark'"
alias tsharp="$TSHARK_DIR/usr/bin/tshark"

# Show version to confirm
tsharp -v
