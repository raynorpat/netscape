# Win32 native threads: source list from nspr/src/makefile.win, OS_RELEASE=32.
netscape_archive(netscape_nspr nspr/src nspr
 file.c getopt.c linker.c log2.c longlong.c page.c prabort.c prarena.c
 prdump.c prevent.c prhash.c prmcache.c prtime.c prmjtime.c prfile.c
 prfileio.c prnetdb.c prlog.c prprf.c prsystem.c prdtoa.c
 md_WIN.c md_WIN32.c os_WIN.c prwindows.c prgcapi.c prmsgc.c
 hwthread.c hwsched.c hwmon.c)
target_compile_definitions(netscape_nspr PRIVATE _NSPR)
set_target_properties(netscape_nspr PROPERTIES OUTPUT_NAME nspr)
