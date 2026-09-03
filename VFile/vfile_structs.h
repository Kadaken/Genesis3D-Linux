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
