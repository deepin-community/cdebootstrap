packages = \
	cdebootstrap-helper-apt \
	cdebootstrap-helper-makedev \
	cdebootstrap-helper-rc.d

packages_files = $(foreach package,$(packages),$(package).deb)

build: $(packages_files)

install-data-local: $(packages_files)
	test -z "$(configdir)" || $(mkdir_p) "$(DESTDIR)$(configdir)"
	@list='$(packages_files)'; for p in $$list; do \
	if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(configdir)/$$f'"; \
	  $(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(configdir)/$$f"; \
	done

%.deb: PACKAGE=$(@:.deb=)
%.deb:
	$(MAKE) temp-$(PACKAGE) PACKAGE=$(PACKAGE)
	$(MAKE) root-$(PACKAGE) PACKAGE=$(PACKAGE)

temp-%:
	$(MAKE) -C $(PACKAGE) package DESTDIR=$(CURDIR)/temp-$(PACKAGE)

root-%:
	dpkg-deb -Zgzip --build temp-$(PACKAGE) $(PACKAGE).deb

clean-local:
	rm -rf $(foreach package,$(packages),temp-$(package))
	rm -f files $(packages_files)

SUBDIRS = $(packages)
