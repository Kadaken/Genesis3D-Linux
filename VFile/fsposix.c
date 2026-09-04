/*
 * The contents of this file are subject to the Genesis3D Public License
 * Version 1.01 (the "License"); you may not use this file except in
 * compliance with the License. A copy is provided in g3dlicense.txt.
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND. The Original Code is Genesis3D, released
 * March 25, 1999. Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C.
 * All Rights Reserved. Contributor(s): Kadaken (native Linux port, 2026).
 */

#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "basetype.h"
#include "RAM.H"
#include "vfile.h"
#include "vfile._h"
#include "fsdos.h"

#define POSIX_PATH_MAX 4096
#define POSIX_FILE_SIGNATURE 0x50465331U

typedef struct PosixFile
{
	uint32 Signature;
	char *Path;
	FILE *Stream;
	geBoolean IsDirectory;
} PosixFile;

typedef struct PosixFinder
{
	DIR *Directory;
	char Base[POSIX_PATH_MAX];
	char Pattern[POSIX_PATH_MAX];
	char Current[POSIX_PATH_MAX];
} PosixFinder;

static char *Posix_Duplicate(const char *Text)
{
	size_t Length = strlen(Text) + 1;
	char *Copy = (char *)geRam_Allocate((uint32)Length);
	if (Copy)
		memcpy(Copy, Text, Length);
	return Copy;
}

static geBoolean Posix_Path(char *Out, size_t Capacity, const char *Base, const char *Name)
{
	int Written;
	size_t i;
	if (!Name)
		return GE_FALSE;
	if (Base && Base[0])
		Written = snprintf(Out, Capacity, "%s/%s", Base, Name);
	else
		Written = snprintf(Out, Capacity, "%s", Name);
	if (Written < 0 || (size_t)Written >= Capacity)
		return GE_FALSE;
	for (i = 0; Out[i]; ++i)
		if (Out[i] == '\\') Out[i] = '/';
	return GE_TRUE;
}

static void *GENESISCC Posix_Open(geVFile *FS, void *Handle, const char *Name,
	void *Context, unsigned int Flags)
{
	PosixFile *Parent = (PosixFile *)Handle;
	PosixFile *File;
	char Path[POSIX_PATH_MAX];
	const char *Mode;
	struct stat St;
	(void)FS; (void)Context;
	if (Parent && !Parent->IsDirectory)
		return NULL;
	if (!Posix_Path(Path, sizeof(Path), Parent ? Parent->Path : NULL, Name))
		return NULL;
	File = (PosixFile *)geRam_Allocate(sizeof(*File));
	if (!File) return NULL;
	memset(File, 0, sizeof(*File));
	File->Path = Posix_Duplicate(Path);
	if (!File->Path) { geRam_Free(File); return NULL; }
	if (Flags & GE_VFILE_OPEN_DIRECTORY)
	{
		if (Flags & GE_VFILE_OPEN_CREATE)
		{
			if (mkdir(Path, 0775) != 0 && errno != EEXIST) goto fail;
		}
		if (stat(Path, &St) != 0 || !S_ISDIR(St.st_mode)) goto fail;
		File->IsDirectory = GE_TRUE;
	}
	else
	{
		if (Flags & GE_VFILE_OPEN_CREATE) Mode = "w+b";
		else if (Flags & GE_VFILE_OPEN_UPDATE) Mode = "r+b";
		else Mode = "rb";
		File->Stream = fopen(Path, Mode);
		if (!File->Stream) goto fail;
	}
	File->Signature = POSIX_FILE_SIGNATURE;
	return File;
fail:
	geRam_Free(File->Path);
	geRam_Free(File);
	return NULL;
}

static void *GENESISCC Posix_OpenNew(geVFile *FS, const char *Name, void *Context, unsigned int Flags)
{ return Posix_Open(FS, NULL, Name, Context, Flags); }
static geBoolean GENESISCC Posix_Update(geVFile *FS, void *H, void *C, int S)
{ (void)FS; (void)H; (void)C; (void)S; return GE_FALSE; }
static void GENESISCC Posix_Close(void *Handle)
{
	PosixFile *File = (PosixFile *)Handle;
	if (!File) return;
	if (File->Stream) fclose(File->Stream);
	geRam_Free(File->Path);
	geRam_Free(File);
}
static geBoolean GENESISCC Posix_GetS(void *H, void *B, int N)
{ return fgets((char *)B, N, ((PosixFile *)H)->Stream) ? GE_TRUE : GE_FALSE; }
static geBoolean GENESISCC Posix_Read(void *H, void *B, int N)
{ return fread(B, 1, (size_t)N, ((PosixFile *)H)->Stream) == (size_t)N ? GE_TRUE : GE_FALSE; }
static geBoolean GENESISCC Posix_Write(void *H, const void *B, int N)
{ return fwrite(B, 1, (size_t)N, ((PosixFile *)H)->Stream) == (size_t)N ? GE_TRUE : GE_FALSE; }
static geBoolean GENESISCC Posix_Seek(void *H, int Where, geVFile_Whence Whence)
{
	int Origin = Whence == GE_VFILE_SEEKCUR ? SEEK_CUR : Whence == GE_VFILE_SEEKEND ? SEEK_END : SEEK_SET;
	return fseek(((PosixFile *)H)->Stream, Where, Origin) == 0 ? GE_TRUE : GE_FALSE;
}
static geBoolean GENESISCC Posix_Eof(const void *H)
{ return feof(((const PosixFile *)H)->Stream) ? GE_TRUE : GE_FALSE; }
static geBoolean GENESISCC Posix_Tell(const void *H, long *P)
{ *P = ftell(((const PosixFile *)H)->Stream); return *P >= 0 ? GE_TRUE : GE_FALSE; }
static geBoolean GENESISCC Posix_Size(const void *H, long *Size)
{
	struct stat St;
	if (stat(((const PosixFile *)H)->Path, &St) != 0) return GE_FALSE;
	*Size = (long)St.st_size; return GE_TRUE;
}
static geBoolean GENESISCC Posix_PropertiesForPath(const char *Path, geVFile_Properties *P)
{
	struct stat St; const char *Name;
	if (stat(Path, &St) != 0) return GE_FALSE;
	memset(P, 0, sizeof(*P));
	P->Size = (long)St.st_size;
	P->Time.Time1 = (unsigned long)St.st_mtime;
	if (S_ISDIR(St.st_mode)) P->AttributeFlags |= GE_VFILE_ATTRIB_DIRECTORY;
	if (!(St.st_mode & S_IWUSR)) P->AttributeFlags |= GE_VFILE_ATTRIB_READONLY;
	Name = strrchr(Path, '/'); Name = Name ? Name + 1 : Path;
	strncpy(P->Name, Name, sizeof(P->Name) - 1);
	return GE_TRUE;
}
static geBoolean GENESISCC Posix_GetProperties(const void *H, geVFile_Properties *P)
{ return Posix_PropertiesForPath(((const PosixFile *)H)->Path, P); }
static geBoolean GENESISCC Posix_SetSize(void *H, long Size)
{ return ftruncate(fileno(((PosixFile *)H)->Stream), (off_t)Size) == 0 ? GE_TRUE : GE_FALSE; }
static geBoolean GENESISCC Posix_SetAttributes(void *H, geVFile_Attributes A)
{
	PosixFile *F=(PosixFile *)H; struct stat St; mode_t M;
	if (stat(F->Path,&St)!=0) return GE_FALSE; M=St.st_mode;
	if (A & GE_VFILE_ATTRIB_READONLY) M &= ~(S_IWUSR|S_IWGRP|S_IWOTH); else M |= S_IWUSR;
	return chmod(F->Path,M)==0 ? GE_TRUE : GE_FALSE;
}
static geBoolean GENESISCC Posix_SetTime(void *H, const geVFile_Time *T)
{ (void)H; (void)T; return GE_FALSE; }
static geBoolean GENESISCC Posix_SetHints(void *H, const geVFile_Hints *Hints)
{ (void)H; (void)Hints; return GE_TRUE; }
static geBoolean GENESISCC Posix_Delete(geVFile *FS, void *H, const char *N)
{ char P[POSIX_PATH_MAX]; (void)FS; return Posix_Path(P,sizeof(P),((PosixFile*)H)->Path,N) && unlink(P)==0 ? GE_TRUE:GE_FALSE; }
static geBoolean GENESISCC Posix_Rename(geVFile *FS, void *H, const char *O, const char *N)
{ char A[POSIX_PATH_MAX],B[POSIX_PATH_MAX]; (void)FS; return Posix_Path(A,sizeof(A),((PosixFile*)H)->Path,O) && Posix_Path(B,sizeof(B),((PosixFile*)H)->Path,N) && rename(A,B)==0 ? GE_TRUE:GE_FALSE; }
static geBoolean GENESISCC Posix_Exists(geVFile *FS, void *H, const char *N)
{ char P[POSIX_PATH_MAX]; struct stat St; (void)FS; return Posix_Path(P,sizeof(P),((PosixFile*)H)->Path,N) && stat(P,&St)==0 ? GE_TRUE:GE_FALSE; }
static geBoolean GENESISCC Posix_Disperse(geVFile *FS, void *H, const char *D, geBoolean R)
{ (void)FS; (void)H; (void)D; (void)R; return GE_FALSE; }

static void *GENESISCC Posix_FinderCreate(geVFile *FS, void *H, const char *Spec)
{
	PosixFinder *F; char Full[POSIX_PATH_MAX]; char *Slash; (void)FS;
	if (!Posix_Path(Full,sizeof(Full),((PosixFile*)H)->Path,Spec)) return NULL;
	F=(PosixFinder*)geRam_Allocate(sizeof(*F)); if(!F) return NULL; memset(F,0,sizeof(*F));
	Slash=strrchr(Full,'/');
	if(Slash){ *Slash='\0'; strncpy(F->Base,Full,sizeof(F->Base)-1); strncpy(F->Pattern,Slash+1,sizeof(F->Pattern)-1); }
	else { strcpy(F->Base,"."); strncpy(F->Pattern,Full,sizeof(F->Pattern)-1); }
	F->Directory=opendir(F->Base); if(!F->Directory){ geRam_Free(F); return NULL; } return F;
}
static geBoolean GENESISCC Posix_FinderNext(void *H)
{
	PosixFinder *F=(PosixFinder*)H; struct dirent *E;
	while((E=readdir(F->Directory))!=NULL)
		if(strcmp(E->d_name,".") && strcmp(E->d_name,"..") && fnmatch(F->Pattern,E->d_name,0)==0)
		{ return Posix_Path(F->Current,sizeof(F->Current),F->Base,E->d_name); }
	return GE_FALSE;
}
static geBoolean GENESISCC Posix_FinderProperties(void *H, geVFile_Properties *P)
{ return Posix_PropertiesForPath(((PosixFinder*)H)->Current,P); }
static void GENESISCC Posix_FinderDestroy(void *H)
{ PosixFinder *F=(PosixFinder*)H; if(F->Directory) closedir(F->Directory); geRam_Free(F); }

static geVFile_SystemAPIs Posix_APIs = {
	Posix_FinderCreate, Posix_FinderNext, Posix_FinderProperties, Posix_FinderDestroy,
	Posix_OpenNew, Posix_Update, Posix_Open, Posix_Delete, Posix_Rename, Posix_Exists,
	Posix_Disperse, Posix_Close, Posix_GetS, Posix_Read, Posix_Write, Posix_Seek,
	Posix_Eof, Posix_Tell, Posix_Size, Posix_GetProperties, Posix_SetSize,
	Posix_SetAttributes, Posix_SetTime, Posix_SetHints
};

const geVFile_SystemAPIs *GENESISCC FSDos_GetAPIs(void) { return &Posix_APIs; }
