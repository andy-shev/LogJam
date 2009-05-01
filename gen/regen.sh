#!/bin/sh

set -v
./gen.pl cmdline_data.h.tmpl > ../src/cmdline_data.h
./gen.pl logjam.tmpl-doc.pl > logjam-doc.pl
perl ./logjam-doc.pl man html && ( mv logjam.1 ../doc ; mv logjam.1.html ../doc )
