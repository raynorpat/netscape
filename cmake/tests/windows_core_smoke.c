/* Exercise the actual Win32 NSPR backend and the on-disk database library. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <nspr/prglobal.h>
#include <nspr/prthread.h>
#include <nspr/prmon.h>
#include <nspr/prhash.h>
#include <nspr/prprf.h>
#include "mcom_db.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

typedef struct {
    PRMonitor *monitor;
    HANDLE finished;
    int notified;
    PRThread *current;
} ThreadResult;

static void worker(void *arg, void *unused)
{
    ThreadResult *result = (ThreadResult *)arg;
    (void)unused;
    PR_EnterMonitor(result->monitor);
    result->current = PR_CurrentThread();
    result->notified = 1;
    PR_Notify(result->monitor);
    PR_ExitMonitor(result->monitor);
    SetEvent(result->finished);
}

int main(void)
{
    ThreadResult result;
    PRThread *thread;
    PRHashTable *table;
    int value = 302;
    int64 timeout;
    char formatted[64], directory[MAX_PATH], filename[MAX_PATH];
    DB *database;
    DBT key, data, found;
    static const unsigned char payload[] = {0, 1, 2, 128, 255};

    PR_Init("windows-core-smoke", 1, 1, 0);
    CHECK(PR_Initialized());
    CHECK(PR_CurrentThread() != NULL);
    memset(&result, 0, sizeof(result));
    result.monitor = PR_NewNamedMonitor(0, "smoke");
    result.finished = CreateEvent(NULL, TRUE, FALSE, NULL);
    CHECK(result.monitor && result.finished);
    PR_EnterMonitor(result.monitor);
    thread = PR_CreateThread("worker", 1, 64 * 1024);
    CHECK(thread != NULL);
    CHECK(PR_Start(thread, worker, &result, NULL) == 0);
    LL_I2L(timeout, 5000000);
    while (!result.notified) {
        CHECK(PR_Wait(result.monitor, timeout) == 0);
        CHECK(result.notified);
    }
    CHECK(result.current != NULL && result.current != PR_CurrentThread());
    PR_ExitMonitor(result.monitor);
    CHECK(WaitForSingleObject(result.finished, 5000) == WAIT_OBJECT_0);
    CloseHandle(result.finished);
    PR_DestroyMonitor(result.monitor);

    table = PR_NewHashTable(16, PR_HashString, PR_CompareStrings,
                           PR_CompareValues, NULL, NULL);
    CHECK(table != NULL);
    CHECK(PR_HashTableAdd(table, "version", &value) != NULL);
    CHECK(PR_HashTableLookup(table, "version") == &value);
    CHECK(PR_HashTableRemove(table, "version"));
    CHECK(PR_HashTableLookup(table, "version") == NULL);
    PR_HashTableDestroy(table);
    PR_snprintf(formatted, sizeof(formatted), "NSPR %d %s", value, "Win32");
    CHECK(strcmp(formatted, "NSPR 302 Win32") == 0);

    CHECK(GetTempPath(sizeof(directory), directory) != 0);
    CHECK(GetTempFileName(directory, "nsc", 0, filename) != 0);
    /* DBM adds O_BINARY internally; its public flags reject that bit. */
    database = dbopen(filename, O_RDWR | O_TRUNC, 0600, DB_HASH, NULL);
    CHECK(database != NULL);
    key.data = "binary";
    key.size = 6;
    data.data = (void *)payload;
    data.size = sizeof(payload);
    CHECK(database->put(database, &key, &data, R_NOOVERWRITE) == 0);
    CHECK(database->sync(database, 0) == 0);
    CHECK(database->close(database) == 0);
    database = dbopen(filename, O_RDONLY, 0600, DB_HASH, NULL);
    CHECK(database != NULL);
    CHECK(database->get(database, &key, &found, 0) == 0);
    CHECK(found.size == sizeof(payload));
    CHECK(memcmp(found.data, payload, sizeof(payload)) == 0);
    CHECK(database->close(database) == 0);
    CHECK(DeleteFile(filename));
    puts("Win32 NSPR threads/monitors/hash/format and DBM persistence passed");
    return 0;
}
