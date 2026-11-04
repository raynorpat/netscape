# Source manifest translated from nspr/src/Makefile.
netscape_archive(netscape_nspr nspr/src nspr
  file.c
  getopt.c
  linker.c
  log2.c
  longlong.c
  page.c
  prabort.c
  prarena.c
  prdump.c
  prevent.c
  prhash.c
  prmcache.c
  prnetdb.c
  prtime.c
  prmjtime.c
  prfile.c
  prfileio.c
  prlog.c
  prnetio.c
  prdtoa.c
  prprf.c
  prsystem.c
  swintr.c
  swmon.c
  swsched.c
  swthread.c
  prgcapi.c
  prmsgc.c
  mdunix.c
  prmem.c
  prmalloc.c
  md_Linux.c
)
set_target_properties(netscape_nspr PROPERTIES PREFIX "")
