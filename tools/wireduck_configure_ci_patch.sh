#!/bin/bash
set -euxo pipefail

if ! command -v tshark > /dev/null; then
    echo "Installing tshark inside Docker build..."
    apt-get update -y && apt-get install -y tshark
else
    echo "tshark is already installed"
fi
