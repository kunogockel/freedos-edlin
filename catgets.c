/* catgets.c - Kitten-like message catalog functions

   AUTHOR: Gregory Pietsch

   DESCRIPTION:

   This file, combined with nl_catd.h, provides a kitten-like version of
   the catgets() functions.  It's different from kitten in that it uses
   the dynamic string functions in the background and you could have more
   than one catalog open at a time.
*/

#include "config.h"
#include "nl_types.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include "defines.h"
#include "dynstr.h"

typedef struct
{
  int set_id, msg_id;
  STRING_T *msg;
} _CAT_MESSAGE_T;

/* functions */

#ifndef __STDC__
#ifndef HAVE_STRCHR
/* no strchr(), so roll our own */
#ifndef OPTIMIZED_FOR_SIZE
/* Nonzero if X is not aligned on an "unsigned long" boundary.  */
#ifdef ALIGN
#define UNALIGNED(X) ((unsigned long)X&(sizeof(unsigned long)-1))
#else
#define UNALIGNED(X) 0
#endif
/* Null character detection.  */
#if ULONG_MAX == 0xFFFFFFFFUL
#define DETECTNULL(X) (((X)-0x01010101UL)&~(X)&0x80808080UL)
#elif ULONG_MAX == 0xFFFFFFFFFFFFFFFFUL
#define DETECTNULL(X) (((X)-0x0101010101010101UL)&~(X)&0x8080808080808080UL)
#else
#error unsigned long is not 32 or 64 bits wide
#endif
#if UCHAR_MAX != 0xFF
#error char is not 8 bits wide
#endif
/* Detect whether the character used to fill mask is in X */
#define DETECTCHAR(X,MASK) (DETECTNULL(X^MASK))
#endif /* OPTIMIZED_FOR_SIZE */
static char *
strchr (const char *s, int c)
{
  char pc = (char) c;
#ifndef OPTIMIZED_FOR_SIZE
  const unsigned long *ps;
  unsigned long mask = 0;
  size_t i;

  /* Special case for finding \0. */
  if (c == '\0')
    {
      while (UNALIGNED (s))
	{
	  if (*s == '\0')
	    return (char *) s;
	  ++s;
	}
      /* Operate a word at a time.  */
      ps = (const unsigned long *) s;
      while (!DETECTNULL (*ps))
	++ps;
      /* Found the end of the string.  */
      s = (const char *) ps;
      while (*s != '\0')
	++s;
      return (char *) s;
    }
  /* All other bytes.  Align the pointer, then search a long at a time.  */
  while (UNALIGNED (s))
    {
      if (*s == '\0' || *s == pc)
	return *s ? (char *) s : 0;
      ++s;
    }
  ps = (const unsigned long *) s;
  for (i = 0; i < sizeof (unsigned long); ++i)
    mask = ((mask << CHAR_BIT) + ((unsigned char) pc & ~(~0 << CHAR_BIT)));
  /* Move ps a block at a time.  */
  while (!DETECTNULL (*ps) && !DETECTCHAR (*ps, mask))
    ++ps;
  /* Pick up any residual with a byte searcher.  */
  s = (const char *) ps;
#endif /* OPTIMIZED_FOR_SIZE */
  /* The normal byte-search loop.  */
  while (*s && *s != pc)
    ++s;
  return *s == pc ? (char *) s : 0;
}

#ifndef OPTIMIZED_FOR_SIZE
#undef ALIGNED
#undef DETECTNULL
#undef DETECTCHAR
#endif /* OPTIMIZED_FOR_SIZE */
#endif /* HAVE_STRCHR */

#ifndef HAVE_STRPBRK
static char *
strpbrk (const char *s1, const char *s2)
{
  const char *sc1;

  for (sc1 = s1; *sc1 != '\0'; ++sc1)
    if (strchr (s2, *sc1) != 0)
      return (char *) sc1;
  return 0;			/* terminating nulls match */
}
#endif /* HAVE_STRPBRK */

#ifndef HAVE_STRRCHR
static char *
strrchr (const char *s, int c)
{
  const char *last = NULL;
  /* If the character we're looking for is the terminating null,
     we just need to look for that character as there's only one
     of them in the string.  */
  if (c == '\0')
    return strchr (s, c);
  /* Loop through, finding the last match before hitting NULL.  */
  while ((s = strchr (s, c)) != NULL)
    {
      last = s;
      s++;
    }
  return (char *) last;
}
#endif /* HAVE_STRRCHR */

#endif /* __STDC__ */

static void
_CAT_MESSAGE_ctor (_CAT_MESSAGE_T * x)
{
  x->set_id = x->msg_id = 0;
  x->msg = DScreate ();
}

static void
_CAT_MESSAGE_dtor (_CAT_MESSAGE_T * x)
{
  x->set_id = x->msg_id = 0;
  DSdestroy (x->msg);
  x->msg = 0;
}

static _CAT_MESSAGE_T *
_CAT_MESSAGE_assign (_CAT_MESSAGE_T * x, _CAT_MESSAGE_T * y)
{
  if (x != y)
    {
      x->set_id = y->set_id;
      x->msg_id = y->msg_id;
      DSassign (x->msg, y->msg, 0, NPOS);
    }
  return x;
}

#define T               _CAT_MESSAGE_T
#define TS              _CATMSG
#define Tassign         _CAT_MESSAGE_assign
#define Tctor           _CAT_MESSAGE_ctor
#define Tdtor           _CAT_MESSAGE_dtor
#define Tstorage_class  static
#define PROTOS_ONLY
#include "dynarray.h"
#undef  PROTOS_ONLY
#include "dynarray.h"
#undef  Tstorage_class
#undef  Tdtor
#undef  Tctor
#undef  Tassign
#undef  TS
#undef  T

typedef struct
{
  int is_opened;
  _CATMSG_ARRAY_T *msgs;
} _CAT_CATALOG_T;

static void
_CAT_catalog_ctor (_CAT_CATALOG_T * x)
{
  x->is_opened = 0;
  x->msgs = _CATMSG_create ();
}

static void
_CAT_catalog_dtor (_CAT_CATALOG_T * x)
{
  x->is_opened = 0;
  _CATMSG_destroy (x->msgs);
  x->msgs = 0;
}

static _CAT_CATALOG_T *
_CAT_catalog_assign (_CAT_CATALOG_T * x, _CAT_CATALOG_T * y)
{
  if (x != y)
    {
      x->is_opened = y->is_opened;
      _CATMSG_assign (x->msgs, _CATMSG_base (y->msgs),
		      _CATMSG_length (y->msgs), 1);
    }
  return x;
}

#define T               _CAT_CATALOG_T
#define TS              _CATCAT
#define Tassign         _CAT_catalog_assign
#define Tctor           _CAT_catalog_ctor
#define Tdtor           _CAT_catalog_dtor
#define Tstorage_class  static
#define PROTOS_ONLY
#include "dynarray.h"
#undef  PROTOS_ONLY
#include "dynarray.h"
#undef  Tstorage_class
#undef  Tdtor
#undef  Tctor
#undef  Tassign
#undef  TS
#undef  T

static _CATCAT_ARRAY_T *theCatalogues = 0;

/* static functions used by catopen */
enum
{
  FSM_ANY = UCHAR_MAX + 1,
  FSM_DIGIT,
  FSM_ODIGIT,
  FSM_XDIGIT,
  FSM_OUTPUT,
  FSM_SUPPRESS,
  FSM_BASE8,
  FSM_BASE8_OUTPUT,
  FSM_BASE10,
  FSM_BASE10_OUTPUT,
  FSM_BASE16,
  FSM_BASE16_OUTPUT,
  FSM_RETAIN
};

static void
transform_string (STRING_T * t, STRING_T * s)
{
  int c, state = 0, accum = 0;
  size_t ip = 0, i;
  static int fsm[] = {
    0, '\\', FSM_SUPPRESS, 1,
    0, FSM_ANY, FSM_OUTPUT, 0,
    1, 'b', '\b', 0,
    1, 'e', '\033', 0,
    1, 'f', '\f', 0,
    1, 'n', '\n', 0,
    1, 'r', '\r', 0,
    1, 't', '\t', 0,
    1, 'v', '\v', 0,
    1, '\\', '\\', 0,
    1, 'd', FSM_SUPPRESS, 2,
    1, FSM_ODIGIT, FSM_BASE8, 3,
    1, 'x', FSM_SUPPRESS, 4,
    1, FSM_ANY, FSM_OUTPUT, 0,
    2, FSM_DIGIT, FSM_BASE10, 21,
    2, FSM_ANY, FSM_RETAIN, 0,
    3, FSM_ODIGIT, FSM_BASE8, 31,
    3, FSM_ANY, FSM_RETAIN, 0,
    4, FSM_XDIGIT, FSM_BASE16, 4,
    4, FSM_ANY, FSM_RETAIN, 0,
    21, FSM_DIGIT, FSM_BASE10, 22,
    21, FSM_ANY, FSM_RETAIN, 0,
    22, FSM_DIGIT, FSM_BASE10_OUTPUT, 0,
    22, FSM_ANY, FSM_RETAIN, 0,
    31, FSM_ODIGIT, FSM_BASE8_OUTPUT, 0,
    31, FSM_ANY, FSM_RETAIN, 0,
    -1, -1, -1, -1
  };
  static char hexits[] =
    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
    'e', 'f', '\0'
  };

  DSresize (t, 0, 0);
  while (ip < DSlength (s) && (c = DSget_at (s, ip)) != '\0')
    {
      for (i = 0;
	   !(fsm[i] == state
	     && (fsm[i + 1] == c
		 || fsm[i + 1] == FSM_ANY
		 || (fsm[i + 1] == FSM_DIGIT && isdigit (c))
		 || (fsm[i + 1] == FSM_ODIGIT && strchr ("01234567", c) != 0)
		 || (fsm[i + 1] == FSM_XDIGIT && isxdigit (c)))); i += 4);
      switch (fsm[i + 2])
	{
	case FSM_OUTPUT:
	  DSappendchar (t, c, 1);
	  ip++;
	  break;
	case FSM_SUPPRESS:
	  accum = 0;
	  ip++;
	  break;
	case FSM_BASE10:
	  accum = (accum * 10) + (c - '0');
	  ip++;
	  break;
	case FSM_BASE8:
	  accum = (accum << 3) + (c - '0');
	  ip++;
	  break;
	case FSM_BASE16:
	  accum = (accum << 4) + (strchr (hexits, tolower (c)) - hexits);
	  ip++;
	  break;
	case FSM_BASE10_OUTPUT:
	  accum = (accum * 10) + (c - '0');
	  DSappendchar (t, accum, 1);
	  accum = 0;
	  ip++;
	  break;
	case FSM_BASE8_OUTPUT:
	  accum = (accum << 3) + (c - '0');
	  DSappendchar (t, accum, 1);
	  accum = 0;
	  ip++;
	  break;
	case FSM_RETAIN:
	  DSappendchar (t, accum, 1);
	  accum = 0;
	  break;
	default:
	  DSappendchar (t, fsm[i + 2], 1);
	  ip++;
	  break;
	}
      state = fsm[i + 3];
    }
}

static int
catmsg_cmp (const void *a, const void *b)
{
  const _CAT_MESSAGE_T pa = *(_CAT_MESSAGE_T *) a;
  const _CAT_MESSAGE_T pb = *(_CAT_MESSAGE_T *) b;

  if (pa.set_id != pb.set_id)
    return pa.set_id < pb.set_id ? -1 : +1;
  if (pa.msg_id != pb.msg_id)
    return pa.msg_id < pb.msg_id ? -1 : +1;
  return 0;
}

/* read a line from a file */
static STRING_T *
append_from_file (FILE * f, STRING_T * s)
{
  char buffer[BUFSIZ];

  if (s == 0)
    s = DScreate ();
  while (fgets (buffer, BUFSIZ, f) != 0)
    {
      DSappendcstr (s, buffer, NPOS);
      if (DSget_at (s, DSlength (s) - 1) == '\n')
	break;
    }
  return s;
}

/* catread - read a catalogue file */
static _CAT_CATALOG_T *
catread (const char *name)
{
  FILE *f;
  STRING_T *s = DScreate (), *t = DScreate ();
  _CAT_CATALOG_T *cat = 0;
  size_t z;
  _CAT_MESSAGE_T catmsg = { 0, 0, 0 };
  int c;

  /* Open the catfile */
  f = fopen (name, "r");
  if (f == 0)
    return 0;			/* could not open file */
  setvbuf (f, 0, _IOFBF, 16384);
  while (DSlength (s = append_from_file (f, s)) > 0)
    {
      DSresize (s, DSlength (s) - 1, 0);
      /* We have a full line */
      if (DSlength (s) > 0)
	{
	  z = DSfind_first_not_of (s, " \t\f\v\r", 0, NPOS);
	  DSremove (s, 0, z);
	  z = DSfind_last_not_of (s, " \t\f\v\r", NPOS, NPOS);
	  DSresize (s, z + 1, 0);
	}
      if (DSlength (s) > 0 && DSget_at (s, DSlength (s) - 1) == '\\')
	{
	  /* continuation */
	  DSresize (s, DSlength (s) - 1, 0);
	}
      else
	{
	  if (DSlength (s) > 0 && isdigit (DSget_at (s, 0)))
	    {
	      /* if it starts with a digit, assume it's a catalog line */
	      for (z = 0, catmsg.set_id = 0;
		   isdigit (c = DSget_at (s, z));
		   catmsg.set_id = catmsg.set_id * 10 + (c - '0'), z++);
	      z++;
	      for (catmsg.msg_id = 0;
		   isdigit (c = DSget_at (s, z));
		   catmsg.msg_id = catmsg.msg_id * 10 + (c - '0'), z++);
	      z++;
	      DSremove (s, 0, z);
	      transform_string (t, s);
	      if (catmsg.msg == 0)
		catmsg.msg = DScreate ();
	      DSassign (catmsg.msg, t, 0, NPOS);
	      if (cat == 0)
		{
		  cat = malloc (sizeof (_CAT_CATALOG_T));
		  if (cat == 0)
		    Nomemory ();
		  cat->is_opened = 0;
		  cat->msgs = _CATMSG_create ();
		}
	      _CATMSG_append (cat->msgs, &catmsg, 1, 0);
	    }
	  DSresize (s, 0, 0);
	}
    }
  fclose (f);
  qsort (_CATMSG_base (cat->msgs), _CATMSG_length (cat->msgs),
	 sizeof (_CAT_MESSAGE_T), catmsg_cmp);
  return cat;
}

/* install_catalog - install a _CAT_CATALOG_T, return where */
static nl_catd
install_catalog (_CAT_CATALOG_T * cat)
{
  size_t i;

  cat->is_opened = 1;
  if (theCatalogues == 0)
    {
      theCatalogues = _CATCAT_create ();
      _CATCAT_append (theCatalogues, cat, 1, 0);
      return (nl_catd) 0;
    }
  else
    {
      for (i = 0;
	   i < _CATCAT_length (theCatalogues)
	   && _CATCAT_get_at (theCatalogues, i)->is_opened; ++i);
      _CATCAT_put_at (theCatalogues, i, cat);
      return (nl_catd) i;
    }
}

/*
  NAME

    catopen - open a message catalog

  SYNOPSIS

    #include <nl_types.h>

    nl_catd catopen(const char *name, int oflag);

  DESCRIPTION

    The catopen() function shall open a message catalog and return a message
    catalog descriptor. The name argument specifies the name of the message
    catalog to be opened. If name contains a '/' , then name specifies a
    complete name for the message catalog. Otherwise, the environment variable
    NLSPATH is used with name substituted for the %N conversion specification
    (see the Base Definitions volume of IEEE Std 1003.1-2001, Chapter 8,
    Environment Variables). If NLSPATH exists in the environment when the
    process starts, then if the process has appropriate privileges, the
    behavior of catopen() is undefined. If NLSPATH does not exist in the
    environment, or if a message catalog cannot be found in any of the
    components specified by NLSPATH, then an implementation-defined default
    path shall be used. This default may be affected by the setting of
    LC_MESSAGES if the value of oflag is NL_CAT_LOCALE, or the LANG
    environment variable if oflag is 0.

    A message catalog descriptor shall remain valid in a process until that
    process closes it, or a successful call to one of the exec functions. A
    change in the setting of the LC_MESSAGES category may invalidate
    existing open catalogs.

    If a file descriptor is used to implement message catalog descriptors, the
    FD_CLOEXEC flag shall be set; see <fcntl.h>.

    If the value of the oflag argument is 0, the LANG environment variable is
    used to locate the catalog without regard to the LC_MESSAGES category. If
    the oflag argument is NL_CAT_LOCALE, the LC_MESSAGES category is used to
    locate the message catalog (see the Base Definitions volume of IEEE Std
    1003.1-2001, Section 8.2, Internationalization Variables).

  RETURN VALUE

    Upon successful completion, catopen() shall return a message catalog
    descriptor for use on subsequent calls to catgets() and catclose().
    Otherwise, catopen() shall return (nl_catd)(-1) and set errno to indicate
    the error.

  ERRORS

    The catopen() function may fail if:

    [EACCES]
        Search permission is denied for the component of the path prefix of
        the message catalog or read permission is denied for the message
        catalog.
    [EMFILE]
        {OPEN_MAX} file descriptors are currently open in the calling process.
    [ENAMETOOLONG]
        The length of a pathname of the message catalog exceeds {PATH_MAX} or
        a pathname component is longer than {NAME_MAX}.
    [ENAMETOOLONG]
        Pathname resolution of a symbolic link produced an intermediate result
        whose length exceeds {PATH_MAX}.
    [ENFILE]
        Too many files are currently open in the system.
    [ENOENT]
        The message catalog does not exist or the name argument points to an
        empty string.
    [ENOMEM]
        Insufficient storage space is available.
    [ENOTDIR]
        A component of the path prefix of the message catalog is not a 
        directory.

*/

#define NLS_DEFAULT_PATH \
"/usr/share/nls/%L/%N.cat;/usr/share/nls/%l.%c/%N.cat;/usr/share/nls/%l/%N.cat"
#define NLS_DEFAULT_LANG "C"

nl_catd
catopen (const char *name, int oflag)
{
  STRING_T *tmppath = 0;
  char *nlspath = 0;
  char *lang = 0;
  char *s, *t, *sep, *dot;
  const char *u;
  nl_catd catd;
  _CAT_CATALOG_T *cat = 0;

  if (name == 0 || *name == 0)
    return (nl_catd) (-1);
  /* absolute or relative path? */
  if (strpbrk (name, "\\/:") != 0)
    {
      cat = catread (name);
      return cat ? install_catalog (cat) : (nl_catd) (-1);
    }
  if ((nlspath = getenv ("NLSPATH")) == 0)
    nlspath = NLS_DEFAULT_PATH;
  lang = 0;
  if (oflag & NL_CAT_LOCALE)
    {
      lang = getenv ("LC_ALL");
      if (lang == 0)
	lang = getenv ("LC_MESSAGES");
    }
  if (lang == 0)
    lang = getenv ("LANG");
  if (lang == 0 || strcmp (lang, "POSIX") == 0)
    lang = NLS_DEFAULT_LANG;
  s = nlspath;
  tmppath = DScreate ();
  /* Locale names are of the form language[_territory][.codeset]. */
  sep = strchr (lang, '_');
  dot = strrchr (lang, '.');
  if (dot && sep && dot < sep)
    dot = 0;			/* ignore dot before sep */
  if (dot == 0)
    lang = NLS_DEFAULT_LANG;	/* no codeset specified */
  do
    {
      while (*s && *s != ';')
	{
	  if (*s == '%')
	    {
	      switch (*(++s))
		{
		case 'L':	/* LANG or LC_MESSAGES */
		  DSappendcstr (tmppath, lang, NPOS);
		  break;
		case 'N':	/* name parameter */
		  DSappendcstr (tmppath, name, NPOS);
		  break;
		case 'l':	/* language part */
		  u = lang;
		  while (*u)
		    {
		      DSappendchar (tmppath, *u, 1);
		      ++u;
		      if (sep && u >= sep)
			break;
		      if (dot && u >= dot)
			break;
		    }
		  break;
		case 't':	/* territory part */
		  if (sep == 0)
		    break;
		  u = sep + 1;
		  while (*u)
		    {
		      DSappendchar (tmppath, *u, 1);
		      ++u;
		      if (dot && u >= dot)
			break;
		    }
		  break;
		case 'c':	/* codeset part */
		  if (dot == 0)
		    break;
		  DSappendcstr (tmppath, dot + 1, NPOS);
		  break;
		default:
		  DSappendchar (tmppath, *s, 1);
		  break;
		}
	    }
	  else
	    DSappendchar (tmppath, *s, 1);
	  ++s;
	}
      cat = catread (DScstr (tmppath));
      if (cat)
	{
	  DSdestroy (tmppath);
	  return install_catalog (cat);
	}
      if (*s)
	++s;
      DSremove (tmppath, 0, NPOS);
    }
  while (*s);
  DSdestroy (tmppath);
  return (nl_catd) (-1);
}

/*
  NAME

    catgets - read a program message

  SYNOPSIS

    #include <nl_types.h>

    char *catgets(nl_catd catd, int set_id, int msg_id, const char *s);

  DESCRIPTION

    The catgets() function shall attempt to read message msg_id, in set
    set_id, from the message catalog identified by catd. The catd argument is
    a message catalog descriptor returned from an earlier call to catopen().
    The s argument points to a default message string which shall be returned
    by catgets() if it cannot retrieve the identified message.

    The catgets() function need not be reentrant. A function that is not
    required to be reentrant is not required to be thread-safe.

  RETURN VALUE

    If the identified message is retrieved successfully, catgets() shall
    return a pointer to an internal buffer area containing the null-terminated
    message string. If the call is unsuccessful for any reason, s shall be
    returned and errno may be set to indicate the error.

  ERRORS

    The catgets() function may fail if:

    [EBADF]
        The catd argument is not a valid message catalog descriptor open for
        reading.
    [EBADMSG]
        The message identified by set_id and msg_id in the specified message
        catalog did not satisfy implementation-defined security criteria.
    [EINTR]
        The read operation was terminated due to the receipt of a signal, and
        no data was transferred.
    [EINVAL]
        The message catalog identified by catd is corrupted.
    [ENOMSG]
        The message identified by set_id and msg_id is not in the message 
        catalog.

*/
char *(catgets) (nl_catd catd, int set_id, int msg_id, const char *s)
{
  _CAT_CATALOG_T * cat;
  static _CAT_MESSAGE_T keymsg = { 0, 0, 0 };
  _CAT_MESSAGE_T *msgptr = 0;

  if (theCatalogues == 0 || _CATCAT_length (theCatalogues) <= catd
      || (cat = _CATCAT_get_at (theCatalogues, catd))->is_opened == 0)
    {
      errno = EBADF;
      return (char *) s;
    }
  keymsg.set_id = set_id;
  keymsg.msg_id = msg_id;
  msgptr =
    bsearch (&keymsg, _CATMSG_base (cat->msgs), _CATMSG_length (cat->msgs),
	     sizeof (_CAT_MESSAGE_T), catmsg_cmp);
  if (msgptr == 0)
    {
#ifdef ENOMSG
      errno = ENOMSG;
#endif
      return (char *) s;
    }
  return DScstr (msgptr->msg);
}

/*
  NAME

    catclose - close a message catalog descriptor

  SYNOPSIS

    #include <nl_types.h>

    int catclose(nl_catd catd);

  DESCRIPTION

    The catclose() function shall close the message catalog identified by
    catd. If a file descriptor is used to implement the type nl_catd, that
    file descriptor shall be closed.

  RETURN VALUE

    Upon successful completion, catclose() shall return 0; otherwise, -1
    shall be returned, and errno set to indicate the error.

  ERRORS

    The catclose() function may fail if:

    [EBADF]
        The catalog descriptor is not valid.
    [EINTR]
        The catclose() function was interrupted by a signal.
*/
int (catclose) (nl_catd catd)
{
  _CAT_CATALOG_T *cat;

  if (theCatalogues == 0 || _CATCAT_length (theCatalogues) <= catd
      || (cat = _CATCAT_get_at (theCatalogues, catd))->is_opened == 0)
    {
      errno = EBADF;
      return -1;
    }
  cat->is_opened = 0;
  _CATMSG_resize (cat->msgs, 0, 0);
  return 0;
}

/* END OF FILE */
