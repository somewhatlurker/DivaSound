#include "framework.h"
#include <detours.h>
#pragma comment(lib, "detours.lib")

#include <windows.h>
#include <iostream>
#include <vector>

void InjectCode(void* address, const std::vector<uint8_t> data)
{
	const size_t byteCount = data.size() * sizeof(uint8_t);

	DWORD oldProtect;
	VirtualProtect(address, byteCount, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy(address, data.data(), byteCount);
	VirtualProtect(address, byteCount, oldProtect, nullptr);
}

void NopBytes(void* address, unsigned int num)
{
	std::vector<uint8_t> newbytes = {};

	for (int i = 0; i < num; ++i) newbytes.push_back(0x90);

	InjectCode(address, newbytes);
}

void testLoop()
{
	int bufferFrameCount = 441;
	byte* pData = new byte[bufferFrameCount * 4 * 2];
	while (true)
	{
		divaAudioFillbuffer(divaAudInternalBufCls, (int16_t*)pData, bufferFrameCount, false, 0); // enableFlag);
		printf("%p %d\n", divaAudInternalBufCls, pData[0]);
		Sleep(20);
	}
}


void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	(void)pDevice;
	(void)pOutput;
	(void)pInput;
	(void)frameCount;

	//printf("%d\n", *(uint64_t*)((uint64_t)divaAudInternalBufCls + 0x10));
	//while (*(uint64_t*)((uint64_t)divaAudInternalBufCls + 0x10) != 32) { }; // loop until ready????
	// this mess is probably far from ideal, but whatever
	divaAudioFillbuffer(divaAudInternalBufCls, (int16_t*)pOutput, frameCount, 0, 0); // enableFlag);
	//printf("%p %d\n", divaAudInternalBufCls, ((int16_t*)pOutput)[0]);
}

void hookedAudioInit(void *cls, uint64_t unk, uint64_t unk2)
{
	printf("[DivaSound] Loaded\n");

	divaAudCls = cls;

	// let the game set some stuff up
	divaAudioInit(divaAudCls, unk, unk2);
	printf("[DivaSound] Game audio initialised\n");

	divaAudInternalBufCls = (void*)*(uint64_t*)((uint64_t)divaAudCls + 0x70);

	//loopThread = std::thread(testLoop);

	ma_backend backends[] = { ma_backend_wasapi };
	//ma_backend backends[] = { ma_backend_dsound };
	//ma_backend backends[] = { ma_backend_winmm };

	contextConfig = ma_context_config_init();
	contextConfig.threadPriority = ma_thread_priority_highest;

	if (ma_context_init(backends, sizeof(backends) / sizeof(backends[0]), &contextConfig, &context) != MA_SUCCESS) {
		printf("[DivaSound] Failed to initialize context\n");
		return;
	}
		
	deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_s16;
	deviceConfig.playback.channels = 2;
	deviceConfig.sampleRate = 44100;
	deviceConfig.bufferSizeInFrames = 441; // actual result may be larger
	deviceConfig.periods = 3;
	deviceConfig.dataCallback = audioCallback;
	deviceConfig.pUserData = NULL;

	if (ma_device_init(&context, &deviceConfig, &device) != MA_SUCCESS) {
		printf("[DivaSound] Failed to open playback device\n");
		ma_context_uninit(&context);
		return;
	}
	printf("[DivaSound] Opened playback device\n");

	printf("[DivaSound] WASAPI buffer size: %d (%dms)\n", device.wasapi.actualBufferSizeInFramesPlayback, device.wasapi.actualBufferSizeInFramesPlayback*1000/44100);

	//divaAudioAllocInternalBuffer(divaAudInternalBufCls, unk, unk2, 1024); // this doesn't affect latency, so....   really large is fine
	divaAudioAllocInternalBuffer(divaAudInternalBufCls, unk, unk2, device.wasapi.actualBufferSizeInFramesPlayback);
	printf("[DivaSound] Allocated internal audio buffer\n");

	if (ma_device_start(&device) != MA_SUCCESS) {
		printf("[DivaSound] Failed to start playback device\n");
		ma_device_uninit(&device);
		ma_context_uninit(&context);
		return;
	}
	printf("[DivaSound] Started playback\n");
}


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		InjectCode((void*)0x0000000140A860C0, { 0x02 }); // force stereo mode

		// attempts to jump over early code (failed)
		//InjectCode((void*)0x0000000140626A50, { 0x66, 0xE9, 0x3E, 0x01 }); // JMP to 0x140626b92
		//InjectCode((void*)0x0000000140626A50, { 0x66, 0xE9, 0x86, 0x00 }); // JMP to 0x140626ada
		//InjectCode((void*)0x0000000140626A50, { 0x66, 0xE9, 0x4E, 0x00 }); // JMP to 0x140626aa2

		// attempts to remove a call here
		// first failed
		//InjectCode((void*)0x0000000140626B56, { 0x66, 0xE9, 0x38, 0x00 }); // JMP to 0x140626b92 (always take branch)
		//NopBytes((void*)0x140626ba9, 11); // remove check
		// but this works
		NopBytes((void*)0x140626b56, 7);
		NopBytes((void*)0x140626b8a, 8);
		InjectCode((void*)0x140626b8a, { 0x48, 0x83, 0xEF, 0x18 }); // fix value of RDI because I changed the flow here
		NopBytes((void*)0x140626ba9, 11);

		InjectCode((void*)0x0000000140626C29, { 0x48, 0xE9 }); // return from the original init early when we run it
    
		DisableThreadLibraryCalls(hModule);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)divaAudioInit, (PVOID)hookedAudioInit);
		DetourTransactionCommit();
	}
	return TRUE;
}

