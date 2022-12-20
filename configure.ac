AC_INIT([cdebootstrap],m4_esyscmd(dpkg-parsechangelog | perl -ne 'print $1 if m/^Version: (.*)$/;'))
AM_INIT_AUTOMAKE([foreign no-define])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

AC_CANONICAL_TARGET

AC_PROG_CC
AC_PROG_RANLIB

AC_ARG_ENABLE(static,
	AS_HELP_STRING(--enable-static,static link binaries),
	[], [enable_static=no])

if test "$enable_static" != no; then
	LDFLAGS="$LDFLAGS -static"
fi

AC_PATH_PROG(DPKG_ARCHITECTURE,dpkg-architecture)
AC_ARG_WITH(dpkg-arch,
	AS_HELP_STRING(--with-dpkg-arch=NAME,which dpkg architecture to use),
	[dpkg_arch="$withval"],[
		if test -n "$DPKG_ARCHITECTURE"; then
			dpkg_arch=$($DPKG_ARCHITECTURE -t$target_cpu-$target_os -qDEB_HOST_ARCH)
		else
			AC_MSG_WARN([can't determine target dpkg architecture])
		fi
	])

AC_MSG_CHECKING(dpkg architecture)
AC_DEFINE_UNQUOTED([DPKG_ARCH],["$dpkg_arch"],[Define to the default dpkg architecture])
AC_MSG_RESULT($dpkg_arch)

AC_ARG_WITH(frontend,
	AS_HELP_STRING(--with-frontend=NAME,which frontend to use),
	[FRONTEND=$withval],[FRONTEND=standalone])

AC_MSG_CHECKING(for frontend)
if test "$FRONTEND" = "debian-installer"; then
	FRONTEND_LIBS="-ldebconfclient"
elif test "$FRONTEND" = "standalone"; then
	if test "$enable_static" = no; then
		AC_DEFINE([HAVE_LIBCURL],[1],[Define if libcurl is available and should be used])
		PKG_CHECK_MODULES([libcurl],[libcurl])
		FRONTEND_LIBS="$libcurl_LIBS"
	fi
else
	AC_MSG_ERROR([unknown frontend $FRONTEND], 1)
fi
AC_MSG_RESULT($FRONTEND)

AC_SUBST(FRONTEND)
AC_SUBST(FRONTEND_LIBS)

AC_MSG_CHECKING(for configdir)
AC_ARG_WITH(configdir,
	AS_HELP_STRING(--with-configdir=DIR,a),
	[
CONFIGDIR="$withval"
if test "${CONFIGDIR:0:7}" = "BINARY/"; then
	configdir="\${bindir}${CONFIGDIR:6}"
	CONFIGDIR="${CONFIGDIR:6}"
	CONFIGDIR_BINARY=1
	AC_MSG_RESULT([path of binary/$CONFIGDIR])
else
	configdir="$CONFIGDIR"
	AC_MSG_RESULT([$CONFIGDIR])
fi
],[
CONFIGDIR="\${datadir}/$PACKAGE_NAME"
configdir="$CONFIGDIR"
AC_MSG_RESULT(default)
])

AC_SUBST([configdir])
AC_SUBST([CONFIGDIR])
AM_CONDITIONAL([CONFIGDIR_BINARY], [test "$CONFIGDIR_BINARY"])

if test "$GCC" = yes; then
	CFLAGS="$CFLAGS -std=gnu99"
fi

AC_CONFIG_FILES([
Makefile
config/Makefile
helper/Makefile
helper/cdebootstrap-helper-apt/Makefile
helper/cdebootstrap-helper-makedev/Makefile
helper/cdebootstrap-helper-rc.d/Makefile
include/Makefile
src/Makefile
src/frontend/Makefile
src/frontend/debian-installer/Makefile
src/frontend/standalone/Makefile
])

AC_OUTPUT
