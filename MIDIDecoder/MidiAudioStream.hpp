#ifndef MIDIAUDIOSTREAM_HPP
#define MIDIAUDIOSTREAM_HPP

#include "AudioStream.hpp"
class AudioQueue;
class MidiAudioStream : public AudioStream
{
    friend class MidiAudioStreamRunner;
    public:
        MidiAudioStream();
        ~MidiAudioStream();
        bool open(const char *file);
        int readSamples(short *buf, int len);
        bool seek(uint32_t sec);
        bool seekRelative(int32_t sec) { return false; }
        uint32_t getCurrentSeconds() const { return m_currentTime; }
        uint32_t getDuration() const { return m_duration; }
        uint32_t getSampleRate() const { return 44100; }
        int getChannels() const { return 2; }
        const char* getTitle() const { return m_title; }
        const char* getArtist() const { return m_artist; }

    private:
        static void taskEntry();
        struct Process* m_workerProc;
        AudioQueue*     m_q;
        bool            m_stop;
        std::string     m_file;

        int32_t         m_seekSeconds;
        uint32_t        m_currentTime;
        uint32_t        m_duration;
        char            m_title[128];
        char            m_artist[128];
};
#endif