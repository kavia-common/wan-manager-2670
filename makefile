include makefile.inc

NOW = $(shell date +"%Y-%m-%d(%H:%M:%S %z)")

# Extra destination directories
PKGDIR = ./output/$(MACHINE)/pkg/

# helper functions - used in multiple targets
define install_to
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT).odl $(1)/etc/amx/$(COMPONENT)/$(COMPONENT).odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_definition.odl $(1)/etc/amx/$(COMPONENT)/$(COMPONENT)_definition.odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_WAN_definition.odl $(1)/etc/amx/$(COMPONENT)/$(COMPONENT)_WAN_definition.odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_WAN_Intf_definition.odl $(1)/etc/amx/$(COMPONENT)/$(COMPONENT)_WAN_Intf_definition.odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_defaults.odl $(1)/etc/amx/$(COMPONENT)/$(COMPONENT)_defaults.odl
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/$(COMPONENT).so $(1)/usr/lib/amx/$(COMPONENT)/$(COMPONENT).so
	$(INSTALL) -d -m 0755 $(1)$(BINDIR)
	$(INSTALL) -D -p -m 0755 scripts/$(COMPONENT).sh $(1)$(INITDIR)/$(COMPONENT)
endef

define create_changelog
	@$(ECHO) "Update changelog"
	mv CHANGELOG.md CHANGELOG.md.bak
	head -n 9 CHANGELOG.md.bak > CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	$(ECHO) "## Release $(VERSION) - $(NOW)" >> CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	$(GIT) log --pretty=format:"- %s" $$($(GIT) describe --tags | grep -v "merge" | cut -d'-' -f1)..HEAD  >> CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	tail -n +10 CHANGELOG.md.bak >> CHANGELOG.md
	rm CHANGELOG.md.bak
endef

# targets
all:
	$(MAKE) -C src all

clean:
	$(MAKE) -C src clean

install: all
	$(call install_to,$(DEST))
	ln -sfr $(DEST)$(BINDIR)/amxrt $(DEST)$(BINDIR)/$(COMPONENT)

package: all
	$(call install_to,$(PKGDIR))
	cd $(PKGDIR) && $(TAR) -czvf ../$(COMPONENT)-$(VERSION).tar.gz .
	cp $(PKGDIR)../$(COMPONENT)-$(VERSION).tar.gz .
	make -C packages

changelog:
	$(call create_changelog)

test:
	$(MAKE) -C test run
	$(MAKE) -C test coverage

.PHONY: all clean changelog install package test