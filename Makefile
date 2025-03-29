PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=wireduck
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile

configure_ci:
	@echo "Running wireduck configure step..."
	@echo "🐳 Building custom tshark Docker image..."
	docker build -t wireduck-tshark tools/tshark
	ln -sf tools/tshark/tshark.sh tools/tshark/tshark
	export PATH=$PWD/tools/tshark:$PATH
	