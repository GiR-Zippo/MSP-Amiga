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

    bool openSocket();
    void processMP3Stream();
    void processAACStream();
    void closeSocket();

    NetworkStream *m_parent;
    struct Library *m_SocketBase;
    AmiSSL *m_amiSSL;
    int m_socket;
};
#endif