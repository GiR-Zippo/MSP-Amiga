# My Shitty Player
just a test how to NOT build a audioplayer for an Amiga

## Formats supported:
M4A, ACC, FLAC, OGG/Vorbis, MP123

WebStreams: HTTP/HTTPS MP3 only

## Requires:

### System:
- OS3.1.x
- AHI (uses device_0 @44100kHz)
- bsdsocket.library (only when webstreams are used)

### CPU:
- 68040 + FPU (no idea what's the lowest spec)
- VampireV2
- Emu68


## Compiling:
If you are using beppos crosscompiler set USE_ADE = 0 in MakeFile

The compiler can be obtained here
https://github.com/AmigaPorts/m68k-amigaos-gcc

You can also compile via ADE native on your Amiga set USE_ADE = 1 in MakeFile
Be awe this will take some time, grab a coffee and compile

## ADE
if you are using ADE and the old gcc2.9 you might need the patched ahi.h proto, a patched bsdsocket.h and AmiSSL headers.

## Uses:
[MiniMP3](https://github.com/lieff/minimp3)

[DR_MP3](https://github.com/mackron/dr_libs/blob/master/dr_mp3.h)

[DR_FLAC](https://github.com/mackron/dr_libs/blob/master/dr_flac.h)

[FAAD](https://aminet.net/package/mus/edit/faad2)

[AmiSSL](https://github.com/jens-maus/amissl)