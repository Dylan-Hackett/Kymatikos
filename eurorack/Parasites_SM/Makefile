# Project Name
TARGET = Parasites
APP_TYPE = BOOT_QSPI

# Sources
CPP_SOURCES += \
Parasites.cpp \
clouds/dsp/correlator.cpp \
clouds/dsp/granular_processor.cpp \
clouds/dsp/mu_law.cpp \
clouds/dsp/pvoc/frame_transformation.cpp \
clouds/dsp/pvoc/phase_vocoder.cpp \
clouds/dsp/pvoc/stft.cpp \
clouds/resources.cpp \
stmlib/dsp/units.cpp \
stmlib/dsp/atan.cpp \
stmlib/utils/random.cpp \

# Library Locations
LIBDAISY_DIR = ../../lib/libdaisy
DAISYSP_DIR = ../../lib/DaisySP

# Include paths for Mutable/Parasites sources
C_INCLUDES += \
-I. \
-I.. \
-Iclouds \
-Iclouds/dsp \
-Iclouds/dsp/pvoc \
-Iclouds/dsp/fx \
-Istmlib \
-Istmlib/dsp \
-Istmlib/utils \

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
