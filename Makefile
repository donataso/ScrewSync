.PHONY: all firmware

KEYS_DIR := $(CURDIR)/assets
SIGN_CMAKE_ARGS := -DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\"$(KEYS_DIR)/mcuboot_signing_key.pem\"

MAIN_ZEPHYR_APP := $(CURDIR)/main
MAIN_ZEPHYR_BUILD := $(CURDIR)/.build/firmware
MAIN_ZEPHYR_BOARD := esp32s3_devkitc/esp32s3/procpu
MAIN_ZEPHYR_ENV := . ~/zephyrproject/.venv/bin/activate && cd ~/zephyrproject &&

TEST_ZEPHYR_APP := $(CURDIR)/main/tests/unit
TEST_ZEPHYR_BUILD := $(CURDIR)/.build/test-unit

SCREEN_DIR := $(CURDIR)/display
SCREEN_HMI := $(SCREEN_DIR)/display.HMI
SCREEN_TFT := $(SCREEN_DIR)/display.tft
SCREEN_180_SRC := $(CURDIR)/.build/display_180
SCREEN_180_HMI := $(CURDIR)/.build/display_180.HMI
SCREEN_180_TFT := $(SCREEN_DIR)/display_180.tft
NXWRIGHT := $(HOME)/apps/nxwright/.venv/bin/nxwright
OTA_HOST ?= 192.168.4.187
# Must match display_partition in main/app.overlay
DISPLAY_PARTITION := 0x400000
DISPLAY_IMAGE := $(MAIN_ZEPHYR_BUILD)/display.bin
# Must match slot0_partition of the board's flash layout
APP_PARTITION := 0x20000
RELEASE_DIR := $(CURDIR)/firmware
ESPTOOL_PORT := $(if $(ESP_DEVICE),--port $(ESP_DEVICE))

web-build:
	python3 scripts/build_webui.py

web-test:
	python3 scripts/test_web.py

build: web-build
	$(MAIN_ZEPHYR_ENV) west build -b $(MAIN_ZEPHYR_BOARD) -d $(MAIN_ZEPHYR_BUILD) $(MAIN_ZEPHYR_APP) -p auto --sysbuild -- -DSB_CONFIG_BOOTLOADER_MCUBOOT=y $(SIGN_CMAKE_ARGS)

build-debug-mcuboot: web-build
	$(MAIN_ZEPHYR_ENV) west build -b $(MAIN_ZEPHYR_BOARD) -d $(MAIN_ZEPHYR_BUILD) $(MAIN_ZEPHYR_APP) -p auto --sysbuild -- -DSB_CONFIG_BOOTLOADER_MCUBOOT=y $(SIGN_CMAKE_ARGS) -Dmcuboot_CONFIG_MCUBOOT_LOG_LEVEL_OFF=n -Dmcuboot_CONFIG_MCUBOOT_LOG_LEVEL_DBG=y

ota-erase-region:
	$(MAIN_ZEPHYR_ENV) esptool.py erase-region 0x170000 0x150000

ota-show-region:
	$(MAIN_ZEPHYR_ENV) esptool.py read_flash 0x170000 0x2000 /tmp/slot1_dump.bin && xxd /tmp/slot1_dump.bin

settings-erase-region:
	$(MAIN_ZEPHYR_ENV) esptool.py erase-region 0x3b0000 0x30000

display-image:
	python3 scripts/pack_display_image.py $(SCREEN_TFT) $(DISPLAY_IMAGE)

# Writes the display image only when it differs from what's in flash.
flash: display-image
	$(MAIN_ZEPHYR_ENV) (esptool.py $(ESPTOOL_PORT) verify-flash $(DISPLAY_PARTITION) $(DISPLAY_IMAGE) || esptool.py $(ESPTOOL_PORT) write-flash $(DISPLAY_PARTITION) $(DISPLAY_IMAGE))
	$(MAIN_ZEPHYR_ENV) west flash -d $(MAIN_ZEPHYR_BUILD) $(if $(ESP_DEVICE),--esp-device $(ESP_DEVICE))

# full.bin flashes a blank device at 0x0; app.bin is the OTA image.
firmware: build display-image
	mkdir -p $(RELEASE_DIR)
	cp $(MAIN_ZEPHYR_BUILD)/main/zephyr/zephyr.signed.bin $(RELEASE_DIR)/firmware.bin
	$(MAIN_ZEPHYR_ENV) esptool.py --chip esp32s3 merge-bin --output $(RELEASE_DIR)/firmware.full.bin \
		0x0 $(MAIN_ZEPHYR_BUILD)/mcuboot/zephyr/zephyr.bin \
		$(APP_PARTITION) $(MAIN_ZEPHYR_BUILD)/main/zephyr/zephyr.signed.bin \
		$(DISPLAY_PARTITION) $(DISPLAY_IMAGE)

ota:
	curl -sf --data-binary "@$(MAIN_ZEPHYR_BUILD)/main/zephyr/zephyr.signed.bin" "http://$(OTA_HOST)/update"

display-build:
	$(NXWRIGHT) check $(SCREEN_DIR)
	$(NXWRIGHT) build $(SCREEN_DIR) $(SCREEN_HMI) --overwrite --no-backup
	$(NXWRIGHT) compile $(SCREEN_HMI) $(SCREEN_TFT) --overwrite --no-backup
	rm -rf $(SCREEN_180_SRC)
	mkdir -p $(SCREEN_180_SRC)
	cp -r $(SCREEN_DIR)/app.toml $(SCREEN_DIR)/defaults.toml $(SCREEN_DIR)/program.nx $(SCREEN_DIR)/pages $(SCREEN_DIR)/fonts $(SCREEN_180_SRC)
	sed -i 's/^rotation = .*/rotation = 180/' $(SCREEN_180_SRC)/app.toml
	$(NXWRIGHT) build $(SCREEN_180_SRC) $(SCREEN_180_HMI) --overwrite --no-backup
	$(NXWRIGHT) compile $(SCREEN_180_HMI) $(SCREEN_180_TFT) --overwrite --no-backup

display-ota:
	curl -if --data-binary "@$(SCREEN_TFT)" "http://$(OTA_HOST)/update-display"

test:
	$(MAIN_ZEPHYR_ENV) west build -b native_sim -d $(TEST_ZEPHYR_BUILD) $(TEST_ZEPHYR_APP) -t run

CPP_SOURCES = $(shell find $(MAIN_ZEPHYR_APP) -name '*.cpp' -o -name '*.hpp')
TIDY_DB := $(CURDIR)/.build/tidy-db

format:
	clang-format -i $(CPP_SOURCES)

MAX_LINE_LENGTH := 240

format-check:
	clang-format --dry-run --Werror $(CPP_SOURCES)
	@! grep -nE '.{$(shell expr $(MAX_LINE_LENGTH) + 1)}' $(CPP_SOURCES)

# Needs the compile databases of `make build` and `make test`, and a clang-tidy
# with the Xtensa target. Clang's own warnings are dropped, its errors are not.
tidy:
	python3 scripts/gen_tidy_db.py $(MAIN_ZEPHYR_BUILD)/main $(TEST_ZEPHYR_BUILD) $(TIDY_DB)/compile_commands.json
	find $(MAIN_ZEPHYR_APP) -name '*.cpp' | xargs -P$(shell nproc) -I{} clang-tidy -p $(TIDY_DB) --quiet {} 2>/dev/null \
	| awk '/^([^[:space:]].*: )?(error|warning): .*\[.*\]$$/ { keep = ($$0 !~ /clang-diagnostic/ || $$0 ~ /clang-diagnostic-error/); if (keep) found = 1 } keep { print } END { exit found }'

web-lint:
	python3 scripts/lint_web.py

web-lint-fix:
	python3 scripts/lint_web.py --fix

lint: format-check web-lint tidy

clean:
	rm -rf $(MAIN_ZEPHYR_BUILD) $(TEST_ZEPHYR_BUILD)

monitor:
	python ./tools/monitor.py

# Uploads the .tft over the USB console, the firmware stores then flashes it.
display-serial:
	python ./tools/monitor.py --tft $(SCREEN_TFT)

screenshots:
	. $(HOME)/apps/nxwright/.venv/bin/activate && python3 scripts/screenshots.py

bf: build flash
db: display-build
do: display-ota
