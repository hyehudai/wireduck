#!/bin/bash
set -euxo pipefail

echo "🧰 Setting up Docker-based tshark wrapper..."

WRAPPER_DIR="$PWD/tools/tshark"
mkdir -p "$WRAPPER_DIR"

cat > "$WRAPPER_DIR/tshark" <<'EOF'
#!/bin/bash
docker run --rm -i -v "$PWD:/data" cincan/tshark "$@"
EOF

chmod +x "$WRAPPER_DIR/tshark"

# Set it up in GitHub Actions PATH if supported
if [ -n "${GITHUB_PATH:-}" ]; then
  echo "$WRAPPER_DIR" >> "$GITHUB_PATH"
else
  export PATH="$WRAPPER_DIR:$PATH"
fi

echo "✅ tshark wrapper installed at $WRAPPER_DIR/tshark"
"$WRAPPER_DIR/tshark" -v || echo "⚠️ Can't run tshark without Docker or PCAP file"
