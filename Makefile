BUILD_DIR := build

-include .env

# export CLSPC_JAVA_BIN
# export CLSPC_JDTLS_HOME
# export CLSPC_TIMEOUT_BIN

# test 
T ?= .*

.PHONY: all config build test_all test_all_verbose test_one clean 

all: config build


config:
	cmake -S . -B $(BUILD_DIR) -DLSPX_BUILD_TESTS=ON


build: config
	cmake --build $(BUILD_DIR) -j -- --no-print-directory


clean:
	rm -rf $(BUILD_DIR)


test-all: build
	cd $(BUILD_DIR) && ctest --output-on-failure


test-all-verbose: build 
	cd $(BUILD_DIR) && ctest --output-on-failure -V


test-one: build
	cd $(BUILD_DIR) && ctest -R $(T) --output-on-failure -V


cli-jdtls-class:
	cd $(BUILD_DIR) && \
		LSPX_TRACE_RPC=1 \
		LSPX_TRACE_LSP=1 \
		LSPX_TRACE_GRAPH=1 \
		./lspx/cli/lspx-cli \
        jdtls callgraph \
			--java "$(LSPX_JAVA_BIN)" \
			--jdtls-home "$(LSPX_JDTLS_HOME)" \
			--root "$(LSPX_ROOT)" \
			--workspace "$(LSPX_WORKSPACE)" \
			--class "$(LSPX_CLASS)" \
			--function "$(LSPX_METHOD)" \
			--max-depth 5 \
			--direction both

cli-jdtls-file:
	cd $(BUILD_DIR) && \
		LSPX_TRACE_RPC=1 \
		LSPX_TRACE_GRAPH=1 \
		./lspx/cli//lspx-cli \
        jdtls callgraph \
			--java "$(LSPX_JAVA_BIN)" \
			--jdtls-home "$(LSPX_JDTLS_HOME)" \
			--root "$(LSPX_ROOT)" \
			--workspace "$(LSPX_WORKSPACE)" \
			--file "$(LSPX_FILE)" \
			--function "$(LSPX_METHOD)" \
			--max-depth 5 \
			--direction both
