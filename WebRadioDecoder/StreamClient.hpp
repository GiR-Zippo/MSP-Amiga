#ifndef NETWORKSTREAM_HPP
#define NETWORKSTREAM_HPP

#include "Common.h"
#include "../Shared/AudioQueue.hpp"
#include "AudioStream.hpp"

class AmiSSL;
class NetworkStream : public AudioStream
{
    friend class StreamRunner;
    public:
        NetworkStream();
        ~NetworkStream();

        bool     open(const char* filename);
        bool     seek(uint32_t targetSeconds) {return false;}
        bool     seekRelative(int32_t targetSeconds) {return false;}
        uint32_t getCurrentSeconds() const 
        {
            Forbid();
            uint32_t rate = m_sampleRate;
            uint64_t samples = m_totalSamples;
            Permit();

            return (uint32_t)(samples / rate);
        }
        uint32_t getDuration() const {return 0;}
        uint32_t getSampleRate() const 
        {
            Forbid();
            uint32_t currentRate = m_sampleRate;
            Permit();
            return currentRate;
        }
        int      getChannels() const 
        {
            Forbid();
            int currentChannels = m_channels;
            Permit();
            return currentChannels;
        }

        bool IsConnected() const { return m_connected; }

        int readSamples(short *buffer, int samplesToRead);

    private:
        static void taskEntry(); // Der Amiga-Prozess-Einstieg
        void closeStream();
        void decodeUrlData(std::string url);
        bool testConnection();
        bool handleServerResponse(std::string response);

        uint32_t        m_sampleRate;
        unsigned char   m_channels;
        uint64_t        m_totalSamples;

        int             m_socket;
        char            m_host[128];
        char            m_path[512];
        uint16_t        m_port;
        bool            m_isHTTP;
        uint8_t         m_codec;
        volatile bool   m_connected;
        volatile bool   m_terminate;
        volatile bool   m_stop;
        struct Process* m_workerProc;
        AudioQueue*     m_q;
        //Für metainfos
        int             m_icyInterval;

        char     m_title[128];
        char     m_artist[128];
};

#endif