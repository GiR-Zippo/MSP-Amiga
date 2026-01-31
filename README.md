# My Shitty Player
just a test how to NOT build a audioplayer for an Amiga

## Formats supported:
M4A, ACC, FLAC, OGG/Vorbis, MP123

WebStreams: HTTP/HTTPS MP3 and AAC+ only

Note: 
- AAC+ Streams will only work with something beefier than a VampireV2 (PiStorm/Emu68 should work)
- for seeking AAC raw files, MSP generates a seek table, this will take a while

## Requires:

### System:
- OS3.1.x or OS4
- AHI (uses device_0 @44100kHz)
- bsdsocket.library (only when webstreams are used)
- AmiSSL for HTTPS

### CPU:
- 68040 + FPU (no idea what's the lowest spec)
- VampireV2
- Emu68


## Compiling:

### Beppos Crosscompiler:
Set USE_ADE = 0 in MakeFile

The compiler can be obtained here
https://github.com/AmigaPorts/m68k-amigaos-gcc

### ADE on Amiga
Set USE_ADE = 1 in MakeFile

Be aware this will take some time, grab a coffee and compile

Note:

Atm I can't get gcc2.9 to work properly with AAC+ decoding, seems it messes up the filterbank


## ADE
if you are using ADE and the old gcc2.9 you might need the patched ahi.h proto, a patched bsdsocket.h and AmiSSL headers.


## Uses:
[MiniMP3](https://github.com/lieff/minimp3)

[DR_MP3](https://github.com/mackron/dr_libs/blob/master/dr_mp3.h)

[DR_FLAC](https://github.com/mackron/dr_libs/blob/master/dr_flac.h)

[FAAD](https://aminet.net/package/mus/edit/faad2)

[AmiSSL](https://github.com/jens-maus/amissl)

[Arrow Classic Rock](https://www.arrow.nl/)