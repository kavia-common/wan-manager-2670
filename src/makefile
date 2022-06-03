include ../makefile.inc

# build destination directories
OBJDIR = ../output/$(MACHINE)

# TARGETS
TARGET_SO = $(OBJDIR)/$(COMPONENT).so

# directories
# source directories
SRCDIR = .
CTRL_SRCDIR = ./ctrl
INTEGRATION_SRCDIR = ./integration
DHCPC_INT_SRC = $(INTEGRATION_SRCDIR)/dhcpc
PPP_INT_SRC = $(INTEGRATION_SRCDIR)/ppp
AUTOSENSING_INT_SRC = $(INTEGRATION_SRCDIR)/autosensing
ETHERNET_INT_SRC = $(INTEGRATION_SRCDIR)/ethernet
NETMODEL_INT_SRC = $(INTEGRATION_SRCDIR)/netmodel

INCDIR_PRIV = ../include_priv
INCDIRS = $(INCDIR_PRIV) $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include)
STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib)

# files
SRC = $(wildcard $(SRCDIR)/*.c 				\
 				 $(CTRL_SRCDIR)/*.c 		\
 				 $(INTEGRATION_SRCDIR)/*.c  \
 				 $(NETDEV_INT_SRC)/*.c      \
 				 $(AUTOSENSING_INT_SRC)/*.c \
 				 $(NETMODEL_INT_SRC)/*.c    \
 				 $(DHCPC_INT_SRC)/*.c       \
 				 $(PPP_INT_SRC)/*.c       \
 				 $(ETHERNET_INT_SRC)/*.c)


OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SRC:.c=.o)))

# compilation and linking flags
CFLAGS += -Werror -Wall -Wextra		 				\
          -Wformat=2 -Wshadow 						\
          -Wwrite-strings -Wredundant-decls 		\
          -Wmissing-declarations -Wno-attributes 	\
          -Wno-format-nonliteral 					\
          -fPIC -g3 $(addprefix -I ,$(INCDIRS))

ifeq ($(CC_NAME),g++)
    CFLAGS += -std=c++2a
else
	CFLAGS += -Wstrict-prototypes -Wold-style-definition -Wnested-externs -std=gnu11
endif

LDFLAGS += $(STAGING_LIBDIR) -shared -fPIC -lamxc -lamxd -lamxb -lamxo -lsahtrace -lnetmodel

CFLAGS += -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL=500

# targets
all: $(TARGET_SO)

$(TARGET_SO): $(OBJECTS)
	$(CC) -Wl,-soname,$(COMPONENT).so.$(VMAJOR) -o $@ $(OBJECTS) $(LDFLAGS)

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/%.o: $(CTRL_SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/%.o: $(INTEGRATION_SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/%.o: $(DHCPC_INT_SRC)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)
	
$(OBJDIR)/%.o: $(PPP_INT_SRC)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/%.o: $(AUTOSENSING_INT_SRC)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/%.o: $(ETHERNET_INT_SRC)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/%.o: $(NETMODEL_INT_SRC)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/:
	$(MKDIR) -p $@

clean:
	rm -rf ../output/ ../$(COMPONENT)-*.* ../$(COMPONENT)_*.*

.PHONY: all clean
