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


void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	(void)pDevice;
	(void)pOutput;
	(void)pInput;
	(void)frameCount;

	if (frameCount > divaBufSizeInFrames)
	{
		printf("[DivaSound] Warning: PDAFT buffer is too small\n");
		printf("    If increasing it in your config doesn't help, let somewhatlurker know.\n");
		frameCount = divaBufSizeInFrames;
	}

	//printf("%d\n", *(uint64_t*)((uint64_t)divaAudInternalBufCls + 0x10));
	//while (*(uint64_t*)((uint64_t)divaAudInternalBufCls + 0x10) != 32) { }; // loop until ready????

	divaAudioFillbuffer(divaAudInternalMixCls, (int16_t*)pOutput, frameCount, 0, 0);
	//printf("%p %d\n", divaAudInternalBufCls, ((int16_t*)pOutput)[0]);

	if (bitDepth == 32) // floating point output
	{
		float volumes[4];
		for (int i = 0; i < 4; i++)
		{
			volumes[i] = divaAudInternalMixCls->volume_master * divaAudInternalMixCls->volume_channels[i];
		}


		int startChannel = 4 - nChannels;

		for (int i = 0; i < frameCount; i++)
		{
			for (int currentChannel = startChannel; currentChannel < 4; currentChannel++)
			{
				((float*)pOutput)[i*nChannels + currentChannel-startChannel] = divaAudInternalMixCls->mixbuffer[i*4 + currentChannel] * volumes[currentChannel-startChannel];
			}
		}
	}
}

void hookedAudioInit(void *cls, uint64_t unk, uint64_t unk2)
{
	printf("[DivaSound] Loaded\n");

	divaAudCls = cls;

	// let the game set some stuff up
	divaAudioInit(divaAudCls, unk, unk2);
	printf("[DivaSound] Game audio initialised\n");

	divaAudInternalMixCls = (audioInfo*)*(uint64_t*)((uint64_t)divaAudCls + 0x70);


	nChannels = GetPrivateProfileIntW(L"general", L"channels", 2, CONFIG_FILE);
	if (nChannels != 4) nChannels = 2;

	bitDepth = GetPrivateProfileIntW(L"general", L"bit_depth", 16, CONFIG_FILE);
	if (bitDepth != 32) bitDepth = 16;

	
	divaAudInternalMixCls->output_details->channels = nChannels; // this could replace stereo patch
	divaAudInternalMixCls->output_details->rate = 44100; // really does nothing
	divaAudInternalMixCls->output_details->depth = bitDepth; // setting this to something other than 16 just removes output
	

	ma_backend backends[] = { ma_backend_wasapi };

	contextConfig = ma_context_config_init();
	contextConfig.threadPriority = ma_thread_priority_highest;

	if (ma_context_init(backends, sizeof(backends) / sizeof(backends[0]), &contextConfig, &context) != MA_SUCCESS) {
		printf("[DivaSound] Failed to initialize context\n");
		return;
	}
	

	deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_s16;
	deviceConfig.playback.channels = nChannels;
	deviceConfig.sampleRate = 44100;
	deviceConfig.bufferSizeInMilliseconds = GetPrivateProfileIntW(L"buffer", L"buffer_size", 10, CONFIG_FILE); // actual result may be larger
	deviceConfig.periods = GetPrivateProfileIntW(L"buffer", L"periods", 2, CONFIG_FILE); // 3;
	deviceConfig.dataCallback = audioCallback;
	deviceConfig.pUserData = NULL;

	if (bitDepth == 32)
		deviceConfig.playback.format = ma_format_f32;


	if (ma_device_init(&context, &deviceConfig, &device) != MA_SUCCESS) {
		printf("[DivaSound] Failed to open playback device\n");
		ma_context_uninit(&context);
		return;
	}
	printf("[DivaSound] Opened playback device\n");


	actualBufferSizeInMillisecondsPlayback = device.wasapi.actualBufferSizeInFramesPlayback * 1000 / device.playback.internalSampleRate; // because miniaudio doesn't seem to have this
	printf("[DivaSound] WASAPI buffer size: %d (%dms at %dHz)\n", device.wasapi.actualBufferSizeInFramesPlayback, actualBufferSizeInMillisecondsPlayback, device.playback.internalSampleRate);
	printf("[DivaSound] WASAPI periods: %d\n", device.playback.internalPeriods);

	divaBufSizeInFrames = device.wasapi.actualBufferSizeInFramesPlayback * device.sampleRate / device.playback.internalSampleRate; // +128; // 128 is just a bit extra in case resampling needs it or something. idk
	divaBufSizeInMilliseconds = divaBufSizeInFrames * 1000 / device.sampleRate;
	printf("[DivaSound] PDAFT buffer size: %d (%dms at %dHz)\n", divaBufSizeInFrames, divaBufSizeInMilliseconds, device.sampleRate);


	divaAudioAllocInternalBuffers(divaAudInternalMixCls, unk, unk2, divaBufSizeInFrames);
	printf("[DivaSound] Allocated internal audio mixer\n");


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

