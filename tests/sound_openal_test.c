#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "Genesis.h"
#include "Sound.h"

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

int main(void)
{
    enum { sample_count = 800, header_size = 44 };
    uint8_t wave[header_size + sample_count * 2];
    geVFile_MemoryContext memory;
    geVFile *file;
    geSound_System *system;
    geSound_Def *definition;
    geSound *source;

    memset(wave, 0, sizeof(wave));
    memcpy(wave, "RIFF", 4);
    put_u32(wave + 4, (uint32_t)sizeof(wave) - 8);
    memcpy(wave + 8, "WAVEfmt ", 8);
    put_u32(wave + 16, 16);
    put_u16(wave + 20, 1);
    put_u16(wave + 22, 1);
    put_u32(wave + 24, 8000);
    put_u32(wave + 28, 16000);
    put_u16(wave + 32, 2);
    put_u16(wave + 34, 16);
    memcpy(wave + 36, "data", 4);
    put_u32(wave + 40, sample_count * 2);

    memory.Data = wave;
    memory.DataLength = (int)sizeof(wave);
    file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_MEMORY, NULL, &memory,
                                 GE_VFILE_OPEN_READONLY);
    assert(file != NULL);
    system = geSound_CreateSoundSystem(NULL);
    assert(system != NULL);
    definition = geSound_LoadSoundDef(system, file);
    assert(definition != NULL);
    assert(geVFile_Close(file) == GE_TRUE);

    source = geSound_PlaySoundDef(system, definition, 0.5f, -0.25f, 1.0f, GE_TRUE);
    assert(source != NULL);
    assert(geSound_SoundIsPlaying(system, source) == GE_TRUE);
    assert(geSound_ModifySound(system, source, 0.75f, 0.5f, 1.25f) == GE_TRUE);
    assert(geSound_SetMasterVolume(system, 0.25f) == GE_TRUE);
    assert(geSound_GetDSound() == NULL);
    assert(geSound_StopSound(system, source) == GE_TRUE);
    geSound_FreeSoundDef(system, definition);

    memory.Data = wave;
    memory.DataLength = (int)sizeof(wave);
    file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_MEMORY, NULL, &memory,
                                 GE_VFILE_OPEN_READONLY);
    assert(file != NULL);
    definition = geSound_LoadSoundDef(system, file);
    assert(definition != NULL);
    assert(geVFile_Close(file) == GE_TRUE);
    source = geSound_PlaySoundDef(system, definition, 1.0f, 0.0f, 1.0f, GE_TRUE);
    assert(source != NULL);
    geSound_FreeAllChannels(system);
    geSound_DestroySoundSystem(system);
    return 0;
}
