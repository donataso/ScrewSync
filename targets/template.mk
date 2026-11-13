# $(eval $(call zephyr_target,<t>)) after <t>_BOARD and <t>_DISPLAY are set.
# Hooks, set before the call: <t>_EXTRA_CMAKE_ARGS, <t>_FLASH_DEPS,
# <t>_IMAGES (addr:file pairs flashed and merged with the app).
# <t>_OWN_<RULE> := 1 skips a rule so target.mk can write its own.

TARGETS :=

define zephyr_target
TARGETS += $(1)
$(1)_DIR := $(CURDIR)/targets/$(1)
$(1)_BOARD_DIR := $(CURDIR)/boards/$$($(1)_BOARD)
$(1)_DISPLAY_DIR := $(CURDIR)/displays/$$($(1)_DISPLAY)
$(1)_BUILD := $(CURDIR)/.build/$(1)
$(1)_RELEASE := $(CURDIR)/.build/release/$(1)
$(1)_CMAKE_ARGS := "-Dmain_EXTRA_DTC_OVERLAY_FILE=$$($(1)_BOARD_DIR)/board.overlay;$$($(1)_DIR)/target.overlay" \
	"-Dmain_EXTRA_CONF_FILE=$$($(1)_BOARD_DIR)/board.conf;$$($(1)_DISPLAY_DIR)/display.conf;$$($(1)_DIR)/target.conf" \
	-Dmain_SCREWSYNC_DISPLAY=$$($(1)_DISPLAY) -Dmain_SCREWSYNC_TARGET=$(1)
$(1)_BACKUP_ROOT := $(CURDIR)/.build/backup/$(1)
$(1)_BACKUP_REGIONS += $$($$($(1)_BOARD)_SETTINGS):$$($$($(1)_BOARD)_SETTINGS_SIZE)
$(1)_BACKUP_REGIONS += $$($$($(1)_BOARD)_GCODE):$$($$($(1)_BOARD)_GCODE_SIZE)
$(1)_WEST_BUILD = $$(ZEPHYR_ENV) west build -b $$($$($(1)_BOARD)_ZEPHYR_BOARD) -d $$($(1)_BUILD) $$(ZEPHYR_APP) -p auto --sysbuild -- \
	-DSB_CONFIG_BOOTLOADER_MCUBOOT=y $$(SIGN_CMAKE_ARGS) $$($(1)_CMAKE_ARGS) $$($(1)_EXTRA_CMAKE_ARGS)

.PHONY: $(1)-build $(1)-build-debug-mcuboot $(1)-flash $(1)-firmware $(1)-tidy \
	$(1)-ota-erase-region $(1)-ota-show-region $(1)-settings-erase-region \
	$(1)-backup $(1)-restore $(1)-smoke $(1)-smoke-interactive

ifndef $(1)_OWN_BUILD
$(1)-build: web-build
	$$($(1)_WEST_BUILD)
endif

ifndef $(1)_OWN_BUILD_DEBUG_MCUBOOT
$(1)-build-debug-mcuboot: web-build
	$$($(1)_WEST_BUILD) -Dmcuboot_CONFIG_MCUBOOT_LOG_LEVEL_OFF=n -Dmcuboot_CONFIG_MCUBOOT_LOG_LEVEL_DBG=y
endif

# Extra images are written only when they differ from what's in flash.
ifndef $(1)_OWN_FLASH
$(1)-flash: $$($(1)_FLASH_DEPS)
	$$(foreach i,$$($(1)_IMAGES),$$(ZEPHYR_ENV) (esptool.py $$(ESPTOOL_PORT) verify-flash $$(subst :, ,$$(i)) || esptool.py $$(ESPTOOL_PORT) write-flash $$(subst :, ,$$(i))) &&) true
	$$(ZEPHYR_ENV) west flash -d $$($(1)_BUILD) $$(if $$(ESP_DEVICE),--esp-device $$(ESP_DEVICE))
endif

# full.bin flashes a blank device at 0x0; firmware.bin is the OTA image.
ifndef $(1)_OWN_FIRMWARE
$(1)-firmware: $(1)-build $$($(1)_FLASH_DEPS)
	mkdir -p $$($(1)_RELEASE)
	cp $$($(1)_BUILD)/main/zephyr/zephyr.signed.bin $$($(1)_RELEASE)/firmware.bin
	$$(ZEPHYR_ENV) esptool.py --chip esp32s3 merge-bin --output $$($(1)_RELEASE)/firmware.full.bin \
		0x0 $$($(1)_BUILD)/mcuboot/zephyr/zephyr.bin \
		$$($$($(1)_BOARD)_APP_PARTITION) $$($(1)_BUILD)/main/zephyr/zephyr.signed.bin \
		$$(subst :, ,$$($(1)_IMAGES))
endif

ifndef $(1)_OWN_OTA_ERASE_REGION
$(1)-ota-erase-region:
	$$(ZEPHYR_ENV) esptool.py $$(ESPTOOL_PORT) erase-region $$($$($(1)_BOARD)_OTA_SLOT) $$($$($(1)_BOARD)_OTA_SLOT_SIZE)
endif

ifndef $(1)_OWN_OTA_SHOW_REGION
$(1)-ota-show-region:
	$$(ZEPHYR_ENV) esptool.py $$(ESPTOOL_PORT) read_flash $$($$($(1)_BOARD)_OTA_SLOT) 0x2000 /tmp/slot1_dump.bin && xxd /tmp/slot1_dump.bin
endif

ifndef $(1)_OWN_SETTINGS_ERASE_REGION
$(1)-settings-erase-region:
	$$(ZEPHYR_ENV) esptool.py $$(ESPTOOL_PORT) erase-region $$($$($(1)_BOARD)_SETTINGS) $$($$($(1)_BOARD)_SETTINGS_SIZE)
endif

# Reads <t>_BACKUP_REGIONS into .build/backup/<t>/<time>/<addr>.bin and links it as latest.
# Skipped when the flash still matches latest.
ifndef $(1)_OWN_BACKUP
$(1)-backup:
	l=$$($(1)_BACKUP_ROOT)/latest; \
	if [ -d $$$$l ] && ($$(ZEPHYR_ENV) esptool.py $$(ESPTOOL_PORT) verify-flash \
		$$(foreach r,$$($(1)_BACKUP_REGIONS),$$(firstword $$(subst :, ,$$(r))) $$$$l/$$(firstword $$(subst :, ,$$(r))).bin)); then \
		echo "backup: unchanged, $$$$(readlink -f $$$$l)"; \
	else \
		d=$$($(1)_BACKUP_ROOT)/$$$$(date +%Y%m%d-%H%M%S) && mkdir -p $$$$d && \
		$$(foreach r,$$($(1)_BACKUP_REGIONS),$$(ZEPHYR_ENV) esptool.py $$(ESPTOOL_PORT) read-flash $$(subst :, ,$$(r)) $$$$d/$$(firstword $$(subst :, ,$$(r))).bin && ) \
		ln -sfn $$$$d $$($(1)_BACKUP_ROOT)/latest && echo "backup: $$$$d"; \
	fi
endif

# Writes back latest, or BACKUP=<dir>, then resets the board.
ifndef $(1)_OWN_RESTORE
$(1)-restore:
	d=$$(abspath $$(or $$(BACKUP),$$($(1)_BACKUP_ROOT)/latest)) && \
	$$(foreach r,$$($(1)_BACKUP_REGIONS),test -f $$$$d/$$(firstword $$(subst :, ,$$(r))).bin && ) \
	$$(ZEPHYR_ENV) esptool.py $$(ESPTOOL_PORT) write-flash \
		$$(foreach r,$$($(1)_BACKUP_REGIONS),$$(firstword $$(subst :, ,$$(r))) $$$$d/$$(firstword $$(subst :, ,$$(r))).bin) && \
	echo "restored: $$$$d"
endif

# Backup, tests, restore; the restore runs however the tests end.
SMOKE_ENV_$(1) = SMOKE_TARGET=$(1) SMOKE_FIRMWARE=$$($(1)_BUILD)/main/zephyr/zephyr.signed.bin \
	SMOKE_TFT=$$($$($(1)_DISPLAY)_TFT)
ifndef $(1)_OWN_SMOKE
$(1)-smoke:
	$$(SMOKE_ENV_$(1)) tests/smoke/run.sh $(1) tests/smoke/auto
endif

ifndef $(1)_OWN_SMOKE_INTERACTIVE
$(1)-smoke-interactive:
	$$(SMOKE_ENV_$(1)) tests/smoke/run.sh $(1) tests/smoke/interactive
endif

# Needs the builds of this target and of `make test`.
ifndef $(1)_OWN_TIDY
$(1)-tidy:
	@test -d $$($(1)_BUILD)/main || (echo "$(1) is not built" && exit 1)
	python3 scripts/gen_tidy_db.py $$($(1)_BUILD)/main $$(TEST_ZEPHYR_BUILD) $$(TIDY_DB)/$(1)/compile_commands.json
	$$(call tidy_run,$$(TIDY_DB)/$(1))
endif
endef
