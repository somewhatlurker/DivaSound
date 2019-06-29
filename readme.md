DivaSound is a replacement audio output for Project DIVA Arcade Future Tone 7.10

**Why?**  
The original WASAPI output used by the game only supports exclusive mode, and
on top of that a lot of hardware can't handle it properly.  
By preventing the original output from functioning and creating our own, it
should be possible to avoid most issues.

**How?**  
The output is a [DIVA Loader](https://github.com/Rayduxz/DIVA-Loader) plugin.  
At launch it patches the game in memory to intercept the original audio
initialisation, avoiding the original exclusive-mode WASAPI code.
Some other functions from the game are then called to finish starting the
audio engine and read the output into buffers.

**Known Issues**  
* Latency can be less than stellar, depending on your hardware and OS.
  It'll probably be playable, but I'll accept PRs to help lower this.  
  Setting your audio output to use 44100Hz sample rate may slightly help.