APPLICATION = openthread_example

# If no BOARD is found in the environment, use this default:
BOARD ?= hamilton

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= /home/hskim/Desktop/RIOT-OS
#$(CURDIR)/../..

USEMODULE += rtt_stdio
CFLAGS += -DRTT_STDIO_DISABLE_STDIN

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

#CFLAGS += -DLEAF_NODE

# Sensors
USEMODULE += tmp006
USEMODULE += hdc1000
USEMODULE += fxos8700

#USEMODULE += crypto
#USEMODULE += cipher_modes

USEMODULE += openthread
USEMODULE += at86rf233

USEMODULE += random

#USEMODULE += shell
#USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += ipv6_addr

#required for C++ compiling
CXXEXFLAGS += -fno-rtti
USEMODULE += cpp11-compat

# Needed by posix_sockets example

#USEMODULE += gnrc_sock_udp
#USEMODULE += posix_sockets
#USEMODULE += conn_udp

DEFAULT_CHANNEL = 25
CFLAGS += -DIEEE802154_DEFAULT_CHANNEL=$(DEFAULT_CHANNEL)

FEATURES_REQUIRED += periph_timer

include $(RIOTBASE)/Makefile.include
