#ifndef __AMISSL_HPP__
#define __AMISSL_HPP__

#include "Common.h"
#include <amissl/amissl.h>

class AmiSSL
{
    public:
        AmiSSL();
        ~AmiSSL();
        bool Fetch(const char* arg);
        bool Init();
        void Cleanup();
        bool OpenConnection(const char* host, int port);
        void CloseConnection();            // Nur SSL & Socket zu
        void CleanupAll();                 // Alles komplett abbauen

        void GenerateRandomSeed(char *buffer, int size);
        int ConnectToServer(char *, short, char *, short);
        static int verify_cb(int preverify_ok, X509_STORE_CTX *ctx);
        SSL* GetSSL() { return ssl;}
        int GetSocket() { return sock; }
        struct Library* GetSocketBase();
    private:
        SSL_CTX *ctx;;
        SSL *ssl;
        BIO *bio_err;
        int sock;
        bool m_AmiSSLInitialized;
};
#endif