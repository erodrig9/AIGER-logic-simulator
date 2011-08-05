
#ifndef aiger_cc_INCLUDED
#define aiger_cc_INCLUDED

#include "aiger_cc.h"

extern "C" {
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <unistd.h>
}

static void die (const char *fmt, ...);

#define USAGE \
"usage: sim [-h][-v][-c #cycles] src dst [in] \n" \
"\n" \
"This is a utility to ...\n" \
"\n" \
"  -h     print this command line option summary\n" \
"  -v     verbose\n" \
"  -c     # simulation cycles (default is 10,000)\n" \
"  src    aiger file\n" \
"  dst    output file\n" \
"  in    intpu trace file\n" \
"\n"

aiger* read_aiger (int argc, char *argv[]);

#endif
