# foo_karamoe
Karaoke Mugen (https://kara.moe) integration for Foobar2000


### Karamoe
Provides a UI element for searching and queueing songs from Karamoe.  
Intended for allowing mixing local and Karamoe songs in a karaoke session without having to swap software, and for having one, consistent queue (instead separate queues in two different clients).  
In the case there is no local library, it is recommended to use Karamoe's own client, it is more feature rich than this plugin.

### Window Swapper
Window swapper service allows swapping the topmost window based on currently playing song.  
Intended to swap between video (for instance mpv plugin, for Karamoe songs, which are mostly mp4 + ass files) display and lyric display (for instance ESLyric, for local library files with LRC lyrics) automatically.

Might be split into separate plugin from foo_karamoe eventually.


### Setup
Place foobar SDK, pfc, libPPUI, and a test copy of Foobar2000 (for testing the plugin) in their relevant folders.
