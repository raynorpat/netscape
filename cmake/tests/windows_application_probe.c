/* Controlled external application: record actual CRT argv and source bytes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
static int probe_main(int argc,char **argv)
{
    const char *report=getenv("NETSCAPE_APP_PROBE_REPORT"),*copy=getenv("NETSCAPE_APP_PROBE_COPY");
    FILE *file=report?fopen(report,"wb"):NULL;int i;
    if(!file) return 2;
    fprintf(file,"argc=%d\n",argc);
    for(i=0;i<argc;++i) fprintf(file,"arg%d=%s\n",i,argv[i]);
    if(fclose(file)) return 3;
    if(copy && *copy && argc>1) {
        FILE *input=fopen(argv[1],"rb"),*output=NULL;
        if(input) {
            unsigned char buffer[4096];size_t length;
            output=fopen(copy,"wb");if(!output) {fclose(input);return 4;}
            while((length=fread(buffer,1,sizeof(buffer),input))!=0)
                if(fwrite(buffer,1,length,output)!=length) {fclose(input);fclose(output);return 5;}
            if(ferror(input)) {fclose(input);fclose(output);return 6;}
            fclose(input);if(fclose(output)) return 7;
        }
    }
    file=fopen(report,"ab");if(!file) return 8;
    fputs("done=1\n",file);if(fclose(file)) return 9;
    return 0;
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show)
{return probe_main(__argc,__argv);}
