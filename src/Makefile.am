AM_CPPFLAGS = \
	-I$(top_srcdir)/include

SUBDIRS = frontend

bin_PROGRAMS = cdebootstrap

cdebootstrap_SOURCES = \
	check.c \
	decompress_bz.c \
	decompress_gz.c \
	decompress_xz.c \
	decompress_null.c \
	download.c \
	execute.c \
	gpg.c \
	install.c \
	main.c \
	log.c \
	package.c \
	suite.c \
	suite_action.c \
	suite_config.c \
	suite_packages.c \
	target.c

cdebootstrap_LDADD = \
	frontend/@FRONTEND@/libfrontend_@FRONTEND@.a @FRONTEND_LIBS@ \
	-ldebian-installer-extra -ldebian-installer -lbz2 -llzma -lz

cdebootstrap_DEPENDENCIES = \
	frontend/@FRONTEND@/libfrontend_@FRONTEND@.a

