dist_noinst_DATA = \
	DEBIAN/control

dist_noinst_SCRIPTS = \
	DEBIAN/postinst \
	DEBIAN/postrm

package:
	@list='$(dist_noinst_DATA)'; \
	for p in $$list; do \
	  d=$$(dirname $$p); \
	  p=$(srcdir)/$$p; \
	  if test ! -e "$(DESTDIR)/$$d"; then \
	    echo " $(INSTALL) -d '$(DESTDIR)/$$d'"; \
	    $(INSTALL) -d "$(DESTDIR)/$$d"; \
	  fi; \
	  if test -f $$p; then \
	    echo " $(INSTALL) -m644 '$$p' '$(DESTDIR)/$$d/'"; \
	    $(INSTALL) -m644 "$$p" "$(DESTDIR)/$$d/" || exit 1; \
	  fi; \
	done
	@list='$(dist_noinst_SCRIPTS)'; \
	for p in $$list; do \
	  d=$$(dirname $$p); \
	  p=$(srcdir)/$$p; \
	  if test ! -e "$(DESTDIR)/$$d"; then \
	    echo " $(INSTALL) -d '$(DESTDIR)/$$d'"; \
	    $(INSTALL) -d "$(DESTDIR)/$$d"; \
	  fi; \
	  if test -f $$p; then \
	    echo " $(INSTALL) -m755 '$$p' '$(DESTDIR)/$$d/'"; \
	    $(INSTALL) -m755 "$$p" "$(DESTDIR)/$$d/" || exit 1; \
	  fi; \
	done

