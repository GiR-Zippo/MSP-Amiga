#ifndef __STREAMRUNNER_HPP__
#define __STREAMRUNNER_HPP__

class AmiSSL;
class NetworkStream;
class StreamRunner
{
    public:
        static void Run(NetworkStream *parent);

    private:
        StreamRunner(NetworkStream *parent);
        ~StreamRunner();

        /// @brief Opens the network socket
        bool openSocket();

        /// @brief processes a mp3 webstream
        void processMP3Stream();

        /// @brief processes an aac webstream
        void processAACStream();

        /// @brief read the HTTP header
        bool readHeader();

        /// @brief reads the data from the socket and store into m_ringBuffer
        bool readStream(int RING_SIZE, int RING_MASK);

        /// @brief reads the ICY-Meta from stream
        /// @return tempStack
        unsigned char *readIcyMeta(int RING_SIZE, int RING_MASK, unsigned char *tempStack);

        /// @brief sets the m_ringBuffer position pointers
        void icyAfterDecode(int RING_SIZE, int RING_MASK, int frame_bytes);

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

        //ringbuffer
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