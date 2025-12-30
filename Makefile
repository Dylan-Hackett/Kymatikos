# Project Name
TARGET = kymatikos

# Library Locations
LIBDAISY_DIR = lib/libdaisy
DAISYSP_DIR = lib/DaisySP
NIMBUS_DIR = eurorack/Nimbus_SM

# Sources
CPP_SOURCES += src/Kymatikos.cpp \
              src/audio.cpp \
              src/Arpeggiator.cpp \
              src/mpr121.cpp \
              src/hardware.cpp \
              src/controls.cpp \
              $(NIMBUS_DIR)/resources.cpp

CPP_SOURCES += $(wildcard $(NIMBUS_DIR)/dsp/*.cpp)
CPP_SOURCES += $(wildcard $(NIMBUS_DIR)/dsp/pvoc/*.cpp)

DAISYSP_SOURCES += $(wildcard $(DAISYSP_DIR)/Source/*.cpp)
DAISYSP_SOURCES += $(wildcard $(DAISYSP_DIR)/Source/*/*.cpp)

# Include paths - flat src/ directory
C_INCLUDES += \
-Isrc \
-I. \
-Iresources \
-I$(NIMBUS_DIR) \
-I$(NIMBUS_DIR)/dsp \
-I$(NIMBUS_DIR)/dsp/fx \
-I$(NIMBUS_DIR)/dsp/pvoc \
-Ieurorack \
-I../.. \
-I$(LIBDAISY_DIR)/Drivers/CMSIS_5/CMSIS/Core/Include \
-I$(LIBDAISY_DIR)/Drivers/CMSIS-Device/ST/STM32H7xx/Include \
-I$(LIBDAISY_DIR)/Drivers/STM32H7xx_HAL_Driver/Inc

# Hardware target
HWDEFS = -DPATCH_SM

# Ensure build is treated as boot application (code executes from QSPI)
C_DEFS += -DBOOT_APP
APP_TYPE = BOOT_QSPI

# Warning suppression
C_INCLUDES += -Wno-unused-local-typedefs

# Optimization level (can be overridden)
OPT ?= -Os

# Set target Linker Script to QSPI (no 256k offset)
LDSCRIPT = $(LIBDAISY_DIR)/core/STM32H750IB_qspi.lds

# Override QSPI write address to match linker (no 0x40000 offset)
QSPI_ADDRESS = 0x90040000

# Add QSPI section start flags
LDFLAGS += -Wl,--gc-sections

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Explicitly override the linker rule AFTER OBJECTS is fully populated
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	@echo Linking $(TARGET).elf with updated OBJECTS list...
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

# -------------------------------------------------------------
# Convenience targets for QSPI workflow
# -------------------------------------------------------------
flash-stub:
	$(MAKE) clean
	$(MAKE) program-boot

flash-app:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) program-app

program-app:
	@echo "Flashing application to QSPI..."
	-dfu-util -a 0 -s $(QSPI_ADDRESS):leave -D $(BUILD_DIR)/$(TARGET_BIN) -d ,0483:df11

program-sram:
	@echo "Loading into SRAM via OpenOCD..."
	$(OCD) -s $(OCD_DIR) $(OCDFLAGS) -c "init; reset halt; load $(BUILD_DIR)/$(TARGET).elf; reset init; exit"

program-boot:
	@echo "Flashing bootloader stub to internal flash…"
	-dfu-util -a 0 -s 0x08000000:leave -D $(BOOT_BIN) -d ,0483:df11

.PHONY: program-dfu
program-dfu: all program-app

.PHONY: flash-stub flash-app program-sram p

p:
	@$(MAKE) -s program-app
