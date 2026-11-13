NXWRIGHT ?= $(HOME)/apps/nxwright/.venv/bin/nxwright
NXWRIGHT_COMPILE_ARGS ?=

# $(eval $(call nextion_display,<d>)): the .tft (also rotated 180), serial upload, screenshots.
define nextion_display
$(1)_DIR := $(CURDIR)/displays/$(1)
$(1)_OUT := $(CURDIR)/.build/release/$(1)
$(1)_TFT := $(CURDIR)/.build/release/$(1)/display.tft
$(1)_TFT_180 := $(CURDIR)/.build/release/$(1)/display_180.tft
$(1)_180_SRC := $(CURDIR)/.build/$(1)_180

.PHONY: $(1)-build $(1)-serial $(1)-screenshots

$(1)-build:
	$$(NXWRIGHT) check $$($(1)_DIR)
	mkdir -p $$($(1)_OUT)
	$$(NXWRIGHT) build $$($(1)_DIR) $$($(1)_OUT)/display.HMI --overwrite --no-backup
	$$(NXWRIGHT) compile $$($(1)_OUT)/display.HMI $$($(1)_TFT) --overwrite --no-backup $$(NXWRIGHT_COMPILE_ARGS)
	rm -rf $(CURDIR)/.build/$(1)_180
	mkdir -p $$($(1)_180_SRC)
	cp -r $$($(1)_DIR)/app.toml $$($(1)_DIR)/defaults.toml $$($(1)_DIR)/program.nx $$($(1)_DIR)/pages $$($(1)_DIR)/fonts $$($(1)_180_SRC)
	sed -i 's/^rotation = .*/rotation = 180/' $$($(1)_180_SRC)/app.toml
	$$(NXWRIGHT) build $$($(1)_180_SRC) $$($(1)_OUT)/display_180.HMI --overwrite --no-backup
	$$(NXWRIGHT) compile $$($(1)_OUT)/display_180.HMI $$($(1)_TFT_180) --overwrite --no-backup $$(NXWRIGHT_COMPILE_ARGS)

# Uploads the .tft over the USB console, the firmware stores then flashes it.
$(1)-serial:
	python ./tools/monitor.py --tft $$($(1)_TFT) --exit-after-upload

$(1)-screenshots:
	. $(HOME)/apps/nxwright/.venv/bin/activate && python3 scripts/screenshots.py
endef

# $(eval $(call nextion_target,<t>)) before zephyr_target: the display's .tft in the display partition.
define nextion_target
$(1)_FLASH_DEPS += $(1)-display-image
$(1)_IMAGES += $$($(1)_DISPLAY_PARTITION):$(CURDIR)/.build/$(1)/display.bin
$(1)_BACKUP_REGIONS += $$($(1)_DISPLAY_PARTITION):$$($(1)_DISPLAY_PARTITION_SIZE)

.PHONY: $(1)-display-image

$(1)-display-image:
	python3 scripts/pack_display_image.py $$($$($(1)_DISPLAY)_TFT) $(CURDIR)/.build/$(1)/display.bin
endef
