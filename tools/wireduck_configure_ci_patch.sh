#!/bin/bash
set -eux

# Define version and paths
TSHARK_DEB="tshark_4.2.6-1_amd64.deb"
TSHARK_URL="http://archive.ubuntu.com/ubuntu/pool/universe/w/wireshark/$TSHARK_DEB"
TSHARK_DIR="$PWD/tools/tshark"

# Create target dir
mkdir -p "$TSHARK_DIR"
cd "$TSHARK_DIR"

# Download and extract
wget -N "$TSHARK_URL"
dpkg -x "$TSHARK_DEB" .

# Create a bin directory with symlink
mkdir -p "$TSHARK_DIR/bin"
ln -sf "$TSHARK_DIR/usr/bin/tshark" "$TSHARK_DIR/bin/tshark"

# Add alias to your shell temporarily (for current session)
echo "alias tshark='$TSHARK_DIR/bin/tshark'"
alias tshark="$TSHARK_DIR/bin/tshark"

# Show version to confirm
tshark -v
