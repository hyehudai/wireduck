#!/bin/bash

# This script wraps a local Docker container that runs tshark
set -e

IMAGE_TAG="wireduck-tshark"

# Build the image if it doesn't exist
if ! docker image inspect "$IMAGE_TAG" > /dev/null 2>&1; then
    echo "🔧 Building Docker image '$IMAGE_TAG' with tshark..."
    docker build -t "$IMAGE_TAG" "$(dirname "$0")"
fi

# Run tshark with mounted volume and passed arguments
docker run --rm -i \
    -v "$PWD:/data" \
    "$IMAGE_TAG" "$@"
