/* written by hand.  :( */

// pointer truncation
#pragma warning( disable : 4311 )
// pointer lengthening
#pragma warning( disable : 4312 )


#undef HAVE_TIMEGM
#define WIN32_FAKE_TIMEGM 1

#define HAVE_GTK 1

#define PACKAGE_VERSION "Win32"

#define HAVE_STRING_H 1
