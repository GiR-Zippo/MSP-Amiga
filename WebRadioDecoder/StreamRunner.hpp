#ifndef __STREAMRUNNER_HPP__
#define __STREAMRUNNER_HPP__
#include "../Common.h"
#include "../Shared/libtremor/ogg/ogg.h"

class AmiSSL;
class NetworkStream;
class StreamRunner
{
    public:
        static void Run(NetworkStream *parent);

    private:
        StreamRunner(NetworkStream *parent);
        ~StreamRunner();


        /// @brief Start the whole streamreader
        void startup();

        /// @brief Opens the network socket
        bool openSocket();

        /// @brief processes a mp3 webstream
        void processMP3Stream();

        /// @brief processes an aac webstream
        void processAACStream();

        /// @brief processes a vorbis webstream
        void processVorbisStream();

        /// @brief processes an opus webstream
        void processOpusStream();

        /// @brief read the HTTP header and codec
        bool readHeader();

        /// @brief reads the data from the socket and store into m_ringBuffer
        bool readStream(int RING_SIZE, int RING_MASK);

        /// @brief reads the ICY-Meta from stream
        /// @return tempStack
        unsigned char *readIcyMeta(int RING_SIZE, int RING_MASK, unsigned char *tempStack);

        /// @brief sets the m_ringBuffer position pointers
        void icyAfterDecode(int RING_SIZE, int RING_MASK, int frame_bytes);

        /// @brief Parses Vorbis comment tags and sets title/artist metadata.
        void parseVorbisComments(const char **comments, int count);

        /// @brief Parses an OpusTags packet and extracts title/artist metadata.
        void parseOpusTags(const unsigned char *data, long bytes);

        /// @brief Reads 1440 bytes from the ring buffer into the Ogg sync layer.
        bool pumpOggData(ogg_sync_state *oy);

        /// @brief close the socet
        void closeSocket();

        /// @brief gets the offset after http header
        int get_body_offset(const char *buffer, int len);

        /// @brief extract the icy interval
        long extract_icy_metaint(const char *header);

        NetworkStream *m_parent;
        struct Library *m_SocketBase;
        AmiSSL *m_amiSSL;
        int m_socket;

        uint8_t m_codec;
        //ringbuffer
        int RING_SIZE = 65536; // Ringpuffer Setup (64KB)
        int RING_MASK = RING_SIZE - 1;
        unsigned char *m_ringBuffer;
        int m_writePos; // Netzwerk-Schreibzeiger
        int m_readPos;  // Decoder-Lesezeiger
        int m_filled;   // Wie viele Bytes sind "ungelesen" im Puffer

        //icy-tags
        int m_icyInterval;
        int m_bytesUntilMeta;
        int m_metaLenBytes;
};
#endif