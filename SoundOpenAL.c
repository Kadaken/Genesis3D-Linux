/****************************************************************************************/
/* Portable OpenAL implementation of the historical Genesis3D sound API.                */
/* Contributor(s): Kadaken (native Linux port, 2026).                                   */
/****************************************************************************************/
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "basetype.h"
#include "vfile.h"
#include "Sound.h"

struct geSound_Def {
	ALuint Buffer;
	int Channels;
	struct geSound_Def *Next;
};

struct geSound {
	ALuint Source;
	geSound_Def *Definition;
	struct geSound *Next;
};

struct geSound_System {
	ALCdevice *Device;
	ALCcontext *Context;
	geSound_Def *Definitions;
	geSound *Sounds;
	geFloat MasterVolume;
};

static uint16_t Sound_ReadU16(const uint8_t *Data)
{
	return (uint16_t)(Data[0] | ((uint16_t)Data[1] << 8));
}

static uint32_t Sound_ReadU32(const uint8_t *Data)
{
	return (uint32_t)Data[0] | ((uint32_t)Data[1] << 8) |
	       ((uint32_t)Data[2] << 16) | ((uint32_t)Data[3] << 24);
}

static geFloat Sound_Clamp(geFloat Value, geFloat Low, geFloat High)
{
	return Value < Low ? Low : (Value > High ? High : Value);
}

static void Sound_Apply(geSound *Sound, geFloat Volume, geFloat Pan, geFloat Frequency)
{
	const geFloat ClampedPan = Sound_Clamp(Pan, -1.0f, 1.0f);
	const geFloat Pitch = Sound_Clamp(Frequency, 0.01f, 4.0f);
	alSourcef(Sound->Source, AL_GAIN, Sound_Clamp(Volume, 0.0f, 1.0f));
	alSourcef(Sound->Source, AL_PITCH, Pitch);
	alSourcei(Sound->Source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSourcef(Sound->Source, AL_ROLLOFF_FACTOR, 0.0f);
	if (Sound->Definition->Channels == 1) {
		const geFloat Z = -(geFloat)sqrt((double)(1.0f - ClampedPan * ClampedPan));
		alSource3f(Sound->Source, AL_POSITION, ClampedPan, 0.0f, Z);
	} else if (alIsExtensionPresent("AL_EXT_STEREO_ANGLES")) {
		ALfloat Angles[2];
		const ALfloat Center = -ClampedPan * (ALfloat)(GE_PIOVER2);
		Angles[0] = Center - (ALfloat)(GE_PI * 0.25f);
		Angles[1] = Center + (ALfloat)(GE_PI * 0.25f);
		alSourcefv(Sound->Source, AL_STEREO_ANGLES, Angles);
	}
}

static void Sound_RemoveInstance(geSound_System *System, geSound **Link)
{
	geSound *Instance = *Link;
	*Link = Instance->Next;
	alSourceStop(Instance->Source);
	alDeleteSources(1, &Instance->Source);
	free(Instance);
	(void)System;
}

GENESISAPI geSound_System *geSound_CreateSoundSystem(HWND Window)
{
	geSound_System *System = (geSound_System *)calloc(1, sizeof(*System));
	(void)Window;
	if (System == NULL) return NULL;
	System->Device = alcOpenDevice(NULL);
	if (System->Device == NULL) { free(System); return NULL; }
	System->Context = alcCreateContext(System->Device, NULL);
	if (System->Context == NULL || !alcMakeContextCurrent(System->Context)) {
		if (System->Context != NULL) alcDestroyContext(System->Context);
		alcCloseDevice(System->Device);
		free(System);
		return NULL;
	}
	System->MasterVolume = 1.0f;
	alListenerf(AL_GAIN, 1.0f);
	return System;
}

GENESISAPI void geSound_DestroySoundSystem(geSound_System *System)
{
	if (System == NULL) return;
	geSound_FreeAllChannels(System);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(System->Context);
	alcCloseDevice(System->Device);
	free(System);
}

GENESISAPI geSound_Def *geSound_LoadSoundDef(geSound_System *System, geVFile *File)
{
	long FileSize;
	uint8_t *Bytes;
	size_t Offset;
	const uint8_t *FormatData = NULL, *SampleData = NULL;
	uint32_t FormatSize = 0, SampleSize = 0, SampleRate;
	uint16_t Encoding, Channels, Bits;
	ALenum AlFormat;
	geSound_Def *Definition;
	if (System == NULL || File == NULL || !geVFile_Size(File, &FileSize) ||
	    FileSize < 12 || FileSize > INT_MAX) return NULL;
	Bytes = (uint8_t *)malloc((size_t)FileSize);
	if (Bytes == NULL || !geVFile_Seek(File, 0, GE_VFILE_SEEKSET) ||
	    !geVFile_Read(File, Bytes, (int)FileSize)) { free(Bytes); return NULL; }
	if (memcmp(Bytes, "RIFF", 4) != 0 || memcmp(Bytes + 8, "WAVE", 4) != 0) {
		free(Bytes); return NULL;
	}
	for (Offset = 12; Offset + 8 <= (size_t)FileSize;) {
		const uint32_t ChunkSize = Sound_ReadU32(Bytes + Offset + 4);
		const size_t DataOffset = Offset + 8;
		if (ChunkSize > (size_t)FileSize - DataOffset) break;
		if (memcmp(Bytes + Offset, "fmt ", 4) == 0) {
			FormatData = Bytes + DataOffset; FormatSize = ChunkSize;
		} else if (memcmp(Bytes + Offset, "data", 4) == 0) {
			SampleData = Bytes + DataOffset; SampleSize = ChunkSize;
		}
		Offset = DataOffset + ChunkSize + (ChunkSize & 1u);
	}
	if (FormatData == NULL || FormatSize < 16 || SampleData == NULL || SampleSize > INT_MAX) {
		free(Bytes); return NULL;
	}
	Encoding = Sound_ReadU16(FormatData);
	Channels = Sound_ReadU16(FormatData + 2);
	SampleRate = Sound_ReadU32(FormatData + 4);
	Bits = Sound_ReadU16(FormatData + 14);
	if (Encoding != 1 || SampleRate == 0 ||
	    !((Channels == 1 || Channels == 2) && (Bits == 8 || Bits == 16))) {
		free(Bytes); return NULL;
	}
	AlFormat = Channels == 1 ? (Bits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16)
	                         : (Bits == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16);
	Definition = (geSound_Def *)calloc(1, sizeof(*Definition));
	if (Definition == NULL) { free(Bytes); return NULL; }
	alGetError();
	alGenBuffers(1, &Definition->Buffer);
	alBufferData(Definition->Buffer, AlFormat, SampleData, (ALsizei)SampleSize, (ALsizei)SampleRate);
	free(Bytes);
	if (alGetError() != AL_NO_ERROR) {
		if (Definition->Buffer != 0) alDeleteBuffers(1, &Definition->Buffer);
		free(Definition); return NULL;
	}
	Definition->Channels = Channels;
	Definition->Next = System->Definitions;
	System->Definitions = Definition;
	return Definition;
}

GENESISAPI void geSound_FreeSoundDef(geSound_System *System, geSound_Def *Definition)
{
	geSound_Def **DefLink;
	geSound **SoundLink;
	if (System == NULL || Definition == NULL) return;
	SoundLink = &System->Sounds;
	while (*SoundLink != NULL) {
		if ((*SoundLink)->Definition == Definition) Sound_RemoveInstance(System, SoundLink);
		else SoundLink = &(*SoundLink)->Next;
	}
	DefLink = &System->Definitions;
	while (*DefLink != NULL && *DefLink != Definition) DefLink = &(*DefLink)->Next;
	if (*DefLink == NULL) return;
	*DefLink = Definition->Next;
	alDeleteBuffers(1, &Definition->Buffer);
	free(Definition);
}

GENESISAPI void geSound_FreeAllChannels(geSound_System *System)
{
	if (System == NULL) return;
	while (System->Sounds != NULL) Sound_RemoveInstance(System, &System->Sounds);
	while (System->Definitions != NULL) geSound_FreeSoundDef(System, System->Definitions);
}

GENESISAPI geSound *geSound_PlaySoundDef(geSound_System *System, geSound_Def *Definition,
	geFloat Volume, geFloat Pan, geFloat Frequency, geBoolean Loop)
{
	geSound *Instance;
	if (System == NULL || Definition == NULL) return NULL;
	Instance = (geSound *)calloc(1, sizeof(*Instance));
	if (Instance == NULL) return NULL;
	Instance->Definition = Definition;
	alGetError();
	alGenSources(1, &Instance->Source);
	alSourcei(Instance->Source, AL_BUFFER, (ALint)Definition->Buffer);
	alSourcei(Instance->Source, AL_LOOPING, Loop ? AL_TRUE : AL_FALSE);
	Sound_Apply(Instance, Volume, Pan, Frequency);
	alSourcePlay(Instance->Source);
	if (alGetError() != AL_NO_ERROR) {
		if (Instance->Source != 0) alDeleteSources(1, &Instance->Source);
		free(Instance); return NULL;
	}
	Instance->Next = System->Sounds;
	System->Sounds = Instance;
	return Instance;
}

GENESISAPI geBoolean geSound_StopSound(geSound_System *System, geSound *Sound)
{
	geSound **Link;
	if (System == NULL || Sound == NULL) return GE_FALSE;
	for (Link = &System->Sounds; *Link != NULL; Link = &(*Link)->Next) {
		if (*Link == Sound) { Sound_RemoveInstance(System, Link); return GE_TRUE; }
	}
	return GE_FALSE;
}

GENESISAPI geBoolean geSound_ModifySound(geSound_System *System, geSound *Sound,
	geFloat Volume, geFloat Pan, geFloat Frequency)
{
	geSound *It;
	if (System == NULL || Sound == NULL) return GE_FALSE;
	for (It = System->Sounds; It != NULL; It = It->Next) {
		if (It == Sound) { Sound_Apply(It, Volume, Pan, Frequency); return GE_TRUE; }
	}
	return GE_FALSE;
}

GENESISAPI geBoolean geSound_SoundIsPlaying(geSound_System *System, geSound *Sound)
{
	geSound *It;
	ALint State;
	if (System == NULL || Sound == NULL) return GE_FALSE;
	for (It = System->Sounds; It != NULL; It = It->Next) {
		if (It == Sound) {
			alGetSourcei(It->Source, AL_SOURCE_STATE, &State);
			return State == AL_PLAYING ? GE_TRUE : GE_FALSE;
		}
	}
	return GE_FALSE;
}

GENESISAPI geBoolean geSound_SetMasterVolume(geSound_System *System, geFloat Volume)
{
	if (System == NULL) return GE_FALSE;
	System->MasterVolume = Sound_Clamp(Volume, 0.0f, 1.0f);
	alGetError();
	alListenerf(AL_GAIN, System->MasterVolume);
	return alGetError() == AL_NO_ERROR ? GE_TRUE : GE_FALSE;
}

/* DirectSound is intentionally unavailable on the portable backend. */
GENESISAPI void *geSound_GetDSound(void)
{
	return NULL;
}
