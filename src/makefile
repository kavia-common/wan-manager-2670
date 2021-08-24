include ../makefile.inc

# build destination directories
OUTPUTDIR = ../output/$(MACHINE)
OBJDIR = $(OUTPUTDIR)

# TARGETS
TARGET_SO = $(OBJDIR)/$(COMPONENT).so

# directories
# source directories
SRCDIR = .
INCDIR_PRIV = ../include_priv
INCDIRS = $(INCDIR_PRIV) $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include)
STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib)

CTRL_SRCDIR= $(SRCDIR)/ctrl

SOURCES = $(wildcard $(SRCDIR)/*.c)
CTRL_SOURCES = $(wildcard $(CTRL_SRCDIR)/*.c)

OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))
CTRL_OBJECTS = $(addprefix $(OBJDIR)/ctrl/,$(notdir $(CTRL_SOURCES:.c=.o)))


# compilation and linking flags
CFLAGS += -Werror -Wall -Wextra \
          -Wformat=2 -Wshadow \
          -Wwrite-strings -Wredundant-decls \
          -Wmissing-declarations -Wno-attributes \
		  -Wno-format-nonliteral \
		  -fPIC -g3 $(addprefix -I ,$(INCDIRS))

ifeq ($(CC_NAME),g++)
    CFLAGS += -std=c++2a
else
	CFLAGS += -Wstrict-prototypes -Wold-style-definition -Wnested-externs -std=c11
endif

LDFLAGS += -shared -fPIC $(STAGING_LIBDIR) \
           -lamxc -lamxp -lamxd -lamxo -lsahtrace

CFLAGS += -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL=500
# targets
all: $(TARGET_SO)

$(TARGET_SO): $(OBJECTS) $(CTRL_OBJECTS)
	$(CC) -Wl,-soname,$(COMPONENT).so -o $@ $(OBJECTS) $(CTRL_OBJECTS) $(LDFLAGS)

-include $(OBJECTS:.o=.d)
-include $(CTRL_OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/ctrl/%.o: ctrl/%.c | $(OBJDIR)/ctrl/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/:
	$(MKDIR) -p $@

$(OBJDIR)/ctrl/:
	$(MKDIR) -p $@


clean:
	rm -rf ../output/ ../$(COMPONENT)-*.* ../$(COMPONENT)_*.*

.PHONY: all clean
