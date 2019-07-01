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

void resizeInternalBuffers(uint64_t frames)
{
	divaBufSizeInFrames = frames;
	divaAudioMixCls->mixbuffer = new float[divaBufSizeInFrames * 4];
	divaAudioMixCls->mixbuffer_size = divaBufSizeInFrames * 4 * 4;
	divaAudioMixCls->state2->buffer = new float[divaBufSizeInFrames * 4];
	divaAudioMixCls->state2->buffer_size = divaBufSizeInFrames * 4 * 4;
	printf("[DivaSound] Resized internal buffers to %d frames\n", frames);
}

void resizeTestLoop()
{
	bool dir = true;
	while (true)
	{
		Sleep(5000);

		if (dir)
			resizeInternalBuffers(divaBufSizeInFrames + 3000);
		else
			resizeInternalBuffers(divaBufSizeInFrames - 3000);

		dir = !dir;
	}
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
		//frameCount = divaBufSizeInFrames;

		// allocate a larger buffer if necessary.
		// add 128 frames of padding because this isn't normal and we want to avoid it happening again if possible.
		resizeInternalBuffers(frameCount + 128);
	}
	
	divaAudioFillbuffer(divaAudioMixCls, (int16_t*)pOutput, frameCount, 0, 0);
	//printf("%p %d\n", divaAudioMixCls, ((int16_t*)pOutput)[0]);

	if (bitDepth > 16) // we should generate the output buffer ourselves
	{
		float volumes[4];
		for (int i = 0; i < 4; i++)
		{
			volumes[i] = divaAudioMixCls->volume_master * divaAudioMixCls->volume_channels[i];
		}


		int startChannel = 4 - nChannels;

		for (int i = 0; i < frameCount; i++)
		{
			for (int currentChannel = startChannel; currentChannel < 4; currentChannel++)
			{
				// note: currentChannel is for the game's buffer, outputChannel is for our output buffer.
				// outputChannel should also be used for volumes.
				int outputChannel = currentChannel - startChannel;

				if (bitDepth == 24) // 24 bit int output
				{
					int32_t out_val = divaAudioMixCls->mixbuffer[i*4 + currentChannel] * volumes[outputChannel] * 8388607.0f;

					if (out_val > 8388607) out_val = 8388607;
					else if (out_val < -8388608) out_val = -8388608;

					uint64_t out_pos = (i*nChannels + outputChannel) * 3;

					((byte*)pOutput)[out_pos] = out_val;
					((byte*)pOutput)[out_pos + 1] = out_val >> 8;
					((byte*)pOutput)[out_pos + 2] = out_val >> 16;
				}

				else if (bitDepth == 32) // floating point output
				{
					((float*)pOutput)[i*nChannels + outputChannel] = divaAudioMixCls->mixbuffer[i*4 + currentChannel] * volumes[outputChannel];
				}
			}
		}
	}
}

void hookedAudioInit(initClass *cls, uint64_t unk, uint64_t unk2)
{
	//printf("[DivaSound] Loaded\n");

	divaAudCls = cls;

	// let the game set some stuff up
	// no longer necessary yayyyy
	//divaAudioInit(divaAudCls, unk, unk2);
	//printf("[DivaSound] Game audio initialised\n");

	// setup some variables instead of using the original init function
	divaAudCls->mixer = new audioMixer();
	divaAudioMixCls = divaAudCls->mixer;

	divaAudioMixCls->volume_mutex = new std::mutex();

	divaAudioMixCls->output_details = new formatDetails();

	
	ma_backend backends[] = { ma_backend_wasapi };

	contextConfig = ma_context_config_init();
	contextConfig.threadPriority = ma_thread_priority_highest;

	if (ma_context_init(backends, sizeof(backends) / sizeof(backends[0]), &contextConfig, &context) != MA_SUCCESS) {
		printf("[DivaSound] Failed to initialize context\n");
		return;
	}
	

	nChannels = GetPrivateProfileIntW(L"general", L"channels", 2, CONFIG_FILE);
	if (nChannels != 4) nChannels = 2;

	bitDepth = GetPrivateProfileIntW(L"general", L"bit_depth", 16, CONFIG_FILE);
	if (bitDepth != 32 && bitDepth != 24) bitDepth = 16;

	deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.channels = nChannels;
	deviceConfig.sampleRate = 44100;
	deviceConfig.bufferSizeInMilliseconds = GetPrivateProfileIntW(L"buffer", L"buffer_size", 10, CONFIG_FILE); // actual result may be larger
	deviceConfig.periods = GetPrivateProfileIntW(L"buffer", L"periods", 2, CONFIG_FILE); // 3;
	deviceConfig.dataCallback = audioCallback;
	deviceConfig.pUserData = NULL;

	if (bitDepth == 32)
		deviceConfig.playback.format = ma_format_f32;
	else if (bitDepth == 24)
		deviceConfig.playback.format = ma_format_s24;
	else
		deviceConfig.playback.format = ma_format_s16;

	if (nChannels == 4)
	{
		deviceConfig.playback.channelMap[0] = MA_CHANNEL_FRONT_LEFT;
		deviceConfig.playback.channelMap[1] = MA_CHANNEL_FRONT_RIGHT;
		deviceConfig.playback.channelMap[2] = MA_CHANNEL_BACK_LEFT;
		deviceConfig.playback.channelMap[3] = MA_CHANNEL_BACK_RIGHT;
	}


	divaAudioMixCls->output_details->channels = nChannels; // this could replace stereo patch
	divaAudioMixCls->output_details->rate = 44100; // really does nothing
	divaAudioMixCls->output_details->depth = bitDepth; // setting this to something other than 16 just removes output
	

	if (ma_device_init(&context, &deviceConfig, &device) != MA_SUCCESS) {
		printf("[DivaSound] Failed to open playback device\n");
		ma_context_uninit(&context);
		return;
	}
	//printf("[DivaSound] Opened playback device\n");

	printf("[DivaSound] Output config: %dch %dbit\n", nChannels, bitDepth);

	actualBufferSizeInMillisecondsPlayback = device.wasapi.actualBufferSizeInFramesPlayback * 1000 / device.playback.internalSampleRate; // because miniaudio doesn't seem to have this
	printf("[DivaSound] WASAPI buffer size: %d (%dms at %dHz)\n", device.wasapi.actualBufferSizeInFramesPlayback, actualBufferSizeInMillisecondsPlayback, device.playback.internalSampleRate);
	printf("[DivaSound] Buffer periods: %d\n", device.playback.internalPeriods);

	divaBufSizeInFrames = device.wasapi.actualBufferSizeInFramesPlayback * device.sampleRate / device.playback.internalSampleRate; // +128; // 128 is just a bit extra in case resampling needs it or something. idk
	divaBufSizeInMilliseconds = divaBufSizeInFrames * 1000 / device.sampleRate;
	printf("[DivaSound] PDAFT buffer size: %d (%dms at %dHz)\n", divaBufSizeInFrames, divaBufSizeInMilliseconds, device.sampleRate);


	divaAudioAllocMixer(divaAudioMixCls, unk, unk2, divaBufSizeInFrames);
	printf("[DivaSound] Created internal audio mixer\n");


	if (ma_device_start(&device) != MA_SUCCESS) {
		printf("[DivaSound] Failed to start playback device\n");
		ma_device_uninit(&device);
		ma_context_uninit(&context);
		return;
	}
	printf("[DivaSound] Started playback\n");


	//loopThread = std::thread(resizeTestLoop);
}


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		// these patches are only needed if calling the game's own init function
		// they shouldn't be necessary anymore

		//// force stereo mode
		//InjectCode((void*)0x0000000140A860C0, { 0x02 });

		//// remove a call to get device info and skip the check for it
		//NopBytes((void*)0x140626b56, 7);
		//NopBytes((void*)0x140626b8a, 8);
		//InjectCode((void*)0x140626b8a, { 0x48, 0x83, 0xEF, 0x18 }); // fix value of RDI because I changed the flow here
		//NopBytes((void*)0x140626ba9, 11);

		//// return from the original init early
		//InjectCode((void*)0x0000000140626C29, { 0x48, 0xE9 });
    
		DisableThreadLibraryCalls(hModule);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)divaAudioInit, (PVOID)hookedAudioInit);
		DetourTransactionCommit();
	}
	return TRUE;
}

