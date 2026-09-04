/*
 * The contents of this file are subject to the Genesis3D Public License
 * Version 1.01 (the "License"); you may not use this file except in
 * compliance with the License. A copy is provided in g3dlicense.txt.
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND. The Original Code is Genesis3D, released
 * March 25, 1999. Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C.
 * All Rights Reserved. Contributor(s): Kadaken (native Linux port, 2026).
 */

#ifndef	VFILESTRUCT_H
#define	VFILESTRUCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include	<stdio.h>
#include	<pthread.h>
#include	<assert.h>
#include	<stdarg.h>
#include	<string.h>

#include	"basetype.h"
#include	"RAM.H"
#include	"vfile.h"
#include	"vfile._h"
#include	"fsdos.h"
#include	"FSMEMORY.H"
#include	"fsvfs.h"

typedef	struct	FSSearchList
{
	geVFile *				FS;
	struct FSSearchList *	Next;
}	FSSearchList;


typedef	struct	geVFile
{
	geVFile_TypeIdentifier		SystemType;
	const geVFile_SystemAPIs *	APIs;
	void *						FSData;
	geVFile *					Context;
	FSSearchList *				SearchList;
	pthread_mutex_t			CriticalSection;
	geVFile *					BaseFile;
}	geVFile;

typedef struct	geVFile_Finder
{
	const geVFile_SystemAPIs *	APIs;
	void *						Data;
}	geVFile_Finder;

#ifdef __cplusplus
}
#endif

#endif
