/* Standalone javap has no VM/Java console. Route verifier diagnostics to stderr. */
#include <stdio.h>
void PrintToConsole(const char *bytes)
{
    fputs(bytes, stderr);
}
