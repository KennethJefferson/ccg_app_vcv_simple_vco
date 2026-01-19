# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= C:/dev-vcvrack/Rack-SDK

# Source files
SOURCES += src/plugin.cpp
SOURCES += src/SimpleVCO.cpp

# Add res folder to distributables
DISTRIBUTABLES += res

# Include the VCV Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk
