.PHONY: all test format format-check tidy lint clean monitor

# Without a key file, MCUboot's default dev key signs the image.
SIGNING_KEY ?= $(wildcard $(CURDIR)/private/key.pem)
SIGN_CMAKE_ARGS := $(if $(SIGNING_KEY),-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\"$(abspath $(SIGNING_KEY))\")

ZEPHYR_ENV := . ~/zephyrproject/.venv/bin/activate && cd ~/zephyrproject &&
ZEPHYR_APP := $(CURDIR)/main
ESPTOOL_PORT := $(if $(ESP_DEVICE),--port $(ESP_DEVICE))

TEST_ZEPHYR_APP := $(CURDIR)/main/tests/unit
TEST_ZEPHYR_BUILD := $(CURDIR)/.build/test-unit
TIDY_DB := $(CURDIR)/.build/tidy-db

# Clang's own warnings are dropped, its errors are not. Needs a clang-tidy with the Xtensa target.
TIDY_FILTER = awk '/^([^[:space:]].*: )?(error|warning): .*\[.*\]$$/ { keep = ($$0 !~ /clang-diagnostic/ || $$0 ~ /clang-diagnostic-error/); if (keep) found = 1 } keep { print } END { exit found }'
tidy_run = python3 -c "import json,sys; print('\n'.join(sorted({e['file'] for e in json.load(open(sys.argv[1])) if e['file'].endswith('.cpp')})))" $(1)/compile_commands.json \
	| xargs -P$(shell nproc) -I{} clang-tidy -p $(1) --quiet {} 2>/dev/null | $(TIDY_FILTER)

include web.mk
include $(wildcard boards/*/board.mk)
include displays/nextion.mk
include $(wildcard displays/*/display.mk)
include targets/template.mk
include $(wildcard targets/*/target.mk)

test:
	$(ZEPHYR_ENV) west build -b native_sim -d $(TEST_ZEPHYR_BUILD) $(TEST_ZEPHYR_APP) -t run

CPP_SOURCES = $(shell find $(ZEPHYR_APP) $(CURDIR)/displays -name '*.cpp' -o -name '*.hpp')
MAX_LINE_LENGTH := 240

format:
	clang-format -i $(CPP_SOURCES)

format-check:
	clang-format --dry-run --Werror $(CPP_SOURCES)
	@! grep -nE '.{$(shell expr $(MAX_LINE_LENGTH) + 1)}' $(CPP_SOURCES)

tidy: $(addsuffix -tidy,$(TARGETS))

lint: format-check web-lint tidy

clean:
	rm -rf $(addprefix $(CURDIR)/.build/,$(TARGETS)) $(TEST_ZEPHYR_BUILD)

monitor:
	python ./tools/monitor.py

-include Makefile.local
