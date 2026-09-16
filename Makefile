ESPHOME_VERSION ?= 2026.8.0
ESPHOME_IMAGE ?= ghcr.io/esphome/esphome:$(ESPHOME_VERSION)
DOCKER ?= docker
CXX ?= g++
BUILD_DIR ?= /tmp/esphome-tuya-wifi-mcu-tests
FIXTURES := arduino-mcu arduino-module esp-idf-mcu esp-idf-module legacy-main

DOCKER_RUN = $(DOCKER) run --rm \
	--user $$(id -u):$$(id -g) \
	-e HOME=/tmp \
	-e XDG_CACHE_HOME=/config/.cache \
	-e PLATFORMIO_CORE_DIR=/config/.cache/platformio \
	-v "$$(pwd):/config" \
	-w /config
ESPHOME ?= $(DOCKER_RUN) --entrypoint esphome $(ESPHOME_IMAGE)
PYTHON ?= $(DOCKER_RUN) --entrypoint python $(ESPHOME_IMAGE)

.PHONY: help host-test e2e-test protocol-test helper-test config-test config compile config-all compile-all test ci clean \
	$(addprefix config-,$(FIXTURES)) $(addprefix compile-,$(FIXTURES))

help:
	@printf '%s\n' \
		'make host-test             Build and run framework-independent C++ tests' \
		'make e2e-test              Compile and run ESPHome host UART/API e2e tests' \
		'make config-test           Run Python configuration compatibility tests' \
		'make config-all            Validate all ESPHome fixtures in Docker' \
		'make compile-all           Compile all ESPHome fixtures in Docker' \
		'make config FIXTURE=name   Validate one fixture' \
		'make compile FIXTURE=name  Compile one fixture' \
		'make ci                    Run the complete verification matrix'

$(BUILD_DIR):
	mkdir -p $@

protocol-test: | $(BUILD_DIR)
	$(CXX) -std=c++17 -Wall -Wextra -Werror \
		tests/protocol/test_tuya_protocol.cpp \
		components/tuya_wifi_mcu/tuya_protocol.cpp \
		-o $(BUILD_DIR)/test_tuya_protocol
	$(BUILD_DIR)/test_tuya_protocol

helper-test: | $(BUILD_DIR)
	$(CXX) -std=c++17 -Wall -Wextra -Werror \
		tests/protocol/test_tuya_helpers.cpp \
		-o $(BUILD_DIR)/test_tuya_helpers
	$(BUILD_DIR)/test_tuya_helpers

host-test: protocol-test helper-test

e2e-test:
	$(PYTHON) tests/e2e/test_host.py

config-test:
	$(PYTHON) -m unittest discover -s tests/config -p 'test_*.py' -v

config:
	@test -n "$(FIXTURE)" || (printf '%s\n' 'FIXTURE is required' >&2; exit 2)
	$(MAKE) config-$(FIXTURE)

compile:
	@test -n "$(FIXTURE)" || (printf '%s\n' 'FIXTURE is required' >&2; exit 2)
	$(MAKE) compile-$(FIXTURE)

$(addprefix config-,$(FIXTURES)): config-%:
	$(ESPHOME) config tests/compile/$*.yaml

$(addprefix compile-,$(FIXTURES)): compile-%:
	$(ESPHOME) compile tests/compile/$*.yaml

config-all: $(addprefix config-,$(FIXTURES))

compile-all: $(addprefix compile-,$(FIXTURES))

test: host-test e2e-test config-test config-all

ci: host-test e2e-test config-test config-all compile-all

clean:
	rm -rf .cache .esphome tests/compile/.esphome tests/e2e/.esphome
