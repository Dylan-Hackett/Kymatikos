# Project Name
TARGET = kymatikos

# Library Locations
LIBDAISY_DIR = lib/libdaisy
DAISYSP_DIR = lib/DaisySP
NIMBUS_DIR = eurorack/Nimbus_SM
MPR121_DIR = src/platform

ifeq ($(BLINK_TEST),1)
CPP_SOURCES = src/app/BlinkTest.cpp
else
# Sources - Define BEFORE including core Makefile
CPP_SOURCES += src/app/Kymatikos.cpp \
              src/app/Interface.cpp \
              src/dsp/Arpeggiator.cpp \
              src/dsp/AudioProcessor.cpp \
              src/platform/mpr121_daisy.cpp \
              src/platform/SynthStateStorage.cpp \
              src/system/HardwareManager.cpp \
              src/system/ControlsManager.cpp \
             src/system/AudioEngine.cpp \
             $(NIMBUS_DIR)/resources.cpp

CPP_SOURCES += $(wildcard $(NIMBUS_DIR)/dsp/*.cpp)
CPP_SOURCES += $(wildcard $(NIMBUS_DIR)/dsp/pvoc/*.cpp)

# Define DaisySP sources *before* including core Makefile
DAISYSP_SOURCES += $(wildcard $(DAISYSP_DIR)/Source/*.cpp)
DAISYSP_SOURCES += $(wildcard $(DAISYSP_DIR)/Source/*/*.cpp)
endif

# Define Includes BEFORE including core Makefile
C_INCLUDES += \
-I$(MPR121_DIR) \
-Isrc/app \
-Isrc/dsp \
-Isrc/system \
-Isrc/platform \
-Isrc/config \
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
include $(SYSTEM_FILES_DIR)/Makefile # Include core makefile

# --- Additions/Overrides AFTER Core Makefile --- 

# Ensure 'all' target only builds the final elf
# all: $(BUILD_DIR)/$(TARGET).elf # COMMENTED OUT TO ALLOW CORE MAKEFILE TO BUILD .bin

# Explicitly override the linker rule AFTER OBJECTS is fully populated
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	@echo Linking $(TARGET).elf with updated OBJECTS list...
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

# No need to override other rules (all, .c, .cpp, .bin, .hex, clean, etc.)
# Let the core Makefile handle those.

# -------------------------------------------------------------
# Convenience targets for QSPI workflow
# -------------------------------------------------------------
# 1. flash-stub : Builds the project with the internal-flash linker
#    and flashes it to alt-setting 0 (0x08000000).  This becomes the
#    tiny boot stub that jumps to QSPI.
# 2. flash-app  : Normal build (QSPI linker) + flash to alt-setting 1
#    (0x90040000).
# -------------------------------------------------------------

flash-stub:
	$(MAKE) clean
	$(MAKE) program-boot

flash-app:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) program-app

# Flash application code directly to QSPI external flash via the QSPI bootloader stub (alt 0)
program-app:
	@echo "Flashing application to QSPI..."
	-dfu-util -a 0 -s $(QSPI_ADDRESS):leave -D $(BUILD_DIR)/$(TARGET_BIN) -d ,0483:df11

program-sram:
	@echo "Loading into SRAM via OpenOCD..."
	$(OCD) -s $(OCD_DIR) $(OCDFLAGS) -c "init; reset halt; load $(BUILD_DIR)/$(TARGET).elf; reset init; exit"

# Override program-boot locally so detach-error (libusb code-74) doesn't abort the make.
program-boot:
	@echo "Flashing bootloader stub to internal flash…"
	-dfu-util -a 0 -s 0x08000000:leave -D $(BOOT_BIN) -d ,0483:df11

# Override program-dfu to build all and then flash via program-app
.PHONY: program-dfu
program-dfu: all program-app

.PHONY: flash-stub flash-app program-sram blink-test blink-flash

# Minimal blink + print test firmware (build only)
blink-test:
	$(MAKE) clean
	$(MAKE) BLINK_TEST=1 all

# Build and flash the blink test image to QSPI
blink-flash:
	$(MAKE) clean
	$(MAKE) BLINK_TEST=1 all
	$(MAKE) BLINK_TEST=1 program-app
