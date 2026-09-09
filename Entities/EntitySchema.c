/* Generic application-declared entity layouts. No application schemas here. */
#include "ENTITIES.H"
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct geEntity_Declaration {
    struct geEntity_Declaration *Next;
    char *Name;
    geEntity_FieldDecl *Fields;
    int Count;
} geEntity_Declaration;

static char *CopyText(const char *s)
{
    size_t n = strlen(s)+1;
    char *p;
    if (n > UINT_MAX) return NULL;
    p = geRam_Allocate((uint32)n);
    if (p) memcpy(p,s,n);
    return p;
}
static void FreeDeclaration(geEntity_Declaration *d)
{
    int i;
    for (i=0;i<d->Count;++i) {
        void *name=(void *)d->Fields[i].Name, *type=(void *)d->Fields[i].TypeName;
        if (name) geRam_Free(name);
        if (type) geRam_Free(type);
    }
    if (d->Fields) geRam_Free(d->Fields);
    if (d->Name) geRam_Free(d->Name);
    geRam_Free(d);
}
void Ent_FreeDeclarations(geEntity_EntitySet *set)
{
    geEntity_Declaration *d, *next;
    if (!set) return;
    for (d=set->Declarations;d;d=next) { next=d->Next; FreeDeclaration(d); }
    set->Declarations=NULL;
}
GENESISAPI geBoolean geEntity_EntitySetDeclareClass(geEntity_EntitySet *set,
    const char *name, const geEntity_FieldDecl *fields, int count)
{
    geEntity_Declaration *d, **slot;
    int i,j;
    if (!set || set->Entity || set->Classes || set->Next || !name || !*name ||
        !fields || count<=0 || (size_t)count>UINT_MAX/sizeof(*fields)) return GE_FALSE;
    for (i=0;i<count;++i) {
        if (!fields[i].Name || !*fields[i].Name || !fields[i].TypeName || !*fields[i].TypeName)
            return GE_FALSE;
        for (j=0;j<i;++j) if (!stricmp(fields[i].Name,fields[j].Name)) return GE_FALSE;
    }
    d=geRam_Allocate(sizeof(*d));
    if (!d) return GE_FALSE;
    memset(d,0,sizeof(*d));
    d->Fields=geRam_Allocate((uint32)(count*sizeof(*fields)));
    if (!d->Fields) { FreeDeclaration(d); return GE_FALSE; }
    memset(d->Fields,0,count*sizeof(*fields)); d->Count=count;
    d->Name=CopyText(name);
    if (!d->Name) { FreeDeclaration(d); return GE_FALSE; }
    for (i=0;i<count;++i) {
        d->Fields[i].Name=CopyText(fields[i].Name);
        d->Fields[i].TypeName=CopyText(fields[i].TypeName);
        if (!d->Fields[i].Name || !d->Fields[i].TypeName) { FreeDeclaration(d); return GE_FALSE; }
    }
    for (slot=&set->Declarations;*slot;slot=&(*slot)->Next)
        if (!stricmp((*slot)->Name,name)) break;
    if (*slot) { geEntity_Declaration *old=*slot; d->Next=old->Next; *slot=d; FreeDeclaration(old); }
    else *slot=d;
    return GE_TRUE;
}
static void FreeFields(geEntity_Field *f)
{
    while (f) { geEntity_Field *next=f->Next; geEntity_FieldDestroy(f); f=next; }
}
static geBoolean HasInstances(geEntity_EntitySet *set, const char *name)
{
    for (;set;set=set->Next) {
        const char *n=set->Entity ? geEntity_GetStringForKey(set->Entity,"classname") : NULL;
        if (n && !stricmp(n,name)) return GE_TRUE;
    }
    return GE_FALSE;
}
geBoolean Ent_ApplySchema(geEntity_EntitySet *set, const geEntity_EntitySet *schema)
{
    const geEntity_Declaration *d;
    if (!schema) return GE_TRUE;
    if (schema->Entity || schema->Classes || schema->Next) return GE_FALSE;
    /* Install names first so declarations can reference each other. */
    for (d=schema->Declarations;d;d=d->Next) {
        geEntity_Class *c=geEntity_EntitySetFindClassByName(set,d->Name);
        if (!c) {
            c=geEntity_ClassCreate(TYPE_STRUCT,d->Name,sizeof(void *),_Alignof(void *));
            if (!c) return GE_FALSE;
            if (!c->Name) { geEntity_ClassDestroy(c); return GE_FALSE; }
            geEntity_EntitySetAddClass(set,c);
        }
        if (c->Type!=TYPE_STRUCT) return GE_FALSE;
    }
    for (d=schema->Declarations;d;d=d->Next) {
        geEntity_Class *c=geEntity_EntitySetFindClassByName(set,d->Name), pending={0};
        geEntity_Field *old;
        geBoolean report=HasInstances(set,d->Name);
        int i;
        for (i=0;i<d->Count;++i) {
            const char *type=!stricmp(d->Fields[i].TypeName,"float") ? "geFloat" : d->Fields[i].TypeName;
            geEntity_Class *t=geEntity_EntitySetFindClassByName(set,type);
            geEntity_Field *f, *source=geEntity_ClassFindFieldByName(c,d->Fields[i].Name);
            int offset;
            if (!t || t->TypeAlignment<=0 || pending.FieldSize>INT_MAX-t->TypeAlignment+1) goto Failed;
            offset=(pending.FieldSize+t->TypeAlignment-1)&~(t->TypeAlignment-1);
            if (offset>INT_MAX-t->TypeSize) goto Failed;
            f=geEntity_FieldCreate(d->Fields[i].Name,offset,t);
            if (!f) goto Failed;
            if (!f->Name) { geEntity_FieldDestroy(f); goto Failed; }
            f->HasSource=source!=NULL;
            if (source) f->SourceType=source->TypeClass->Type;
            geEntity_ClassAddField(&pending,f);
            if (report && !source)
                fprintf(stderr,"entity-schema: %s field=%s action=zero\n",d->Name,f->Name);
            else if (report && source->TypeClass->Type!=t->Type)
                fprintf(stderr,"entity-schema: %s field=%s action=reconcile from=%s to=%s\n",
                    d->Name,f->Name,source->TypeClass->Name,t->Name);
        }
        for (old=c->Fields;old;old=old->Next)
            if (report && !geEntity_ClassFindFieldByName(&pending,old->Name))
                fprintf(stderr,"entity-schema: %s field=%s action=ignore\n",d->Name,old->Name);
        if (report)
            fprintf(stderr,"entity-schema: %s level=%d declared=%d\n",d->Name,c->FieldSize,pending.FieldSize);
        FreeFields(c->Fields); c->Fields=pending.Fields; c->FieldSize=pending.FieldSize;
        c->AppDeclared=GE_TRUE;
        continue;
Failed:
        fprintf(stderr,"entity-schema: %s invalid declaration; rejecting world\n",d->Name);
        FreeFields(pending.Fields); return GE_FALSE;
    }
    return GE_TRUE;
}
static int Numbers(const char *text, double *v)
{
    char extra;
    int n=sscanf(text,"%lf %lf %lf %c",v,v+1,v+2,&extra), i;
    if (n!=1 && n!=3) return 0;
    /* A failed second conversion can otherwise hide trailing non-numeric text. */
    if (n==1 && sscanf(text,"%lf %c",v,&extra)!=1) return 0;
    for (i=0;i<n;++i) if (!isfinite(v[i]) || fabs(v[i])>FLT_MAX) return 0;
    return n;
}
geBoolean Ent_ParseDeclaredField(geEntity_EntitySet *set, geEntity *entity,
    geEntity_Field *field, const char *value, geWorld *world)
{
    char *dst=(char *)entity->UserData+field->Offset;
    geEntity_ClassType target=field->TypeClass->Type, source=field->SourceType;
    double v[3]={0,0,0};
    geBoolean ok=GE_FALSE;
    int n;
    if (!field->HasSource) return GE_TRUE;
    memset(dst,0,field->TypeClass->TypeSize);
    if (target==TYPE_INT && source==TYPE_INT) {
        char *end; long x;
        errno=0; x=strtol(value,&end,10);
        if (end!=value) {
            while (isspace((unsigned char)*end)) ++end;
            if (!*end && !errno && x>=INT32_MIN && x<=INT32_MAX) { *(int32 *)dst=(int32)x; ok=GE_TRUE; }
        }
    } else if (target==TYPE_FLOAT && (source==TYPE_INT || source==TYPE_FLOAT)) {
        if (Numbers(value,v)==1) { *(geFloat *)dst=(geFloat)v[0]; ok=GE_TRUE; }
    } else if (target==TYPE_POINT && (source==TYPE_POINT || source==TYPE_FLOAT || source==TYPE_INT)) {
        n=Numbers(value,v);
        if (n==3 && source==TYPE_POINT) {
            geVec3d *p=(geVec3d *)dst; p->X=(geFloat)v[0]; p->Y=(geFloat)v[1]; p->Z=(geFloat)v[2]; ok=GE_TRUE;
        } else if (n==1 && source!=TYPE_POINT) { ((geVec3d *)dst)->Y=(geFloat)v[0]; ok=GE_TRUE; }
    } else if (target==TYPE_COLOR && source==TYPE_COLOR) {
        if (Numbers(value,v)==3) { GE_RGBA *c=(GE_RGBA *)dst; c->r=v[0]; c->g=v[1]; c->b=v[2]; ok=GE_TRUE; }
    } else if (target==TYPE_STRING && source==TYPE_STRING) {
        *(const char **)dst=value; ok=GE_TRUE;
    } else if ((target==TYPE_STRUCT || target==TYPE_PTR) && source==TYPE_STRUCT) {
        geEntity *other=geEntity_EntitySetFindEntityByName(set,value);
        if (other) { *(void **)dst=other->UserData; ok=GE_TRUE; }
    } else if (target==TYPE_PTR && source==TYPE_PTR) {
        ok=GE_TRUE; /* Private runtime pointer remains NULL. */
    } else if (target==TYPE_MODEL && source==TYPE_MODEL && world) {
        geEntity *other=geEntity_EntitySetFindEntityByName(set,value); int32 model;
        if (other && geEntity_GetModelNumForKey(other,"Model",&model) && model>=0 &&
            model<world->CurrentBSP->BSPData.NumGFXModels) {
            *(geWorld_Model **)dst=&world->CurrentBSP->Models[model]; ok=GE_TRUE;
        }
    }
    if (!ok && !(field->Diagnostics&1)) {
        fprintf(stderr,"entity-schema: %s field=%s action=unconvertible-zero\n",entity->Class->Name,field->Name);
        field->Diagnostics|=1;
    }
    return GE_TRUE;
}
