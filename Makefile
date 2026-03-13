BUILD_DIR := build

# test 
T ?= .*

.PHONY: all config config-integ build integ test_all test_all_verbose test_one clean

all: config build

integ: config-integ build

config:
	cmake -S . -B $(BUILD_DIR) -DCLSPC_BUILD_TESTS=ON -DCLSPC_BUILD_INTEGRATION_TESTS=OFF

config-integ:
	cmake -S . -B $(BUILD_DIR) -DCLSPC_BUILD_TESTS=ON -DCLSPC_BUILD_INTEGRATION_TESTS=ON

build: config
	cmake --build $(BUILD_DIR) -j -- --no-print-directory

build-integ: config-integ
	cmake --build $(BUILD_DIR) -j -- --no-print-directory


test_all: 
	cd $(BUILD_DIR) && ctest --output-on-failure

test_all_verbose: 
	cd $(BUILD_DIR) && ctest --output-on-failure -V

test_one: 
	cd $(BUILD_DIR) && ctest -R $(T) --output-on-failure -V

clean:
	rm -rf $(BUILD_DIR)




