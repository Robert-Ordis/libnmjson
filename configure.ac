#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])

# ------------------------------------------------------------
# バージョンの唯一の定義元。更新時はここだけを書き換える。
#   -> AC_INIT(PACKAGE_VERSION, make dist のアーカイブ名)
#   -> include/nmjson/nmjson_version.h (NMJSON_VERSION_*)
#   -> nmjson_get_build_ver() 等
# ------------------------------------------------------------
m4_define([nmjson_ver_major], [0])
m4_define([nmjson_ver_minor], [11])
m4_define([nmjson_ver_patch], [0])
m4_define([nmjson_ver_tails], [])	dnl 例: [-rc1]

AC_INIT([libnmjson], [nmjson_ver_major.nmjson_ver_minor.nmjson_ver_patch[]nmjson_ver_tails], [BUG-REPORT-ADDRESS])
AC_SUBST([NMJSON_VERSION_MAJOR], [nmjson_ver_major])
AC_SUBST([NMJSON_VERSION_MINOR], [nmjson_ver_minor])
AC_SUBST([NMJSON_VERSION_PATCH], [nmjson_ver_patch])
AC_SUBST([NMJSON_VERSION_TAILS], [nmjson_ver_tails])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([include/nmjson/json_const.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h stddef.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([memmove memset strncasecmp strstr strtol strtoul strtoull])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 include/nmjson/nmjson_version.h
                 src/Makefile
                 test/Makefile])
AC_OUTPUT
