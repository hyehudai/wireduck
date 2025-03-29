PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=wireduck
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile
configure_ci:
	@echo "Setting up Docker-based tshark wrapper"
	@bash tools/setup_tshark_docker_wrapper.sh
	
	