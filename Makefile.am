AUTOMAKE_OPTIONS = dist-bzip2

if WITH_GTK
MAYBE_IMAGES = images
endif

# dependencies
#   src: images     (pixbufs.h)
#   protocol: util  (libgxr)
SUBDIRS = data doc po util protocol $(MAYBE_IMAGES) src

EXTRA_DIST = ChangeLog \
	     intltool-extract.in intltool-merge.in intltool-update.in
DISTCLEANFILES = intltool-extract.in intltool-merge.in intltool-update.in
