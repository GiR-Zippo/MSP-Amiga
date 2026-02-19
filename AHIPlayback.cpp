#include "AHIPlayback.hpp"
#include "AudioStream.hpp"
#include "Shared/Configuration.hpp"
#include "Ui/gui.hpp"

#define BUFSIZE 16384
#define TYPE AHIST_S16S
// CIA-A Port A Adresse: 0xBFE001
volatile unsigned char *ciaa_porta = (unsigned char *)0xBFE001;

AHIPlayback::AHIPlayback(AudioStream *s)
    : m_stream(s), m_port(NULL), m_active(false)
{
    m_req[0] = m_req[1] = NULL;
    m_buffer[0] = new short[BUFSIZE / sizeof(short)];
    m_buffer[1] = new short[BUFSIZE / sizeof(short)];
    m_bytesRead = 0;
    m_current = 0;
    m_volume = 0xFFFF;
    m_softwareVolume = 1.0f;
    m_pause = false;
    m_useSoftVol = sConfiguration->GetConfigInt("UseSoftVolume", 1);
    //setAmigaFilter(false);
}

AHIPlayback::~AHIPlayback()
{
    //setAmigaFilter(true);
    WaitIO((struct IORequest *)m_req[m_current]);
    delete[] m_buffer[0];
    delete[] m_buffer[1];
}

bool AHIPlayback::Init()
{
    m_port = CreateMsgPort();
    if (!m_port)
        return false;

    for (int i = 0; i < 2; i++)
    {
        m_req[i] = (struct AHIRequest *)CreateIORequest(m_port, sizeof(struct AHIRequest));
        if (!m_req[i])
            return false;
        m_req[i]->ahir_Version = 4;
    }

    // AHI Gerät öffnen
    if (OpenDevice((CONST_STRPTR)AHINAME, AHI_DEFAULT_ID, (struct IORequest *)m_req[0], 0))
        return false;

    // Zweiten Request mit denselben Geräte-Daten initialisieren
    m_req[1]->ahir_Std.io_Device = m_req[0]->ahir_Std.io_Device;
    m_req[1]->ahir_Std.io_Unit = m_req[0]->ahir_Std.io_Unit;

    // Preroll: Beide Puffer füllen und abschicken
    for (int i = 0; i < 2; i++)
        if (fillBufferFromStream(m_buffer[i], BUFSIZE, m_bytesRead))
            sendRequest(m_req[i], m_buffer[i], m_bytesRead, (i == 0) ? NULL : m_req[0]);

    m_active = true;
    return true;
}

bool AHIPlayback::Update()
{
    // Warte hart, bis der aktuelle Request fertig ist.
    // Wenn er noch spielt, schläft der Task hier (0% CPU).
    // Wenn er schon fertig ist, kehrt WaitIO sofort zurück.
    WaitIO((struct IORequest *)m_req[m_current]);

    // Neu füllen:
    if (fillBufferFromStream(m_buffer[m_current], BUFSIZE, m_bytesRead))
    {
        // Request wieder abschicken
        sendRequest(m_req[m_current], m_buffer[m_current], m_bytesRead, m_req[1 - m_current]);

        // Index umschalten
        m_current = 1 - m_current;
        return true;
    }

    return false; // Song Ende
}

void AHIPlayback::Stop()
{
    if (m_req[0] && m_req[0]->ahir_Std.io_Device)
    {
        AbortIO((struct IORequest *)m_req[0]);
        AbortIO((struct IORequest *)m_req[1]);
        WaitIO((struct IORequest *)m_req[0]);
        WaitIO((struct IORequest *)m_req[1]);
        if (m_port)
        {
            struct Message *msg;
            while ((msg = GetMsg(m_port))) {}
        }
        CloseDevice((struct IORequest *)m_req[0]);
    }
    if (m_req[0])
        DeleteIORequest(m_req[0]);
    if (m_req[1])
        DeleteIORequest(m_req[1]);
    if (m_port)
        DeleteMsgPort(m_port);

    m_port = NULL;
    m_req[0] = m_req[1] = NULL;
    m_active = false;
}

bool AHIPlayback::fillBufferFromStream(short *buf, int maxBytes, int &m_bytesRead)
{
    int samplesToRead = maxBytes / sizeof(short);
    int read = m_stream->readSamples(buf, samplesToRead);

    if (m_useSoftVol)
    {
        // vol sollte ein Wert zwischen 0 und 256 sein (8-bit Fixed Point)
        // 256 = 100% Volume, 128 = 50%, usw.
        int vol = (MainUi::getInstance()->GetVolume() *256)/100;
        if (vol < 256)
        {
            for (int i = 0; i < read*2; i +=2)
            {
                // Multiplizieren und um 8 Bit nach rechts schieben (entspricht / 256)
                buf[i]   = (short)((buf[i]   * vol) >> 8); // L
                buf[i+1] = (short)((buf[i+1] * vol) >> 8); // R
            }
        }
    }

    m_bytesRead = read * 2 * sizeof(short); // Stereo-Multiplikator
    return (read > 0);
}

void AHIPlayback::sendRequest(struct AHIRequest *r, short *data, int length, struct AHIRequest *link)
{
    r->ahir_Std.io_Command = 10; // AHICMD_SETVOL
    r->ahir_Type = AHI_TYPE;   // S16S
    r->ahir_Volume = m_useSoftVol ?  0x00010000 : m_volume;
    DoIO((struct IORequest *)r);

    r->ahir_Std.io_Command = CMD_WRITE;
    r->ahir_Std.io_Data = data;
    r->ahir_Std.io_Length = length;
    r->ahir_Type = AHI_TYPE;
    r->ahir_Frequency = m_stream->getSampleRate();
    r->ahir_Volume = m_useSoftVol ?  0x00010000 : m_volume;
    r->ahir_Position = 0x8000; // Center
    r->ahir_Link = link;
    SendIO((struct IORequest *)r);
}

void AHIPlayback::setAmigaFilter(bool active)
{
    if (active)
    {
        // Bit 1 auf 0 setzen -> Filter AN, LED HELL
        *ciaa_porta &= ~(1 << 1);
    }
    else
    {
        // Bit 1 auf 1 setzen -> Filter AUS, LED DUNKEL
        *ciaa_porta |= (1 << 1);
    }
}
