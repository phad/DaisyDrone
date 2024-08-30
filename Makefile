# Project Name
TARGET = DaisyDrone

# Sources
CPP_SOURCES = DaisyDrone.cpp

# Library Locations
LIBDAISY_DIR = ../../electro-smith/DaisyExamples/libDaisy/
DAISYSP_DIR = ../../electro-smith/DaisyExamples/DaisySP/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

ifeq ($(DEBUG), 1)
	OPT = -Og
endif
