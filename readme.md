DivaSound is a replacement audio output for Project DIVA Arcade Future Tone 7.10

### DivaSound is now included in PD Loader.
This repo will still be updated as a standalone alternative and for the wiki,
but if you use a recent version of PD Loader you do not need to install
DivaSound separately.

--------------------------------------------------------------------------------

#### Why?
The original WASAPI output used by the game only supports exclusive mode,
and on top of that a lot of hardware can't handle it properly.  
By preventing the original output from functioning and creating our own,
these downsides can be avoided.

This will allow you to use any audio device you want, to hear audio from other
programs while the game's running, and to record/stream the game's audio like
you can with other games.  
Previously these weren't possible with PDAFT.

#### How?
The output is a [PD Loader](https://notabug.org/nastys/PD-Loader/wiki) plugin.  
It replaces the game's original audio initialisation code so that a configurable
stream will be created instead of the original exclusive-mode one.
Functions from the game are called to finish starting the audio engine and
generate output buffers as necessary.

#### Known Issues
* Latency can be less than stellar, depending on your hardware and OS.
  This seems to be a limitation of shared-mode WASAPI, but I'll accept PRs to
  help lower this.  
  Setting your audio output to use 44100Hz sample rate may slightly help.
* ASIO mode may not cleanly exit. This might be driver dependant.  
  Closing the game by double-tapping escape works best with the latest unstable
  PD Loader. For older versions, using the debug console window's close button
  seems to work well enough. (tested using ASIO4ALL)