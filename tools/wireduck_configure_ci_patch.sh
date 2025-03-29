#!/bin/bash
set -euxo pipefail

# Version and target paths
TSHARK_VERSION="2.6.10-1~ubuntu18.04.0"
WIRESHARK_COMMON_VERSION="2.6.10-1~ubuntu18.04.0"
TSHARK_DIR="$PWD/tools/tshark"

mkdir -p "$TSHARK_DIR"
cd "$TSHARK_DIR"

# Download main Wireshark packages (Ubuntu 18.04 - bionic)
wget -N http://security.ubuntu.com/ubuntu/pool/universe/w/wireshark/wireshark-common_${WIRESHARK_COMMON_VERSION}_amd64.deb
wget -N http://security.ubuntu.com/ubuntu/pool/universe/w/wireshark/tshark_${TSHARK_VERSION}_amd64.deb

# Extract to current dir
dpkg -x wireshark-common_${WIRESHARK_COMMON_VERSION}_amd64.deb .
dpkg -x tshark_${TSHARK_VERSION}_amd64.deb .

wget -N http://security.ubuntu.com/ubuntu/pool/universe/w/wireshark/libwireshark11_2.6.10-1~ubuntu18.04.0_amd64.deb
dpkg -x libwireshark11_2.6.10-1~ubuntu18.04.0_amd64.deb .

wget -N http://security.ubuntu.com/ubuntu/pool/universe/w/wireshark/libwiretap8_2.6.10-1~ubuntu18.04.0_amd64.deb
dpkg -x libwiretap8_2.6.10-1~ubuntu18.04.0_amd64.deb .

wget -N http://security.ubuntu.com/ubuntu/pool/universe/w/wireshark/libwsutil9_2.6.10-1~ubuntu18.04.0_amd64.deb
dpkg -x libwsutil9_2.6.10-1~ubuntu18.04.0_amd64.deb .

# Create wrapper to run tshark with correct LD_LIBRARY_PATH
mkdir -p "$TSHARK_DIR/bin"
cat > "$TSHARK_DIR/bin/tshark" <<EOF
#!/bin/bash
DIR=\$(cd "\$(dirname "\$0")/.." && pwd)
LD_LIBRARY_PATH="\$DIR/usr/lib/x86_64-linux-gnu" "\$DIR/usr/bin/tshark" "\$@"
EOF

chmod +x "$TSHARK_DIR/bin/tshark"

# Expose to GitHub Actions (for future steps)
if [ -n "${GITHUB_PATH:-}" ]; then
  echo "$TSHARK_DIR/bin" >> "$GITHUB_PATH"
fi

# Also expose to current shell session (immediate use)
export PATH="$TSHARK_DIR/bin:$PATH"

# Validate
tshark -v || echo "⚠️ TShark wrapper not working yet"
