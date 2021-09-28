#
# ASH Makefile
#

# Common Compiler Flags ########################################################

# Include paths.
COMMON_CFLAGS += -Iash/include
COMMON_CFLAGS += -Iash/include/ash_api

# Simulator-specific Source Files ##############################################

SIM_SRCS += ash/platform/linux/ash.cc
