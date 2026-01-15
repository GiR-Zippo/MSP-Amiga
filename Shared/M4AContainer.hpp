#ifndef M4A_INFO_HPP
#define M4A_INFO_HPP

#include <string>
#include <vector>
#include <cstdio>
#include <vector>
#include "Common.h"

struct M4AAudioTrackInfo {
    uint32_t trackID;
    char codec[5];      // z.B. "mp4a" für AAC
    uint32_t sampleRate;
    uint16_t channels;
    uint32_t duration;      // in Millisekunden
    bool is_audio;
};

struct M4AMeta {
    std::string title;
    std::string artist;
    std::string album;
    std::string date;
    std::string genre;
    std::string comment;
    std::string composer;
    std::string encoder;
    uint32_t trackNumber;
    uint32_t trackTotal;

    uint32_t audioOffset;
    uint32_t audioSize;
    bool valid;
    std::vector<M4AAudioTrackInfo> audioTracks;
    M4AMeta() : trackNumber(0), trackTotal(0), audioOffset(0), audioSize(0), valid(false) {}
};

class M4AReader {
public:
    static bool parse(const char* filepath, M4AMeta& meta);
private:
    static uint32_t read32BE(FILE* f);
    static void parseTrack(FILE* f, long trakEnd, M4AAudioTrackInfo &info);
    static void parseBox(FILE* f, long endPos, M4AMeta& meta);
    static void handleMetadataAtom(FILE* f, const char* name, uint32_t size, M4AMeta& meta);
    static std::string readStringData(FILE* f, uint32_t parentSize);
};

#endif