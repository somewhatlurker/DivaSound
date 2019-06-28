#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <string>
#include <thread>
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio/miniaudio.h"

void (__cdecl* divaAudioInit)(void* cls, uint64_t unk, uint64_t unk2) = (void(__cdecl*)(void* cls, uint64_t unk, uint64_t unk2))0x1406269F0;

void (__cdecl* divaAudioFillbuffer)(void* cls, int16_t* buf, uint64_t nFrames, bool disable, bool enable) = (void(__cdecl*)(void* cls, int16_t* buf, uint64_t nFrames, bool disable, bool enable))0x140627370;
// cls is from a pointer at audio init's cls + 0x70
// enable can't be an enable...  it's always 0(?)

int (__cdecl* divaAudioAllocInternalBuffer)(void* info, uint64_t unk, uint64_t unk2, int64_t nFrames) = (int(__cdecl*)(void* info, uint64_t unk, uint64_t unk2, int64_t nFrames))0x140626710;
// unks are the same as from the init call. 
// info seems to be some kind of class or struct...  I think the same as for divaAudioFillbuffer
// nFrames is number of audio frames to hold. internally this is multiplied by 16 (stored as 32bit float)

uint8_t* enableFlag = (uint8_t*)0x140CB3560;

void *divaAudCls;
void *divaAudInternalBufCls;

std::thread loopThread;

ma_device_config deviceConfig;
ma_device device;