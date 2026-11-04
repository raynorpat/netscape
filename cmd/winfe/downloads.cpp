#include <afxwin.h>
#include <afxdlgs.h>
#include <afxcmn.h>
#include <shlobj.h>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "downloads.h"
extern "C" {
#include "shist.h"
#include "merrors.h"
#include "xp_file.h"
HWND WFE_DocumentWindow(MWContext *);
void WFE_CloseWindow(MWContext *);
extern int MK_ZERO_LENGTH_FILE;
}

class DownloadFrame;
struct DownloadJob {
    uint32 id = 0;
    MWContext context = {};
    bool initialized = false, cancel = false, stream_done = false;
    bool stream_aborted = false, expected_known = false, length_check = false;
    int state = WFE_DOWNLOAD_PENDING, abort_status = 0, http_status = 0;
    ULONGLONG bytes = 0, expected = 0;
    URL_Struct *url = nullptr;
    HANDLE file = INVALID_HANDLE_VALUE;
    DownloadFrame *frame = nullptr;
    std::string address, requested, destination, temporary, error;
};
static std::map<uint32, std::shared_ptr<DownloadJob>> download_jobs;
static uint32 next_download = 1;
static bool download_smoke = false, shutting_down = false;
static const UINT download_finished_message = WM_APP + 71;

static std::shared_ptr<DownloadJob> job_for(uint32 id)
{
    auto found = download_jobs.find(id);
    return found == download_jobs.end() ? std::shared_ptr<DownloadJob>() : found->second;
}

static std::shared_ptr<DownloadJob> context_job(MWContext *context)
{
    for (auto item : download_jobs) if (&item.second->context == context) return item.second;
    return {};
}

static bool running(const DownloadJob &job) { return job.state <= WFE_DOWNLOAD_ACTIVE; }

static void close_file(DownloadJob &job)
{
    if (job.file != INVALID_HANDLE_VALUE) {
        HANDLE file = job.file; job.file = INVALID_HANDLE_VALUE;
        if (!CloseHandle(file) && job.error.empty()) job.error = "Could not close download file.";
    }
}

static void remove_temporary(DownloadJob &job)
{
    close_file(job);
    if (!job.temporary.empty()) {
        if (DeleteFileA(job.temporary.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND ||
            GetLastError() == ERROR_PATH_NOT_FOUND) job.temporary.clear();
        else if (job.error.find(job.temporary) == std::string::npos) {
            if (!job.error.empty()) job.error += " ";
            job.error += "Could not remove incomplete file: " + job.temporary;
        }
    }
}

class DownloadFrame : public CFrameWnd {
public:
    uint32 id;
    CStatic address, filename, status;
    CProgressCtrl progress;
    CButton cancel;
    explicit DownloadFrame(uint32 value) : id(value) { }
    void Update();
    void PostNcDestroy() override { delete this; }
protected:
    afx_msg int OnCreate(LPCREATESTRUCT);
    afx_msg void OnTimer(UINT_PTR);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg LRESULT OnFinished(WPARAM, LPARAM);
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(DownloadFrame, CFrameWnd)
 ON_WM_CREATE()
 ON_WM_TIMER()
 ON_WM_CLOSE()
 ON_WM_DESTROY()
 ON_COMMAND(IDCANCEL, OnClose)
 ON_MESSAGE(download_finished_message, OnFinished)
END_MESSAGE_MAP()

int DownloadFrame::OnCreate(LPCREATESTRUCT create)
{
    if (CFrameWnd::OnCreate(create) == -1) return -1;
    if (!address.Create("", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS, CRect(12,12,550,34), this) ||
        !filename.Create("Choosing destination...", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS, CRect(12,40,550,62), this) ||
        !progress.Create(WS_CHILD | WS_VISIBLE, CRect(12,72,550,96), this, 5100) ||
        !status.Create("Waiting for response...", WS_CHILD | WS_VISIBLE, CRect(12,106,440,130), this) ||
        !cancel.Create("Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        CRect(458,104,550,132), this, IDCANCEL)) return -1;
    CFont *font = CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT));
    address.SetFont(font); filename.SetFont(font); status.SetFont(font); cancel.SetFont(font);
    progress.SetRange32(0, 1000);
    SetTimer(1, 20, nullptr);
    return 0;
}

void DownloadFrame::Update()
{
    auto job = job_for(id);
    if (!job || !GetSafeHwnd()) return;
    address.SetWindowText(job->address.c_str());
    if (!job->destination.empty()) filename.SetWindowText(job->destination.c_str());
    CString text;
    if (job->state == WFE_DOWNLOAD_COMPLETE) text.Format("Saved %I64u bytes.", job->bytes);
    else if (job->state == WFE_DOWNLOAD_CANCELLED) text = job->error.empty() ? "Download cancelled." : job->error.c_str();
    else if (job->state == WFE_DOWNLOAD_FAILED) text = job->error.c_str();
    else if (job->expected_known) text.Format("%I64u of %I64u bytes", job->bytes, job->expected);
    else text.Format("%I64u bytes received", job->bytes);
    status.SetWindowText(text);
    int percent = job->expected ? (int)((std::min)(job->bytes, job->expected) * 1000 / job->expected) : 0;
    if (job->state == WFE_DOWNLOAD_COMPLETE) percent = 1000;
    progress.SetPos(percent);
    cancel.SetWindowText(running(*job) ? "Cancel" : "Close");
}

void DownloadFrame::OnTimer(UINT_PTR timer)
{
    if (timer != 1) { CFrameWnd::OnTimer(timer); return; }
    Update();
    WFE_Pump();
}

void DownloadFrame::OnClose()
{
    auto job = job_for(id);
    if (job && running(*job)) { WFE_DownloadCancel(id); Update(); return; }
    DestroyWindow();
}

LRESULT DownloadFrame::OnFinished(WPARAM, LPARAM)
{
    auto job = job_for(id);
    if (job && job->initialized && !running(*job)) {
        /* URL exit runs inside netlib. Defer teardown until it unwinds. */
        job->initialized = false;
        WFE_DestroyContext(&job->context);
    }
    Update();
    return 0;
}

void DownloadFrame::OnDestroy()
{
    KillTimer(1);
    auto job = job_for(id);
    if (job) {
        job->frame = nullptr;
        if (job->initialized && !running(*job)) {
            job->initialized = false;
            WFE_DestroyContext(&job->context);
        }
        WFE_UnregisterNativeWindow(&job->context);
    }
    CFrameWnd::OnDestroy();
}

static std::shared_ptr<DownloadJob> new_job()
{
    if (shutting_down) return {};
    unsigned active = 0;
    for (auto item : download_jobs) if (running(*item.second)) ++active;
    if (active >= 32) return {};
    for (auto item = download_jobs.begin(); download_jobs.size() >= 128 && item != download_jobs.end();) {
        if (!running(*item->second) && !item->second->frame) item = download_jobs.erase(item);
        else ++item;
    }
    auto job = std::make_shared<DownloadJob>();
    job->id = next_download++;
    download_jobs[job->id] = job;
    DownloadFrame *frame = new DownloadFrame(job->id);
    WFE_InitContext(&job->context, frame);
    job->context.type = MWContextSaveToDisk;
    job->initialized = true;
    if (!frame->Create(nullptr, "Download - Netscape", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                        CRect(160,160,730,340))) {
        job->initialized = false;
        WFE_DestroyContext(&job->context);
        download_jobs.erase(job->id);
        return {};
    }
    job->frame = frame;
    WFE_RegisterNativeWindow(&job->context, frame->GetSafeHwnd());
    if (!download_smoke) frame->ShowWindow(SW_SHOWNOACTIVATE);
    return job;
}

static std::string response_header(URL_Struct *url, const char *name)
{
    if (!url) return "";
    /* Keep redirect/interim response metadata outside the current group. */
    uint32 first = (std::min)(url->all_headers.response_start, url->all_headers.empty_index);
    for (uint32 remaining = url->all_headers.empty_index; remaining > first; --remaining) {
        uint32 i = remaining - 1;
        if (url->all_headers.key && url->all_headers.value && url->all_headers.key[i] &&
            url->all_headers.value[i] && !_stricmp(url->all_headers.key[i], name))
            return url->all_headers.value[i];
    }
    return "";
}

static std::string suggested_name(URL_Struct *url)
{
    std::string name = url->content_name ? url->content_name : "";
    if (name.empty()) {
        char *path = NET_ParseURL(url->address, GET_PATH_PART);
        if (path) { name = path; XP_FREE(path); }
        size_t query = name.find_first_of("?#");
        if (query != std::string::npos) name.resize(query);
    }
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name.erase(0, slash + 1);
    for (char &byte : name) if ((unsigned char)byte < 32 || strchr("<>:\"/\\|?*", byte)) byte = '_';
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) name.pop_back();
    if (name.empty() || name == "." || name == "..") name = "download.bin";
    std::string stem = name.substr(0, name.find('.'));
    if (!_stricmp(stem.c_str(), "CON") || !_stricmp(stem.c_str(), "PRN") ||
        !_stricmp(stem.c_str(), "AUX") || !_stricmp(stem.c_str(), "NUL") ||
        (stem.size() == 4 && (!_strnicmp(stem.c_str(), "COM", 3) || !_strnicmp(stem.c_str(), "LPT", 3)) &&
         stem[3] >= '1' && stem[3] <= '9')) name.insert(0, "_");
    if (name.size() > 180) name.resize(180);
    return name;
}

static bool prepare_file(const std::shared_ptr<DownloadJob> &job, URL_Struct *url)
{
    std::string destination = job->requested;
    if (destination.empty()) {
        std::string name = suggested_name(url);
        CFileDialog dialog(FALSE, nullptr, name.c_str(), OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR,
            "All files|*.*||", job->frame);
        if (dialog.DoModal() != IDOK) { job->cancel = true; return false; }
        destination = dialog.GetPathName();
    }
    DWORD attributes = GetFileAttributesA(destination.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
        destination += "\\" + suggested_name(url);
    char absolute[MAX_PATH];
    DWORD length = GetFullPathNameA(destination.c_str(), sizeof(absolute), absolute, nullptr);
    if (!length || length >= sizeof(absolute)) { job->error = "Invalid download destination."; return false; }
    job->destination = absolute;
    std::string directory = job->destination.substr(0, job->destination.find_last_of("\\/"));
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        char suffix[100];
        sprintf(suffix, "\\.netscape-download-%lu-%lu-%u.part", (unsigned long)GetCurrentProcessId(),
                (unsigned long)job->id, attempt);
        job->temporary = directory + suffix;
        job->file = CreateFileA(job->temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (job->file != INVALID_HANDLE_VALUE) return true;
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    /* Never remove a colliding file that this job did not create. */
    job->temporary.clear();
    job->error = "Could not create temporary download file.";
    return false;
}

struct DownloadStream { std::shared_ptr<DownloadJob> job; };
static unsigned stream_ready(void *) { return 65536; }

static int stream_write(void *closure, const char *data, int32 length)
{
    auto job = ((DownloadStream *)closure)->job;
    if (job->cancel) return MK_INTERRUPTED;
    if (length < 0 || job->file == INVALID_HANDLE_VALUE) return MK_FILE_WRITE_ERROR;
    DWORD written = 0;
    if (!WriteFile(job->file, data, (DWORD)length, &written, nullptr) || written != (DWORD)length) {
        job->error = "Could not write download file.";
        return MK_FILE_WRITE_ERROR;
    }
    job->bytes += written;
    return 1;
}

static void stream_complete(void *closure)
{
    auto stream = (DownloadStream *)closure;
    auto job = stream->job;
    if (job->file != INVALID_HANDLE_VALUE && !FlushFileBuffers(job->file))
        job->error = "Could not flush download file.";
    close_file(*job);
    job->stream_done = true;
    delete stream;
}

static void stream_abort(void *closure, int status)
{
    auto stream = (DownloadStream *)closure;
    auto job = stream->job;
    close_file(*job);
    job->stream_aborted = true;
    job->abort_status = status;
    delete stream;
}

static void download_exit(URL_Struct *url, int status, MWContext *context)
{
    if (status == MK_CHANGING_CONTEXT) return;
    auto job = context_job(context);
    if (!job) { NET_FreeURLStruct(url); return; }
    job->url = nullptr;
    close_file(*job);
    bool empty_response = status == MK_ZERO_LENGTH_FILE && job->http_status >= 200 && job->http_status < 300 &&
        job->expected_known && job->expected == 0 && job->bytes == 0 && job->length_check &&
        job->stream_aborted && job->abort_status == MK_ZERO_LENGTH_FILE;
    bool good = ((status >= 0 && job->stream_done) || empty_response) && job->error.empty() &&
        !job->cancel && (!job->expected_known || !job->length_check || job->bytes == job->expected);
    if (good && !job->temporary.empty()) {
        good = MoveFileExA(job->temporary.c_str(), job->destination.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        if (good) job->temporary.clear();
        else job->error = "Could not replace the download destination.";
    } else good = false;
    if (job->cancel) job->state = WFE_DOWNLOAD_CANCELLED;
    else if (good) job->state = WFE_DOWNLOAD_COMPLETE;
    else {
        job->state = WFE_DOWNLOAD_FAILED;
        if (job->error.empty()) {
            if (job->expected_known && job->length_check && job->bytes != job->expected)
                job->error = "The download ended before all bytes were received.";
            else job->error = url->error_msg ? url->error_msg : "Download failed.";
        }
    }
    remove_temporary(*job);
    NET_FreeURLStruct(url);
    if (job->frame) job->frame->PostMessage(download_finished_message);
}

extern "C" NET_StreamClass *fe_MakeSaveAsStream(FO_Present_Types, void *, URL_Struct *url, MWContext *context)
{
    auto job = context_job(context);
    if (!job) {
        if (!NET_IsSafeForNewContext(url)) {
            WFE_Status(context, "This response cannot be moved to a separate download window.");
            return nullptr;
        }
        job = new_job();
        if (!job) return nullptr;
        job->address = url->address ? url->address : "";
        job->url = url;
        if (NET_SetNewContext(url, &job->context, download_exit) < 0) {
            job->url = nullptr;
            job->state = WFE_DOWNLOAD_FAILED;
            job->error = "Could not detach download from its browser window.";
            if (job->frame) job->frame->PostMessage(download_finished_message);
            return nullptr;
        }
    }
    job->http_status = url->server_status;
    if (job->http_status >= 400) { job->error = "The server returned an HTTP error."; return nullptr; }
    std::string encoding = response_header(url, "Content-Encoding");
    if (encoding.empty() && url->content_encoding) encoding = url->content_encoding;
    if (!encoding.empty() && _stricmp(encoding.c_str(), "identity") &&
        _stricmp(encoding.c_str(), "binary") && _stricmp(encoding.c_str(), "7bit") &&
        _stricmp(encoding.c_str(), "8bit")) {
        job->error = "This content encoding cannot be saved with byte-preserving validation.";
        return nullptr;
    }
    std::string length = response_header(url, "Content-Length");
    job->length_check = true;
    if (!length.empty()) {
        ULONGLONG expected = 0; bool valid = true;
        for (char digit : length) {
            if (digit < '0' || digit > '9' || expected > 0x7fffffffffffffffULL / 10) { valid = false; break; }
            expected = expected * 10 + digit - '0';
        }
        if (!valid) { job->error = "The server returned an invalid download length."; return nullptr; }
        job->expected = expected; job->expected_known = true;
    } else if (url->content_length > 0) {
        job->expected = url->content_length;
        job->expected_known = true;
    }
    if (!response_header(url, "Transfer-Encoding").empty()) {
        job->error = "This transfer encoding is not supported by the original download engine.";
        return nullptr;
    }
    if (!prepare_file(job, url)) return nullptr;
    DownloadStream *data = new DownloadStream{job};
    NET_StreamClass *stream = NET_NewStream((char *)"Native save to disk", stream_write,
        stream_complete, stream_abort, stream_ready, data, &job->context);
    if (!stream) { delete data; job->error = "Could not allocate download stream."; return nullptr; }
    job->state = WFE_DOWNLOAD_ACTIVE;
    if (job->frame) job->frame->Update();
    return stream;
}

static uint32 start_download(URL_Struct *url, const char *destination, FO_Present_Types format = FO_SAVE_AS)
{
    if (!url) return 0;
    auto job = new_job();
    if (!job) { NET_FreeURLStruct(url); return 0; }
    job->address = url->address ? url->address : "";
    job->requested = destination ? destination : "";
    job->url = url;
    url->load_background = TRUE;
    if (job->frame) job->frame->Update();
    /* NET_GetURL may invoke the exit callback before returning. */
    uint32 id = job->id;
    NET_GetURL(url, format, &job->context, download_exit);
    return id;
}

extern "C" void WFE_DownloadInit(void)
{
    shutting_down = false;
    NET_RegisterContentTypeConverter((char *)"*", FO_SAVE_AS, nullptr, fe_MakeSaveAsStream);
    NET_RegisterContentTypeConverter((char *)"*", FO_PRESENT, nullptr, fe_MakeSaveAsStream);
}

extern "C" uint32 WFE_SavePage(MWContext *source, const char *destination)
{
    if (!source) return 0;
    History_entry *history = SHIST_GetCurrent(&source->hist);
    if (!history) { WFE_Status(source, "There is no page to save."); return 0; }
    if (history->method != URL_GET_METHOD) {
        WFE_Status(source, "Saving this page would resend a form. Save the response from cache instead.");
        return 0;
    }
    URL_Struct *url = SHIST_CreateWysiwygURLStruct(source, history);
    if (url) { memset(&url->savedData, 0, sizeof(url->savedData)); url->history_num = 0; }
    return start_download(url, destination);
}

extern "C" uint32 WFE_SaveLink(MWContext *, const char *address, const char *destination)
{
    if (!address || !*address) return 0;
    return start_download(NET_CreateURLStruct(address, NET_DONT_RELOAD), destination);
}

extern "C" int WFE_DownloadCancel(uint32 id)
{
    auto job = job_for(id);
    if (!job || !running(*job)) return 0;
    job->cancel = true;
    if (job->url && NET_InterruptStream(job->url) < 0) NET_InterruptWindow(&job->context);
    return 1;
}

extern "C" int WFE_GetDownloadSnapshot(uint32 id, WFE_DownloadSnapshot *snapshot)
{
    auto job = job_for(id);
    if (!job || !snapshot) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->id = id; snapshot->state = job->state; snapshot->bytes = job->bytes;
    strncpy(snapshot->destination, job->destination.c_str(), sizeof(snapshot->destination) - 1);
    strncpy(snapshot->error, job->error.c_str(), sizeof(snapshot->error) - 1);
    return 1;
}

extern "C" void WFE_DownloadProgress(MWContext *context, URL_Struct *, int32, int32)
{
    auto job = context_job(context);
    if (job && job->frame) job->frame->Update();
}

extern "C" void WFE_DownloadShutdown(void)
{
    shutting_down = true;
    std::vector<uint32> ids;
    for (auto item : download_jobs) ids.push_back(item.first);
    for (uint32 id : ids) {
        auto job = job_for(id);
        if (running(*job)) WFE_DownloadCancel(id);
        if (job->frame) job->frame->DestroyWindow();
        if (!running(*job)) remove_temporary(*job);
    }
}

extern "C" int WFE_DownloadPromptForFileName(MWContext *context, const char *prompt, const char *path,
    XP_Bool must_exist, XP_Bool directories, ReadFileNameCallbackFunction callback, void *closure)
{
    if (!callback) return -1;
    HWND owner = context ? WFE_DocumentWindow(context) : nullptr;
    if (directories) {
        HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        BROWSEINFOA options = {};
        options.hwndOwner = owner;
        options.lpszTitle = prompt;
        options.ulFlags = BIF_RETURNONLYFSDIRS | (SUCCEEDED(apartment) ? BIF_USENEWUI : 0);
        LPITEMIDLIST selected = SHBrowseForFolderA(&options);
        if (!selected) { if (SUCCEEDED(apartment)) CoUninitialize(); callback(context, nullptr, closure); return 0; }
        char chosen[MAX_PATH];
        bool valid = SHGetPathFromIDListA(selected, chosen) != FALSE;
        CoTaskMemFree(selected);
        if (SUCCEEDED(apartment)) CoUninitialize();
        if (!valid) return -1;
        char *answer = XP_STRDUP(chosen);
        if (!answer) return -1;
        callback(context, answer, closure);
        return 0;
    }
    char *native = path ? WH_FileName(path, xpURL) : nullptr;
    CFileDialog dialog(must_exist, nullptr, native, OFN_HIDEREADONLY | OFN_NOCHANGEDIR |
        (must_exist ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT), "All files|*.*||", CWnd::FromHandle(owner));
    dialog.m_ofn.lpstrTitle = prompt;
    INT_PTR result = dialog.DoModal();
    if (native) XP_FREE(native);
    if (result != IDOK && CommDlgExtendedError()) return -1;
    char *answer = result == IDOK ? XP_STRDUP((const char *)dialog.GetPathName()) : nullptr;
    if (result == IDOK && !answer) return -1;
    callback(context, answer, closure);
    return 0;
}

static void download_pump()
{
    WFE_Pump();
    MSG message;
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) { PostQuitMessage((int)message.wParam); break; }
        if (!AfxGetApp()->PreTranslateMessage(&message)) { TranslateMessage(&message); DispatchMessage(&message); }
    }
    Sleep(1);
}

static bool wait_download(uint32 id)
{
    DWORD started = GetTickCount();
    auto job = job_for(id);
    while (job && running(*job) && GetTickCount() - started < 15000) download_pump();
    if (job && running(*job)) { WFE_DownloadCancel(id); return false; }
    download_pump();
    return job && job->state == WFE_DOWNLOAD_COMPLETE;
}

extern "C" int WFE_DownloadSmoke(MWContext *source, const char *report, const char *base_url, const char *output_directory)
{
    if (!source || !base_url || !output_directory) return 1;
    download_smoke = true;
    std::string base = base_url, directory = output_directory;
    auto path = [&](const char *name) { return directory + "\\" + name; };
    auto save = [&](const char *endpoint, const char *name) {
        return WFE_SaveLink(source, (base + endpoint).c_str(), path(name).c_str());
    };
    bool binary = wait_download(save("/binary.bin", "binary saved.bin"));
    WFE_Load(source, (base + "/source.html").c_str());
    DWORD started = GetTickCount();
    while (NET_AreThereActiveConnectionsForWindow(source) && GetTickCount() - started < 15000) download_pump();
    bool html = wait_download(WFE_SavePage(source, path("saved source.html").c_str()));
    History_entry *history = SHIST_GetCurrent(&source->hist);
    bool post_declined = false;
    if (history) {
        int method = history->method; history->method = URL_POST_METHOD;
        post_declined = WFE_SavePage(source, path("post must not resend.html").c_str()) == 0;
        history->method = method;
    }
    bool redirect = wait_download(save("/redirect", "redirected.bin"));
    bool redirect_close = wait_download(save("/redirect-close", "redirected close.bin"));
    bool empty = wait_download(save("/empty.bin", "empty.bin"));
    std::string existing = path("existing.bin");
    const unsigned char sentinel[] = "Keep existing download.\0\xff\r\n";
    FILE *file = fopen(existing.c_str(), "wb");
    if (file) { fwrite(sentinel, 1, sizeof(sentinel)-1, file); fclose(file); }
    uint32 bad_id = save("/truncated.bin", "existing.bin");
    bool truncated = !wait_download(bad_id) && job_for(bad_id) && job_for(bad_id)->state == WFE_DOWNLOAD_FAILED;
    HANDLE locked = CreateFileA(path("locked destination.bin").c_str(), GENERIC_READ | GENERIC_WRITE,
                                0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD seeded = 0;
    bool lock_ready = locked != INVALID_HANDLE_VALUE &&
        WriteFile(locked, sentinel, sizeof(sentinel)-1, &seeded, nullptr) && seeded == sizeof(sentinel)-1;
    uint32 locked_id = lock_ready ? save("/binary.bin", "locked destination.bin") : 0;
    bool rename_failed = lock_ready && !wait_download(locked_id) && job_for(locked_id) &&
        job_for(locked_id)->state == WFE_DOWNLOAD_FAILED;
    if (locked != INVALID_HANDLE_VALUE) CloseHandle(locked);
    file = fopen(path("blocked parent").c_str(), "wb");
    bool blocked_ready = false;
    if (file) { blocked_ready = fwrite(sentinel, 1, sizeof(sentinel)-1, file) == sizeof(sentinel)-1; fclose(file); }
    uint32 blocked_id = blocked_ready ? save("/binary.bin", "blocked parent\\file.bin") : 0;
    bool write_failed = blocked_ready && !wait_download(blocked_id) && job_for(blocked_id) &&
        job_for(blocked_id)->state == WFE_DOWNLOAD_FAILED;
    uint32 slow = save("/slow.bin", "cancelled.bin");
    started = GetTickCount();
    while (job_for(slow) && running(*job_for(slow)) && job_for(slow)->bytes == 0 && GetTickCount() - started < 5000) download_pump();
    WFE_DownloadCancel(slow); wait_download(slow);
    bool cancel = job_for(slow) && job_for(slow)->state == WFE_DOWNLOAD_CANCELLED;
    uint32 first = save("/concurrent-a.bin", "concurrent A.bin");
    uint32 second = save("/concurrent-b.bin", "concurrent B.bin");
    bool concurrent = wait_download(first) && wait_download(second);
    MWContext *disposable = WFE_NewWindow(nullptr);
    uint32 independent = disposable ? WFE_SaveLink(disposable, (base + "/concurrent-a.bin").c_str(),
                                                   path("concurrent A.bin").c_str()) : 0;
    if (disposable) WFE_CloseWindow(disposable);
    bool source_closed = disposable && wait_download(independent);
    uint32 named = start_download(NET_CreateURLStruct((base + "/filename.bin").c_str(), NET_DONT_RELOAD), directory.c_str(), FO_PRESENT);
    bool filename = wait_download(named) && job_for(named)->destination == path("badname.bin");
    uint32 missing = save("/missing.bin", "error.bin");
    bool http_error = !wait_download(missing) && job_for(missing) && job_for(missing)->state == WFE_DOWNLOAD_FAILED;
    bool cleaned = true;
    for (auto item : download_jobs) if (!item.second->temporary.empty()) cleaned = false;
    FILE *output = report ? fopen(report, "wb") : nullptr;
    if (output) {
        fprintf(output, "binary=%d\nsource=%d\nredirect=%d\nredirect_close=%d\nempty=%d\ntruncated=%d\ncancel=%d\nconcurrent=%d\nfilename=%d\nhttp_error=%d\ntemp_cleanup=%d\npost_declined=%d\nsource_closed=%d\nrename_failed=%d\nwrite_failed=%d\n",
            binary, html, redirect, redirect_close, empty, truncated, cancel, concurrent, filename, http_error, cleaned, post_declined, source_closed, rename_failed, write_failed);
        for (auto item : download_jobs) {
            fprintf(output, "job_%lu_received=%I64u\njob_%lu_expected=%I64u\n",
                (unsigned long)item.first, item.second->bytes, (unsigned long)item.first, item.second->expected);
            if (item.second->state == WFE_DOWNLOAD_FAILED)
                fprintf(output, "job_%lu_error=%s\n", (unsigned long)item.first, item.second->error.c_str());
        }
        fclose(output);
    }
    WFE_DownloadShutdown();
    download_smoke = false;
    return binary && html && redirect && redirect_close && empty && truncated && cancel && concurrent && filename && http_error && cleaned && post_declined && source_closed && rename_failed && write_failed ? 0 : 1;
}
