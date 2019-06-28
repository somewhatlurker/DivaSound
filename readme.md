DivaSound is a replacement audio output for Project DIVA Arcade Future Tone.

**Why?**  
The original WASAPI output used by the game only supports exclusive mode, and
on top of that a lot of hardware can't handle it properly.  
By preventing the original output from functioning and creating our own, it
should be possible to avoid most issues.

**How?**  
The output is a .dva file (actually just a renamed dll) for use as a
[DIVA Loader](https://github.com/Rayduxz/DIVA-Loader) plugin.  
At launch it patches the game in memory to intercept the original audio
initialisation, replacing most of it with our own code.
The original buffer is read by reusing a function the game already used.

**Known Issues**  
* Latency is less than stellar, but at least playable.
  While it's not strictly a problem, I will accept PRs that help on this.
* It seems prone to crashing.
  I suspect this happens when the game can't supply enough data to fill a
  buffer, but I haven't tested this yet.