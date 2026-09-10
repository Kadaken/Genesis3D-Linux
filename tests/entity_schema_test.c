/* Synthetic schema migration and stable-name lifetime checks. */
#include "Genesis.h"
#include "ENTITIES.H"
#include "neutral_bsp_fixture.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static unsigned char data[4096];
static int used;
static void number(int32 n) { memcpy(data+used,&n,4); used+=4; }
static void string(const char *s) { int n=(int)strlen(s)+1; number(n); memcpy(data+used,s,n); used+=n; }
static void pair(const char *k,const char *v) { string(k); string(v); }
static void field(const char *k,const char *v) { pair(k,v); pair("%defaultvalue%",""); }
static void field_default(const char *k,const char *v,const char *d) { pair(k,v); pair("%defaultvalue%",d); }
typedef struct Node { int32 a; geVec3d w,g,c; geFloat z,alpha; struct Node *link; int32 bad; } Node;
static const geEntity_FieldDecl fields[]={
    {"a","int"},{"w","point"},{"g","point"},{"c","point"},
    {"z","geFloat"},{"alpha","geFloat"},{"link","ptr"},{"bad","int"}
};
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"entity schema failed at %d\n",__LINE__); goto done; } } while (0)
static geWorld *make_world(geEntity_EntitySet *schema)
{
    geVFile_MemoryContext context={0};
    geVFile *file=geVFile_OpenNewSystem(NULL,GE_VFILE_TYPE_MEMORY,NULL,&context,GE_VFILE_OPEN_CREATE);
    geWorld *world=NULL;
    if (!file) return NULL;
    if (neutral_bsp_write_with_entities(file,data,used) && geVFile_Seek(file,0,GE_VFILE_SEEKSET))
        world=geWorld_CreateWithEntitySchema(file,schema);
    geVFile_Close(file);
    return world;
}
int main(int argc,char **argv)
{
    geEntity_EntitySet *schema=NULL,*set;
    geWorld *world=NULL,*old=NULL;
    geEntity *a,*b;
    Node *p,*q;
    const char *name;
    char caller_class[]="Node", caller_field[]="a", caller_type[]="int";
    geEntity_FieldDecl copied[8];
    int ok=0, i, names=argc>1 && !strcmp(argv[1],"names");
    number(3);
    number(16); pair("classname","%typedef%"); pair("%typename%","Node");
    field("a","int"); field("g","geFloat"); field("c","point");
    field_default("alpha","int","255"); field("link","Node");
    field_default("bad","string","not-an-int"); field("removed","int");
    number(9); pair("classname","Node"); pair("%Name%","first"); pair("a","37");
    pair("g","-4.6"); pair("c","1 2 3"); pair("alpha","0"); pair("link","second");
    pair("removed","8"); pair("z","99");
    number(4); pair("classname","Node"); pair("%Name%","second"); pair("a","41"); pair("bad","invalid again");
    schema=geEntity_EntitySetCreate(); CHECK(schema);
    memcpy(copied,fields,sizeof(copied)); copied[0].Name=caller_field; copied[0].TypeName=caller_type;
    CHECK(geEntity_EntitySetDeclareClass(schema,caller_class,copied,8));
    caller_class[0]='X'; caller_field[0]='X'; caller_type[0]='X'; /* Declarations own their strings. */
    CHECK(!geEntity_EntitySetDeclareClass(schema,"Node",fields,0));
    world=make_world(schema); CHECK(world);
    geEntity_EntitySetDestroy(schema); schema=NULL; /* World owns independent metadata. */
    set=geWorld_GetEntitySet(world,NULL); CHECK(set);
    CHECK(!geEntity_EntitySetDeclareClass(set,"Node",fields,8));
    a=geEntity_EntitySetFindEntityByName(set,"first"); b=geEntity_EntitySetFindEntityByName(set,"second"); CHECK(a && b);
    p=geEntity_GetUserData(a); q=geEntity_GetUserData(b);
    if (names) {
        name=geEntity_GetNameString(a); CHECK(name && !strcmp(name,"first"));
        for (i=0;i<32;++i) { char scratch[128]; memset(scratch,'x',sizeof(scratch)); geEntity_GetName(a,scratch,sizeof(scratch)); CHECK(!strcmp(scratch,"first")); }
        CHECK(name==geEntity_GetNameString(a) && !strcmp(name,"first"));
        CHECK(!geEntity_GetNameString(NULL));
        CHECK(!geEntity_GetNameString(set->Entity)); /* typedef has no instance name */
    } else {
        CHECK(geEntity_ClassFindFieldByName(a->Class,"g")->Offset==offsetof(Node,g));
        CHECK(geEntity_ClassFindFieldByName(a->Class,"link")->Offset==offsetof(Node,link));
        CHECK(p->a==37 && q->a==41 && p->link==q);
        CHECK(p->g.X==0 && p->g.Y==-4.6f && p->g.Z==0);
        CHECK(p->c.X==1 && p->c.Y==2 && p->c.Z==3);
        CHECK(p->w.X==0 && p->w.Y==0 && p->w.Z==0 && p->z==0);
        CHECK(p->alpha==0 && q->alpha==255 && p->bad==0 && q->bad==0);
        CHECK(geEntity_ClassFindFieldByName(a->Class,"bad")->Diagnostics==1);
        CHECK(!geEntity_ClassFindFieldByName(a->Class,"removed"));
        /* No declarations leak into legacy construction; omit extra epair it rejects. */
        used=0; number(2); number(4); pair("classname","%typedef%"); pair("%typename%","Node"); field("g","geFloat");
        number(2); pair("classname","Node"); pair("g","-4.6");
        old=make_world(NULL); CHECK(old);
        set=geWorld_GetEntitySet(old,"Node"); CHECK(set);
        CHECK(*(geFloat *)geEntity_GetUserData(geEntity_EntitySetGetNextEntity(set,NULL))==-4.6f);
        schema=geEntity_EntitySetCreate(); CHECK(schema);
        copied[0].Name="g"; copied[0].TypeName="undefined_type";
        CHECK(geEntity_EntitySetDeclareClass(schema,"Node",copied,1));
        CHECK(!make_world(schema)); /* Invalid schema fails without leaking a partial world. */
    }
    ok=1;
done:
    if (old) geWorld_Free(old);
    if (world) geWorld_Free(world);
    if (schema) geEntity_EntitySetDestroy(schema);
    geVFile_CloseAPI();
    fprintf(stderr,"entity %s: %s\n",names?"names":"schema",ok?"PASS":"FAIL");
    return ok?0:1;
}
