/* Generated neutral entity schemas; no world, renderer or external data. */
#include "Genesis.h"
#include "ENTITIES.H"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct NodeData { int32 value; struct NodeData *next; } NodeData;
typedef struct Expanded { NodeData prefix; int32 extra[3]; } Expanded;
typedef struct Holder { NodeData *target; void *raw; } Holder;
static unsigned char data[4096];
static size_t used;
static void integer(int32 n) { memcpy(data+used, &n, sizeof(n)); used += sizeof(n); }
static void string(const char *s)
{
    int32 n = (int32)strlen(s)+1;
    integer(n); memcpy(data+used, s, (size_t)n); used += (size_t)n;
}
static void pair(const char *key, const char *value) { string(key); string(value); }
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"entity reserve failed at line %d\n",__LINE__); goto done; } } while (0)

int main(void)
{
    geEntity_EntitySet *set = NULL, view = {0};
    geEntity *a, *b, *h;
    Expanded *expanded;
    NodeData *first, *second;
    Holder *holder;
    int size = -1, original_size, ok = 0;
    size_t i;
    void *saved;
    integer(5);
    integer(6); pair("classname","%typedef%"); pair("%typename%","Node");
    pair("value","int"); pair("%defaultvalue%","0");
    pair("next","Node"); pair("%defaultvalue%","");
    integer(6); pair("classname","%typedef%"); pair("%typename%","Holder");
    pair("target","Node"); pair("%defaultvalue%","");
    pair("raw","ptr"); pair("%defaultvalue%","");
    integer(4); pair("classname","Node"); pair("%Name%","a"); pair("value","37"); pair("next","b");
    integer(4); pair("classname","Node"); pair("%Name%","b"); pair("value","41"); pair("next","b");
    integer(3); pair("classname","Holder"); pair("%Name%","holder"); pair("target","a");
    set = LoadEntitySet((const char *)data, (int32)used);
    CHECK(set);
    CHECK(geEntity_EntitySetGetClassFieldSize(set,"Node",&size));
    CHECK(size == sizeof(NodeData)); original_size = size;
    a = geEntity_EntitySetFindEntityByName(set,"a");
    b = geEntity_EntitySetFindEntityByName(set,"b");
    h = geEntity_EntitySetFindEntityByName(set,"holder"); CHECK(a && b && h);
    first = geEntity_GetUserData(a); second = geEntity_GetUserData(b); holder = geEntity_GetUserData(h);
    CHECK(first->value == 37 && first->next == second && second->next == second && holder->target == first);
    holder->raw = second; /* Runtime TYPE_PTR reference; reparsing would erase it. */
    first->value = 73;
    CHECK(!geEntity_EntitySetGetClassFieldSize(NULL,"Node",&size));
    CHECK(!geEntity_EntitySetGetClassFieldSize(set,"missing",&size) && size == original_size);
    CHECK(!geEntity_EntitySetGetClassFieldSize(set,"Node",NULL));
    CHECK(!geEntity_EntitySetReserveUserData(set,"missing",sizeof(Expanded)));
    CHECK(!geEntity_EntitySetReserveUserData(set,"Node",-1));
    view.Entity = a; /* A view is insufficient to repair incoming references. */
    CHECK(!geEntity_EntitySetReserveUserData(&view,"Node",sizeof(Expanded)));
    CHECK(geEntity_EntitySetReserveUserData(set,"Node",0));
    CHECK(geEntity_EntitySetReserveUserData(set,"Node",original_size));
    CHECK(geEntity_GetUserData(a) == first);
    CHECK(geEntity_EntitySetReserveUserData(set,"Node",sizeof(Expanded)));
    first = geEntity_GetUserData(a); second = geEntity_GetUserData(b);
    CHECK(first->value == 73 && second->value == 41);
    CHECK(first->next == second && second->next == second && holder->target == first && holder->raw == second);
    CHECK((uintptr_t)first % _Alignof(void *) == 0);
    CHECK((uintptr_t)first % _Alignof(GE_RGBA) == 0 && (uintptr_t)first % _Alignof(geVec3d) == 0);
    for (i=(size_t)original_size; i<sizeof(Expanded); ++i) {
        CHECK(((unsigned char *)first)[i] == 0 && ((unsigned char *)second)[i] == 0);
    }
    expanded = (Expanded *)first; expanded->extra[1] = 99; saved = first;
    CHECK(geEntity_EntitySetReserveUserData(set,"Node",sizeof(Expanded)) && geEntity_GetUserData(a) == saved);
    CHECK(geEntity_EntitySetReserveUserData(set,"Node",sizeof(Expanded)+16));
    expanded = geEntity_GetUserData(a); second = geEntity_GetUserData(b);
    CHECK(expanded->extra[1] == 99 && expanded->prefix.value == 73);
    CHECK(expanded->prefix.next == second && second->next == second && holder->target == (NodeData *)expanded && holder->raw == second);
    for (i=sizeof(Expanded); i<sizeof(Expanded)+16; ++i) CHECK(((unsigned char *)expanded)[i] == 0);
    CHECK(geEntity_EntitySetGetClassFieldSize(set,"Node",&size) && size == original_size);
    ok = 1;
done:
    if (set) geEntity_EntitySetDestroy(set);
    geVFile_CloseAPI();
    fprintf(stderr,"entity reserve: %s (declared size, zero growth, references, repeat growth)\n",ok?"PASS":"FAIL");
    return ok ? 0 : 1;
}
