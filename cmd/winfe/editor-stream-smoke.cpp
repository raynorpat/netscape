/* Real buffered-write failure coverage for the original editor output stream. */
#include "../../lib/layout/editor.h"
#include <io.h>

bool WFE_EditorInvokeAutoSave(MWContext *context)
{
    CEditBuffer *buffer=LO_GetEDBuffer(context);
    if(!buffer || buffer->IsBlocked()) return false;
    buffer->AutoSaveCallback();return true;
}

bool WFE_EditorStreamSmoke(const char *path)
{
    XP_File file=fopen(path,"w+b");
    if(!file) return false;
    char buffer[8192];setvbuf(file,buffer,_IOFBF,sizeof(buffer));
    CStreamOutFile stream(file,TRUE);
    char text[]="Buffered Gold HTML";
    stream.Write(text,sizeof(text)-1);
    bool buffered=stream.Status()==IStreamOut::EOS_NoError;
    // Invalidate the OS handle while retaining the CRT stream's pending bytes.
    // This makes the final flush fail, after all Write calls reported success.
    HANDLE handle=(HANDLE)_get_osfhandle(_fileno(file));
    bool closed=CloseHandle(handle)!=FALSE;
    bool rejected=stream.Close()!=IStreamOut::EOS_NoError;
    bool stable=stream.Close()!=IStreamOut::EOS_NoError;
    DeleteFileA(path);
    return buffered&&closed&&rejected&&stable;
}
