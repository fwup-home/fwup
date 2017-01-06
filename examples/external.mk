# Include custom packages
include $(sort $(wildcard $(BR2_EXTERNAL_FWUP_EXAMPLES_PATH)/package/*/*.mk))

FW_FILE=$(firstword $(wildcard $(BINARIES_DIR)/*.fw))

# Replace everything on the SDCard with new bits
burn-complete: burn
burn:
	@if [ -e "$(FW_FILE)" ]; then \
		echo "Burning $(FW_FILE)..."; \
		sudo $(HOST_DIR)/usr/bin/fwup -a -i $(FW_FILE) -t complete; \
	else \
		echo "ERROR: No firmware found. Check that 'make' completed successfully"; \
		echo "and that a firmware (.fw) file is in $(BINARIES_DIR)."; \
	fi

# Upgrade the image on the SDCard (app data won't be removed)
# This is usually the fastest way to update an SDCard that's already
# been programmed. It won't update bootloaders, so if something is
# really messed up, burn-complete may be better.
burn-upgrade:
	@if [ -e "$(FW_FILE)" ]; then \
		echo "Upgrading $(FW_FILE)..."; \
		sudo $(HOST_DIR)/usr/bin/fwup -a -i $(FW_FILE) -t upgrade; \
	else \
		echo "ERROR: No firmware found. Check that 'make' completed successfully"; \
		echo "and that a firmware (.fw) file is in $(BINARIES_DIR)."; \
	fi

help: fwup-examples-help

fwup-examples-help:
	@echo "Fwup Examples Help"
	@echo "------------------"
	@echo
	@echo "This build directory is configured to create the system described in:"
	@echo
	@echo "$(BR2_DEFCONFIG)"
	@echo
	@echo "Building:"
	@echo "  all                           - Build the current configuration"
	@echo "  burn                          - Burn the most recent build to an SDCard"
	@echo "                                  (invokes sudo)"
	@echo "  clean                         - Clean everything"
	@echo
	@echo "Configuration:"
	@echo "  menuconfig                    - Run Buildroot's menuconfig"
	@echo "  linux-menuconfig              - Run menuconfig on the Linux kernel"
	@echo "  busybox-menuconfig            - Run menuconfig on Busybox to enable shell"
	@echo "                                  commands and more"
	@echo
	@echo "---------------------------------------------------------------------------"
	@echo
	@echo "Buildroot Help"
	@echo "--------------"

.PHONY: burn burn-complete burn-upgrade fwup-examples-help
