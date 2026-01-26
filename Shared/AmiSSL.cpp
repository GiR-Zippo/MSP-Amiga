#include "AmiSSL.hpp"

#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <proto/exec.h>
#include <proto/dos.h>
#define __NOLIBBASE__
#include <proto/utility.h>
#undef __NOLIBBASE__
#include <proto/amissl.h>
#include <proto/amisslmaster.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>
#include <utility/utility.h>

#include <amissl/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>

const char stack_size[] = "$STACK:102400";

#define FFlush(x) Flush(x)
#define XMKSTR(x) #x
#define MKSTR(x) XMKSTR(x)

static BPTR GetStdErr(void)
{
    BPTR err = Output();
    return err;
}


struct Library *AmiSSLBase = NULL;
struct Library *AmiSSLMasterBase =NULL;
struct Library *SocketBase =NULL;
struct Library *UtilityBase =NULL;

AmiSSL::AmiSSL()
{
    ctx = NULL;
    ssl = NULL;
    bio_err = NULL;
    sock = -1;

    m_AmiSSLInitialized = false;
}

AmiSSL::~AmiSSL()
{
}

bool AmiSSL::Init()
{
    m_AmiSSLInitialized = false;

    if (!(UtilityBase = OpenLibrary((CONST_STRPTR) "utility.library", 0)))
        FPrintf(GetStdErr(), "Couldn't open utility.library!\n");
    else if (!(SocketBase = OpenLibrary((CONST_STRPTR) "bsdsocket.library", 4)))
        FPrintf(GetStdErr(), "Couldn't open bsdsocket.library v4!\n");
    else if (!(AmiSSLMasterBase = OpenLibrary((CONST_STRPTR) "amisslmaster.library", AMISSLMASTER_MIN_VERSION)))
        FPrintf(GetStdErr(), "Couldn't open amisslmaster.library v" MKSTR(AMISSLMASTER_MIN_VERSION) "!\n");
    else if (!InitAmiSSLMaster(AMISSL_CURRENT_VERSION, TRUE))
        FPrintf(GetStdErr(), "AmiSSL version is too old!\n");
    else if (!(AmiSSLBase = OpenAmiSSL()))
        FPrintf(GetStdErr(), "Couldn't open AmiSSL!\n");
    else
        m_AmiSSLInitialized = true;

    if (!m_AmiSSLInitialized)
    {
        Cleanup();
        return false;
    }

    // Do it this way, cuz of old gcc in ADE
    struct TagItem tags[] = {
        {
            AmiSSL_ErrNoPtr,
            (Tag)&errno,
        },
        {
            AmiSSL_SocketBase,
            (Tag)SocketBase,
        },
        {TAG_DONE}};

    if (InitAmiSSLA(tags) != 0)
    {
        FPrintf(GetStdErr(), "Couldn't initialize AmiSSL!\n");
        Cleanup();
        return false;
    }

    OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);
    if ((bio_err = BIO_new(BIO_s_file())) != NULL)
        BIO_set_fp_amiga(bio_err, GetStdErr(), BIO_NOCLOSE | BIO_FP_TEXT);

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx)
        return false;

    SSL_CTX_set_default_verify_paths(ctx);
    // verify_cb muss global oder statisch sein
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_cb);
    return true;
}

void AmiSSL::Cleanup()
{
    if (AmiSSLBase)
    {
        if (m_AmiSSLInitialized)
            CleanupAmiSSLA(NULL);
        CloseAmiSSL();
    }

    if (AmiSSLMasterBase) 
        CloseLibrary(AmiSSLMasterBase);

    if(SocketBase)
        CloseLibrary(SocketBase);

    if(UtilityBase)
        CloseLibrary(UtilityBase);
}

/// @brief Get some suitable random seed data
/// @param buffer
/// @param size
void AmiSSL::GenerateRandomSeed(char *buffer, int size)
{
    int i;
    for (i = 0; i < size / 2; i++)
    {
        ((UWORD *)buffer)[i] = RangeRand(65535);
    }
}

/// @brief Connect to the specified server, either directly or through the specified proxy using HTTP CONNECT method.
int AmiSSL::ConnectToServer(char *host, short port, char *proxy, short pport)
{
    struct sockaddr_in addr;
    struct hostent *hostent;
    char buffer[1024]; /* This should be dynamically alocated */
    BOOL is_ok = FALSE;
    char *s1, *s2;
    int sock = -1;

    /* Lookup hostname */
    if ((hostent = gethostbyname((proxy && pport) ? proxy : host)) != NULL)
    {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((proxy && pport) ? pport : port);
        addr.sin_len = hostent->h_length;
        memcpy(&addr.sin_addr, hostent->h_addr, hostent->h_length);
    }
    else
        FPrintf(GetStdErr(), "Host lookup failed\n");

    /* Create a socket and connect to the server */
    if (hostent && ((sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0))
    {
        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) >= 0)
        {
            /* For proxy connection, use SSL tunneling. First issue a HTTP CONNECT
             * request and then proceed as with direct HTTPS connection.
             */
            if (proxy && pport)
            {
                /* This should be done with snprintf to prevent buffer
                 * overflows, but some compilers don't have it and
                 * handling that would be an overkill for this example
                 */
                sprintf(buffer, "CONNECT %s:%ld HTTP/1.0\r\n\r\n",
                        host, (long)port);

                /* In a real application, it would be necessary to loop
                 * until everything is sent or an error occurrs, but here we
                 * hope that everything gets sent at once.
                 */
                if (send(sock, buffer, strlen(buffer), 0) >= 0)
                {
                    int len;

                    /* Again, some optimistic behaviour: HTTP response might not be
                     * received with only one recv
                     */
                    if ((len = recv(sock, buffer, sizeof(buffer) - 1, 0)) >= 0)
                    {
                        /* Assuming it was received, find the end of
                         * the line and cut it off
                         */
                        if ((s1 = strchr(buffer, '\r')) || (s1 = strchr(buffer, '\n')))
                            *s1 = '\0';
                        else
                            buffer[len] = '\0';

                        printf("Proxy returned: %s\n", buffer);

                        /* Check if HTTP response makes sense */
                        if (strncmp(buffer, "HTTP/", 4) == 0 && (s1 = strchr(buffer, ' ')) && (s2 = strchr(++s1, ' ')) && (s2 - s1 == 3))
                        {
                            /* Only accept HTTP 200 OK response */
                            if (atol(s1) == 200)
                                is_ok = TRUE;
                            else
                                FPrintf(GetStdErr(), "Proxy responce indicates error!\n");
                        }
                        else
                            FPrintf(GetStdErr(), "Amibigous proxy responce!\n");
                    }
                    else
                        FPrintf(GetStdErr(), "Couldn't get proxy response!\n");
                }
                else
                    FPrintf(GetStdErr(), "Couldn't send request to proxy!\n");
            }
            else
                is_ok = TRUE;
        }
        else
            printf("Connect fehlgeschlagen! Error: %d\n", errno);

        if (!is_ok)
        {
            CloseSocket(sock);
            sock = -1;
        }
    }

    return (sock);
}

/// @brief This callback is called everytime OpenSSL verifies a certificate
/// @brief in the chain during a connection, indicating success or failure.
int AmiSSL::verify_cb(int preverify_ok, X509_STORE_CTX *ctx)
{
    if (!preverify_ok)
    {
        /* Here, you could ask the user whether to ignore the failure,
         * displaying information from the certificate, for example.
         */
        printf("Certificate verification failed (%s)\n",
               X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx)));
    }
    else
        X509_issuer_and_serial_hash(X509_STORE_CTX_get_current_cert(ctx));
    return preverify_ok;
}

bool AmiSSL::OpenConnection(const char *host, int port)
{
    sock = ConnectToServer((char *)host, port, NULL, 0);
    if (sock < 0)
        return false;

    ssl = SSL_new(ctx);
    if (!ssl)
    {
        CloseSocket(sock);
        return false;
    }

    SSL_set_fd(ssl, sock);
    SSL_set_tlsext_host_name(ssl, host); // Wichtig für SNI

    if (SSL_connect(ssl) <= 0)
    {
        ERR_print_errors(bio_err);
        CloseConnection();
        return false;
    }
    return true;
}

void AmiSSL::CloseConnection()
{
    if (ssl)
    {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = NULL;
    }
    if (sock != -1)
    {
        CloseSocket(sock);
        sock = -1;
    }
}

void AmiSSL::CleanupAll()
{
    CloseConnection();
    if (ctx)
    {
        SSL_CTX_free(ctx);
        ctx = NULL;
    }
    if (bio_err)
    {
        BIO_free(bio_err);
        bio_err = NULL;
    }
    Cleanup();
}

bool AmiSSL::Fetch(const char *arg)
{
    if (!Init())
        return false;

    char buffer[4096];
    char host[127], path[127];
    int port = 443;

    std::string fName = arg;
    stringToLower(fName);

    size_t pos = fName.find("://");
    if (pos == std::string::npos)
        return false;

    size_t slashPos = fName.find('/', pos + 3);

    if (slashPos == std::string::npos)
    {
        strncpy(host, fName.substr(pos + 3).c_str(), 127);
        strcpy(path, "/");
    }
    else
    {
        strncpy(host, fName.substr(pos + 3, slashPos - (pos + 3)).c_str(), 127);
        strncpy(path, fName.substr(slashPos).c_str(), 127);
    }

    printf("Connecting to: %s:%d%s\n", host, port, path);

    if (!OpenConnection(host, port))
        return false;

    sprintf(buffer, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Amiga\r\nConnection: close\r\n\r\n", path, host);
    if (SSL_write(ssl, buffer, strlen(buffer)) <= 0)
    {
        CloseConnection();
        return false;
    }

    // Schritt 3: Daten lesen
    int bytes;
    while ((bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0)
        FWrite(Output(), buffer, bytes, 1);

    // Schritt 4: Abbau
    CleanupAll();
    return (bytes >= 0);
}