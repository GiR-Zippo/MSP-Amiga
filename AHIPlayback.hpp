#ifndef AHIPLAYBACK_HPP
#define AHIPLAYBACK_HPP

#include "Common.h"

class AudioStream;
class AHIPlayback
{
    public:
        /// @brief Creates the AHIPlayback
        AHIPlayback(AudioStream* s);

        /// @brief free the buffer and sets the filter back, use Stop before
        ~AHIPlayback();

        /// @brief Init the playback
        /// @return true if success 
        bool Init();

        /// @brief Send the buffer to AHI and waits [BLOCKING Function]
        /// @return true if success 
        bool Update();

        /// @brief Stops everything, not freeing the buffers
        void Stop();

    private:
        /// @brief Fill the audiobuffer
        /// @return true if we have samples
        bool fillBufferFromStream(short* buf, int maxBytes, int& bytesRead);

        /// @brief send a request to AHI
        void sendRequest(struct AHIRequest* r, short* data, int length, struct AHIRequest* link);

        /// @brief set the amiga audio filter
        void setAmigaFilter(bool active);

        AudioStream*        m_stream;
        struct MsgPort*     m_port;
        struct AHIRequest*  m_req[2];
        short*              m_buffer[2];
        bool                m_active;
        int                 m_bytesRead;
        int                 m_current;

        // Konstanten für die Konfiguration
        static const uint32_t BUFSIZE = 16384;
        static const uint16_t AHI_TYPE = AHIST_S16S;

        bool m_pause;
};

#endif