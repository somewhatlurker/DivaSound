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
	// this mess is probably far from ideal, but whatever
	divaAudioFillbuffer(divaAudInternalBufCls, (int16_t*)pOutput, frameCount, 0, 0); // enableFlag);
	//printf("%p %d\n", divaAudInternalBufCls, ((int16_t*)pOutput)[0]);
}

void hookedAudioInit(void *cls, uint64_t unk, uint64_t unk2)
{
	divaAudCls = cls;

	// let the game set some stuff up
	divaAudioInit(divaAudCls, unk, unk2);

	divaAudInternalBufCls = (void*)*(uint64_t*)((uint64_t)divaAudCls + 0x70);

	//loopThread = std::thread(testLoop);

	ma_backend backends[] = { ma_backend_winmm };
	//ma_backend backends[] = { ma_backend_wasapi };
	
	deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_s16;
	deviceConfig.playback.channels = 2;
	deviceConfig.sampleRate = 44100;
	deviceConfig.bufferSizeInFrames = 441; // 10ms
	deviceConfig.periods = 3;
	deviceConfig.dataCallback = audioCallback;
	deviceConfig.pUserData = NULL;

	if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
		printf("Failed to open playback device.\n");
		return;
	}

	//printf("winmm fragment size: %d\n", device.winmm.fragmentSizeInFrames);
	//printf("wasapi buffer size: %d\n", device.wasapi.actualBufferSizeInFramesPlayback);

	divaAudioAllocInternalBuffer(divaAudInternalBufCls, unk, unk2, 882); // allow for two full buffers
	//divaAudioAllocInternalBuffer(divaAudInternalBufCls, unk, unk2, device.wasapi.actualBufferSizeInFramesPlayback);

	if (ma_device_start(&device) != MA_SUCCESS) {
		printf("Failed to start playback device.\n");
		ma_device_uninit(&device);
		return;
	}
}

void hookedAudioFillbuffer(void* cls, int16_t* buf, uint64_t nFrames, bool disable, bool enable)
{
	printf("%p (%p) %d %d %d\n", cls, divaAudInternalBufCls, nFrames, disable, enable);
	divaAudioFillbuffer(cls, buf, nFrames, disable, enable);
}


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		InjectCode((void*)0x0000000140A860C0, { 0x02 }); // force stereo mode

		//InjectCode((void*)0x0000000140626BAE, { 0x48, 0xE9 }); // return from the original init early when we run it
		InjectCode((void*)0x0000000140626C29, { 0x48, 0xE9 }); // return from the original init early when we run it
		//InjectCode((void*)0x0000000140626C3D, { 0x66, 0xE9, 0x04, 0x02 }); // return from the original init early when we run it (crashes)
		//InjectCode((void*)0x0000000140626540, { 0x48, 0xCA, 0x01, 0x00 }); // ensure output is activated?? (crashes)
		//InjectCode((void*)0x0000000140626C52, { 0x66, 0xE9, 0xE4, 0x01 });

		//I think if you just jump over any WASAPI or threading-related calls this might start to work

		//NopBytes((void*)0x140626a50, 14);
		//NopBytes((void*)0x140626a6e, 11);
		//NopBytes((void*)0x140626a97, 11);
		//NopBytes((void*)0x140626b56, 7);
		//NopBytes((void*)0x140626b8a, 8);
		//InjectCode((void*)0x140626b8a, { 0x48, 0x83, 0xEF, 0x18 }); // fix value of RDI because I changed the flow here
		//NopBytes((void*)0x140626ba9, 11);
	//NopBytes((void*)0x140626bf4, 8);
		//NopBytes((void*)0x140626c4d, 11);

		//InjectCode((void*)0x140626c7d, { 0x66, 0xE9, 0xA9, 0x00 }); // skip wasapi init and always jump to 0x140626d2a (shorter code path)
		//NopBytes((void*)0x140626d2a, 8); // remove return check for wasapi init
		//NopBytes((void*)0x140626d3c, 11); // ?
		////NopBytes((void*)0x140626d57, 13); // ?
		//NopBytes((void*)0x140626d6e, 19); // don't create event for callback
		//NopBytes((void*)0x140626da9, 5); // skip loading new thread

		//NopBytes((void*)0x140626db7, 9); // remove return check for making thread
		//NopBytes((void*)0x140626dc8, 10); // don't set thread priority
		//NopBytes((void*)0x140626ddc, 7); // ?
		//NopBytes((void*)0x140626df4, 7); // ?
		//NopBytes((void*)0x140626e0f, 7); // ?
		//NopBytes((void*)0x140626e26, 7); // ?
		//NopBytes((void*)0x140626e33, 7); // ?
		////NopBytes((void*)0x140626e51, 5); // exception check
    
		DisableThreadLibraryCalls(hModule);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)divaAudioInit, (PVOID)hookedAudioInit);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)divaAudioFillbuffer, (PVOID)hookedAudioFillbuffer);
		DetourTransactionCommit();
	}
	return TRUE;
}

