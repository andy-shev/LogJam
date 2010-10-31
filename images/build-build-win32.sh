srcdir=.

IMAGES="\
	logjam_ljuser.png     \
	logjam_ljcomm.png     \
	logjam_twuser.png     \
	logjam_private.png    \
	logjam_protected.png  \
	logjam_pencil.png     \
	logjam_music.png      \
	logjam_mood.png       \
	logjam_rarrow.png     \
	logjam_larrow.png     \
	logjam_throbber_1.png \
	logjam_throbber_2.png \
	logjam_throbber_3.png \
	logjam_throbber_4.png \
	logjam_throbber_5.png \
	logjam_throbber_6.png \
	logjam_throbber_7.png \
	logjam_throbber_8.png \
	logjam_lrarrow.png    \
	logjam_goat.png       \
	logjam_cfriends_on.png  \
	logjam_cfriends_off.png \
	logjam_cfriends_new.png"

LIST="\
	logjam_ljuser        $srcdir/logjam_ljuser.png       \
	logjam_ljcomm        $srcdir/logjam_ljcomm.png       \
	logjam_twuser        $srcdir/logjam_twuser.png       \
	logjam_private       $srcdir/logjam_private.png      \
	logjam_protected     $srcdir/logjam_protected.png    \
	logjam_pencil        $srcdir/logjam_pencil.png       \
	logjam_music         $srcdir/logjam_music.png        \
	logjam_mood          $srcdir/logjam_mood.png         \
	logjam_rarrow        $srcdir/logjam_rarrow.png       \
	logjam_larrow        $srcdir/logjam_larrow.png       \
	logjam_lrarrow       $srcdir/logjam_lrarrow.png      \
	logjam_goat          $srcdir/logjam_goat.png         \
	logjam_cfriends_on   $srcdir/logjam_cfriends_on.png  \
	logjam_cfriends_off  $srcdir/logjam_cfriends_off.png \
	logjam_cfriends_new  $srcdir/logjam_cfriends_new.png"

THROBBER_LIST="\
	logjam_throbber_1    $srcdir/logjam_throbber_1.png   \
	logjam_throbber_2    $srcdir/logjam_throbber_2.png   \
	logjam_throbber_3    $srcdir/logjam_throbber_3.png   \
	logjam_throbber_4    $srcdir/logjam_throbber_4.png   \
	logjam_throbber_5    $srcdir/logjam_throbber_5.png   \
	logjam_throbber_6    $srcdir/logjam_throbber_6.png   \
	logjam_throbber_7    $srcdir/logjam_throbber_7.png   \
	logjam_throbber_8    $srcdir/logjam_throbber_8.png"

echo "gdk-pixbuf-csource --raw --build-list $THROBBER_LIST $LIST >$srcdir/pixbufs.h"


