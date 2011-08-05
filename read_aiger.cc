#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

#include "read_aiger.h"

static void die (const char *fmt, ...)
{
  va_list ap;
  fputs ("*** [aigtoaig] ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

aiger* read_aiger (int argc, char *argv[])
{
  const char *src, *dst, *src_name, *dst_name, *error;
  aiger *aiger;
  memory memory;
  unsigned i;

  src_name = dst_name = src = dst = 0;

  for (i = 1; i < argc; i++)
  {
    if (!strcmp (argv[i], "-h"))
	  {
	    fprintf (stderr, USAGE);
	    exit (0);
	  }
    else if (argv[i][0] == '-' && argv[i][1])
	    die ("invalid command line option '%s'", argv[i]);
    else if (!src_name)
      src = src_name = argv[i];
    else if (!dst_name)
      dst = dst_name = argv[i];
    else
	    die ("more than two files specified");
  }

  if (src && dst && !strcmp (src, dst))
    die ("identical 'src' and 'dst' file");

  memory.max = memory.bytes = 0;
  aiger = aiger_init_mem (&memory);
  if (src)
  {
    error = aiger_open_and_read_from_file (aiger, src);
    if (error)
	  { 
	    READ_ERROR:
	    fprintf (stderr, "*** [aigtoaig] %s\n", error);
	    exit(1);
	  }
  }
  else
  {
    fprintf (stderr, USAGE);
    exit (0);
  }

  return aiger;
}
