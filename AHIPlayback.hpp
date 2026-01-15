#ifndef AHIPLAYBACK_HPP
#define AHIPLAYBACK_HPP

#include <exec/types.h>
#include <exec/ports.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/ahi.h>

typedef ULONG uint32;
typedef USHORT uint16;

class AudioStream;
class AHIPlayback
{
    public:
        AHIPlayback(AudioStream* s);
        ~AHIPlayback();

        bool Init();
        bool Update();
        void Stop();

        void ReinitBuffer(bool fill);
    private:
        bool fillBufferFromStream(short* buf, int maxBytes, int& bytesRead);
        void sendRequest(struct AHIRequest* r, short* data, int length, struct AHIRequest* link);
        void setAmigaFilter(bool active);

        AudioStream*        m_stream;
        struct MsgPort*     m_port;
        struct AHIRequest*  m_req[2];
        short*              m_buffer[2];
        bool                m_active;
        int                 m_bytesRead;
        int                 m_current;

        // Konstanten für die Konfiguration
        static const uint32 BUFSIZE = 16384;
        static const uint16 AHI_TYPE = AHIST_S16S;

        bool m_pause;
};

#endif