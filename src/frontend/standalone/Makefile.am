AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DCONFIGDIR='"$(CONFIGDIR)"'

if CONFIGDIR_BINARY
AM_CPPFLAGS += -DCONFIGDIR_BINARY
endif

noinst_LIBRARIES = libfrontend_standalone.a

man1_MANS = \
	cdebootstrap.man

libfrontend_standalone_a_SOURCES = \
	main.c \
	message.c

