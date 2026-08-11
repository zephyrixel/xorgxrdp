EXTRA_DIST =

AM_CFLAGS = \
  $(XRDP_CFLAGS) \
  $(XORG_SERVER_CFLAGS) \
  -DPACKAGE_VERSION_MAJOR=@package_version_major@ \
  -DPACKAGE_VERSION_MINOR=@package_version_minor@ \
  -DPACKAGE_VERSION_PATCHLEVEL=@package_version_patchlevel@ \
  -I$(top_srcdir)/module

xrdptouch_drv_la_LTLIBRARIES = xrdptouch_drv.la

xrdptouch_drv_la_LDFLAGS = -module -avoid-version

xrdptouch_drv_ladir = $(moduledir)/input

xrdptouch_drv_la_SOURCES = rdpTouch.c

xrdptouch_drv_la_LIBADD =
