AC_DEFUN([LJ_PKG_ARG_WITH], [LJ_PKG_ARG_WITHA($1, $2, [$3], $4, [with_$1=yes])])
AC_DEFUN([LJ_PKG_ARG_WITHA], [
  HAVE_$2=no

  AC_ARG_WITH($1, [$3], , $5)

  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi

  if test "$PKG_CONFIG" = "no" ; then
    echo "*** The pkg-config script could not be found. Make sure it is"
    echo "*** in your path, or set the PKG_CONFIG environment variable"
    echo "*** to the full path to pkg-config."
    echo "*** Or see http://www.freedesktop.org/software/pkgconfig to get pkg-config."
  else
    if test "$with_$1" = "yes"; then
      AC_MSG_CHECKING(for $4)
      if $PKG_CONFIG --exists "$4" ; then
	AC_MSG_RESULT(yes)
	HAVE_$2=yes
	AC_DEFINE(HAVE_$2,1,[do we have $1?])
	MODULES="$MODULES $4"
      else
	AC_MSG_RESULT(not found)
	with_$1=no
      fi
    fi
  fi

])

dnl From licq: Copyright (c) 2000 Dirk Mueller <[EMAIL PROTECTED]>
dnl Check if the type socklen_t is defined anywhere
AC_DEFUN([AC_C_SOCKLEN_T],
[AC_CACHE_CHECK(for socklen_t, ac_cv_c_socklen_t,
[
  AC_TRY_COMPILE([
    #include <sys/types.h>
    #include <sys/socket.h>
  ],[
    socklen_t foo;
  ],[
    ac_cv_c_socklen_t=yes
  ],[
    ac_cv_c_socklen_t=no
  ])
])
if test $ac_cv_c_socklen_t = no; then
  AC_DEFINE(socklen_t, int, [define to int if socklen_t not available])
fi
])

dnl vim: set sw=2 :
