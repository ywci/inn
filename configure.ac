#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

AC_INIT([Q1],0.0.1)

# initialize automake
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE

# Checks for programs.
AC_ARG_WITH(cppunit,
        [  --without-cppunit       do not use CPPUNIT])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S

# AC_DISABLE_SHARED
AC_PROG_LIBTOOL

#enable -D_GNU_SOURCE since the return code value of getaddrinfo
#ifdefed with __USE_GNU
#features.h header undef's __USE_GNU and defines it only if _GNU_SOURCE is defined
#hence this define for gcc
AC_ARG_ENABLE([debug],
 [AS_HELP_STRING([--enable-debug],[enable debug build [default=no]])],
 [CFLAGS="-g -O0 -D_GNU_SOURCE";CXXFLAGS="-g -O0"],[CFLAGS="-g -O0 -D_GNU_SOURCE";CXXFLAGS="-g -O0"])
 
AC_ARG_WITH([syncapi],
 [AS_HELP_STRING([--with-syncapi],[build with support for SyncAPI [default=yes]])],
 [],[with_syncapi=yes])

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_mutex_lock],[have_pthread=yes],[have_pthread=no])

if test "x$with_syncapi" != xno && test "x$have_pthread" = xno; then
    AC_MSG_WARN([cannot build SyncAPI -- pthread not found])
    with_syncapi=no
fi
if test "x$with_syncapi" != xno; then
    AC_MSG_NOTICE([building with SyncAPI support])
else
    AC_MSG_NOTICE([building without SyncAPI support])
fi

AM_CONDITIONAL([WANT_SYNCAPI],[test "x$with_syncapi" != xno])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h sys/utsname.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME
AC_CHECK_TYPE([nfds_t],
    [AC_DEFINE([POLL_NFDS_TYPE],[nfds_t],[poll() second argument type])],
    [AC_DEFINE([POLL_NFDS_TYPE],[unsigned int],[poll() second argument type])],
    [#include <poll.h>])

AC_MSG_CHECKING([whether to enable ipv6])

AC_TRY_RUN([ /* is AF_INET6 available? */
#include <sys/types.h>
#include <sys/socket.h>
main()
{
 if (socket(AF_INET6, SOCK_STREAM, 0) < 0)
   exit(1);
 else
   exit(0);
}
], AC_MSG_RESULT(yes) 
   ipv6=yes, 
   AC_MSG_RESULT(no) 
   ipv6=no, 
   AC_MSG_RESULT(no) 
   ipv6=no)

if test x"$ipv6" = xyes; then
  USEIPV6="-DZOO_IPV6_ENABLED"
  AC_SUBST(USEIPV6)
fi

# Checks for library functions.
AC_CHECK_FUNCS([getcwd gethostbyname gethostname getlogin getpwuid_r gettimeofday getuid memmove memset poll socket strchr strdup strerror strtol])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
