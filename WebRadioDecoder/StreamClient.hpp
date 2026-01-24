#ifndef NETWORKSTREAM_HPP
#define NETWORKSTREAM_HPP

#include "Common.h"
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include "../Shared/AudioQueue.hpp"
#include "AudioStream.hpp"

class NetworkStream : public AudioStream
{
    public:
        NetworkStream();
        ~NetworkStream();

        bool     open(const char* filename);
        bool     seek(uint32_t targetSeconds) {return false;}
        bool     seekRelative(int32_t targetSeconds) {return false;}
        uint32_t getCurrentSeconds() const {return 0;}
        uint32_t getDuration() const {return 200;}
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
        void CloseStream();

        bool IsConnected() const { return m_connected; }
        uint32_t GetBytesRead() const { return m_bytesRead; }

        int readSamples(short *buffer, int samplesToRead)
        {
            if (m_q == NULL)
            {
                memset(buffer, 0, (samplesToRead) * sizeof(short));
                return samplesToRead / 2;
            }

            unsigned int read = m_q->get(buffer, samplesToRead);
            if (read < (unsigned int)samplesToRead)
                memset(buffer + read, 0, (samplesToRead - read) * sizeof(short));
            return samplesToRead / 2;
        }

    private:
        static void TaskEntry(); // Der Amiga-Prozess-Einstieg
        void StreamLoop();       // Die Netzwerk-Logik
        bool testConnection();

        uint32_t       m_sampleRate;
        unsigned char  m_channels;
        uint64_t       m_totalSamples;

        int    m_socket;
        char   m_host[128];
        char   m_path[128];
        uint16_t m_port;

        volatile bool   m_connected;
        volatile bool   m_terminate;
        volatile uint32_t m_bytesRead;
        struct Process* m_workerProc;
        AudioQueue* m_q;
};

#endif