DivaSound is a replacement audio output for Project DIVA Arcade Future Tone 7.10

**Why?**  
The original WASAPI output used by the game only supports exclusive mode,
and on top of that a lot of hardware can't handle it properly.  
By preventing the original output from functioning and creating our own,
these downsides can be avoided.

This will allow you to use any audio device you want, to hear audio from other
programs while the game's running, and to record/stream the game's audio like
you can with other games.  
Previously these weren't possible with PDAFT.

**How?**  
The output is a [DIVA Loader](https://github.com/Rayduxz/DIVA-Loader) plugin.  
It replaces the game's original audio initialisation code so that a configurable
stream will be created instead of the original exclusive-mode one.
Functions from the game are called to finish starting the audio engine and
generate output buffers as necessary.

**Known Issues**  
* Latency can be less than stellar, depending on your hardware and OS.
  It'll probably be playable, but I'll accept PRs to help lower this.  
  Setting your audio output to use 44100Hz sample rate may slightly help.
* ASIO mode doesn't cleanly exit. This may be driver dependant.  
  Closing the game using the debug console window seems to work well enough.
  (tested using ASIO4ALL)