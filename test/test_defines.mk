MACHINE = $(shell $(CC) -dumpmachine)
SRCDIR = $(realpath ../../src)

CTRL_SRCDIR = $(realpath ../../src/ctrl)
INTEGRATION_SRCDIR = $(realpath ../../src/integration)
DHCPC_SRCDIR = $(realpath ../../src/integration/dhcpc)
AUTOSENSING_SRCDIR = $(realpath ../../src/integration/autosensing)
ETHERNET_SRCDIR = $(realpath ../../src/integration/ethernet)
NETMODEL_SRCDIR = $(realpath ../../src/integration/netmodel)


OBJDIR = $(realpath ../../output/$(MACHINE)/coverage)
INCDIR = $(realpath ../../include ../../include_priv ../include ../mocks ../test_utils)

HEADERS = $(wildcard $(INCDIR)/*.h)
SOURCES = $(wildcard $(SRCDIR)/*.c)
SOURCES += $(wildcard $(CTRL_SRCDIR)/*.c)
SOURCES += $(wildcard $(INTEGRATION_SRCDIR)/*.c)
SOURCES += $(wildcard $(DHCPC_SRCDIR)/*.c)
SOURCES += $(wildcard $(AUTOSENSING_SRCDIR)/*.c)
SOURCES += $(wildcard $(ETHERNET_SRCDIR)/*.c)
SOURCES += $(wildcard $(NETMODEL_SRCDIR)/*.c)


CFLAGS += -Werror -Wall -Wextra -Wno-attributes\
          --std=gnu99 -g3 -Wmissing-declarations \
		  $(addprefix -I ,$(INCDIR)) -I$(OBJDIR)/.. \
		  -fkeep-inline-functions -fkeep-static-functions \
		   -Wno-format-nonliteral \
		  $(shell pkg-config --cflags cmocka) -pthread -DUNIT_TESTS \
		   -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL=500

LDFLAGS += -fkeep-inline-functions -fkeep-static-functions \
		   $(shell pkg-config --libs cmocka) -lamxc -lamxp -lamxd -lamxo -lamxb -ldl -lsahtrace -lnetmodel
