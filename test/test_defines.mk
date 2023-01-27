MACHINE = $(shell $(CC) -dumpmachine)
SRCDIR = $(realpath ../../src)

CTRL_SRCDIR = $(realpath ../../src/ctrl)
DHCPC_SRCDIR = $(realpath ../../src/dhcpc)
STATICC_SRCDIR = $(realpath ../../src/staticc)
PPP_SRCDIR = $(realpath ../../src/ppp)
AUTOSENSING_SRCDIR = $(realpath ../../src/autosensing)
ETHERNET_SRCDIR = $(realpath ../../src/ethernet)
NETMODEL_SRCDIR = $(realpath ../../src/netmodel)
DNS_INT_SRC = $(realpath ../../src/dns)
DSLITE_SRCDIR = $(realpath ../../src/dslite)

OBJDIR = $(realpath ../../output/$(MACHINE)/coverage)
INCDIR = $(realpath ../../include ../../include_priv ../include ../mocks ../test_utils)

HEADERS = $(wildcard $(INCDIR)/*.h)
SOURCES = $(wildcard $(SRCDIR)/*.c)
SOURCES += $(wildcard $(CTRL_SRCDIR)/*.c)
SOURCES += $(wildcard $(DHCPC_SRCDIR)/*.c)
SOURCES += $(wildcard $(PPP_SRCDIR)/*.c)
SOURCES += $(wildcard $(AUTOSENSING_SRCDIR)/*.c)
SOURCES += $(wildcard $(ETHERNET_SRCDIR)/*.c)
SOURCES += $(wildcard $(NETMODEL_SRCDIR)/*.c)
SOURCES += $(wildcard $(DNS_INT_SRC)/*.c)
SOURCES += $(wildcard $(DSLITE_SRCDIR)/*.c)
SOURCES += $(wildcard $(STATICC_SRCDIR)/*.c)


CFLAGS += -Werror -Wall -Wextra -Wno-attributes\
          --std=gnu99 -g3 -Wmissing-declarations \
		  $(addprefix -I ,$(INCDIR)) -I$(OBJDIR)/.. \
		  -fkeep-inline-functions -fkeep-static-functions \
		   -Wno-format-nonliteral \
		  $(shell pkg-config --cflags cmocka) -pthread -DUNIT_TESTS \
		   -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL=500

LDFLAGS += -fkeep-inline-functions -fkeep-static-functions \
		   $(shell pkg-config --libs cmocka) -lamxc -lamxp -lamxd -lamxo -lamxb -lamxm -lamxj -ldl -lsahtrace -lnetmodel
