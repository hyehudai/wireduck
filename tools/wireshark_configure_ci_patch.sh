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

# Optional: Confirm it works
./usr/bin/tshark -v

# Create a wrapper script that sets the needed LD_LIBRARY_PATH and runs tshark
mkdir -p "$TSHARK_DIR/bin"
cat > "$TSHARK_DIR/bin/tshark" <<EOF
#!/bin/bash
DIR=\$(cd "\$(dirname "\$0")/.." && pwd)
LD_LIBRARY_PATH="\$DIR/usr/lib/x86_64-linux-gnu" "\$DIR/usr/bin/tshark" "\$@"
EOF

chmod +x "$TSHARK_DIR/bin/tshark"

# Make sure this path is picked up during build/test
if [ -n "${GITHUB_PATH:-}" ]; then
  echo "$TSHARK_DIR/bin" >> "$GITHUB_PATH"
else
  export PATH="$TSHARK_DIR/bin:$PATH"
fi

# Test the wrapper
tshark -v || echo "⚠️ Wrapper not working yet."
