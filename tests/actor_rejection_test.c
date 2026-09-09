/* Generated legacy actor archives: rejection must release partially loaded data. */
#include "Genesis.h"
#include "body.h"
#include "Errorlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "actor rejection fixture: line %d: %s\n", __LINE__, #expr); \
    exit(1); } } while (0)

static int logged_error;
/* Observe the numeric error at the public logging boundary and still log it.
 * ERRORLOG.C currently does not populate ErrorList[].ErrorID for Report(). */
void geErrorLog_AddExplicit(geErrorLog_ErrorClassType code, const char *id,
    const char *file, int line, const char *message, const char *context)
{
    typedef void (*LogFn)(geErrorLog_ErrorClassType, const char *, const char *, int,
        const char *, const char *);
    LogFn forward = (LogFn)dlsym(RTLD_NEXT, "geErrorLog_AddExplicit");
    CHECK(forward);
    logged_error = (int)code;
    forward(code, id, file, line, message, context);
}

static void run_case(int has_body, int write_body, int motions, int accepted)
{
    /* ACTR, version 0x00f1, HasBody, zero motions; explicit LE disk words. */
    unsigned char header[16] = {'A','C','T','R', 0xf1,0,0,0, 0,0,0,0, 0,0,0,0};
    geVFile *archive, *member, *file;
    geActor_Def *actor;
    header[8] = (unsigned char)has_body;
    archive = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_VIRTUAL, "neutral.act", NULL,
        GE_VFILE_OPEN_CREATE | GE_VFILE_OPEN_DIRECTORY);
    CHECK(archive);
    member = geVFile_Open(archive, "Header", GE_VFILE_OPEN_CREATE);
    CHECK(member);
    CHECK(geVFile_Write(member, header, sizeof(header)));
    CHECK(geVFile_Close(member));
    if (write_body) {
        geBody *body = geBody_Create();
        CHECK(body);
        member = geVFile_Open(archive, "Body", GE_VFILE_OPEN_CREATE);
        CHECK(member);
        CHECK(geBody_WriteToFile(body, member));
        CHECK(geVFile_Close(member));
        geBody_Destroy(&body);
    }
    if (motions) {
        member = geVFile_Open(archive, "Motions", GE_VFILE_OPEN_CREATE | GE_VFILE_OPEN_DIRECTORY);
        CHECK(member);
        CHECK(geVFile_Close(member));
    }
    CHECK(geVFile_Close(archive));
    geErrorLog_Clear();
    logged_error = -1;
    file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_DOS, "neutral.act", NULL, GE_VFILE_OPEN_READONLY);
    CHECK(file);
    actor = geActor_DefCreateFromFile(file);
    if (accepted) {
        CHECK(actor);
        CHECK(geErrorLog_Count() == 0);
        CHECK(geActor_DefDestroy(&actor));
    } else {
        CHECK(actor == NULL);
        CHECK(geErrorLog_Count() > 0);
        CHECK(logged_error == (int)ERR_ACTOR_FILE_READ);
    }
    CHECK(geVFile_Close(file));
    fprintf(stderr, "actor rejection: body=%d stored=%d motions=%d accepted=%d\n",
        has_body, write_body, motions, accepted);
}

int main(void)
{
    char directory[] = "/tmp/genesis-actor-rejection-XXXXXX";
    CHECK(mkdtemp(directory));
    CHECK(chdir(directory) == 0);
    run_case(1, 1, 1, 1); /* Body writer/reader control, no external data. */
    run_case(1, 1, 0, 0); /* Missing Motions after closing Body. */
    run_case(1, 0, 1, 0); /* Missing required Body. */
    run_case(0, 0, 0, 0); /* Missing Motions after closing Header. */
    geVFile_CloseAPI();
    CHECK(unlink("neutral.act") == 0);
    CHECK(chdir("/") == 0);
    CHECK(rmdir(directory) == 0);
    return 0;
}
