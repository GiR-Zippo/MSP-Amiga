#include "AHIPlayback.hpp"
#include "AudioStream.hpp"
#include <iostream>
#include <utility/tagitem.h>
#include <proto/dos.h>
#include <hardware/cia.h>
#include <proto/cia.h>
#include <stdio.h>

#define BUFSIZE 16384
#define TYPE AHIST_S16S
// CIA-A Port A Adresse: 0xBFE001
volatile unsigned char *ciaa_porta = (unsigned char *)0xBFE001;

AHIPlayback::AHIPlayback(AudioStream *s)
    : m_port(NULL), m_active(false), m_stream(s)
{
    m_req[0] = m_req[1] = NULL;
    m_buffer[0] = new short[BUFSIZE / sizeof(short)];
    m_buffer[1] = new short[BUFSIZE / sizeof(short)];
    m_bytesRead = 0;
    m_current = 0;
    m_pause = false;
    setAmigaFilter(true);
}

AHIPlayback::~AHIPlayback()
{
    setAmigaFilter(false);
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
    if (OpenDevice((UBYTE *)AHINAME, AHI_DEFAULT_ID, (struct IORequest *)m_req[0], 0))
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
// 1. Prüfen, ob der aktuelle Request überhaupt schon fertig ist
    // CheckIO gibt den Request zurück, wenn er fertig ist, sonst NULL.
    if (CheckIO((struct IORequest *)m_req[m_current])) 
    {
        // 2. WICHTIG: Den Request mit WaitIO "abholen". 
        // Da CheckIO bereits TRUE war, blockiert WaitIO hier NICHT mehr.
        // Es räumt nur die Message-Queue sauber auf.
        WaitIO((struct IORequest *)m_req[m_current]);

        // 3. Puffer neu füllen
        if (fillBufferFromStream(m_buffer[m_current], BUFSIZE, m_bytesRead))
        {
            // 4. Wieder abschicken
            sendRequest(m_req[m_current], m_buffer[m_current], m_bytesRead, m_req[1 - m_current]);
            
            // 5. Index umschalten
            m_current = 1 - m_current;
            return true;
        }
        else {
            return false; // Song Ende
        }
    }

    // Falls der Puffer noch spielt, einfach weitermachen (non-blocking)
    return true;
}

void AHIPlayback::Stop()
{
    if (m_req[0] && m_req[0]->ahir_Std.io_Device)
    {
        AbortIO((struct IORequest *)m_req[0]);
        WaitIO((struct IORequest *)m_req[0]);
        AbortIO((struct IORequest *)m_req[1]);
        WaitIO((struct IORequest *)m_req[1]);
        CloseDevice((struct IORequest *)m_req[0]);
    }

    if (m_req[0])
        DeleteIORequest(m_req[0]);
    if (m_req[1])
        DeleteIORequest(m_req[1]);
    if (m_port)
        DeleteMsgPort(m_port);

    m_active = false;
}

void AHIPlayback::ReinitBuffer(bool fill)
{
    if (fill)
{
m_current = 0; 
    // Erste Füllung nach Seek
    if (fillBufferFromStream(m_buffer[m_current], BUFSIZE, m_bytesRead)) {
        sendRequest(m_req[m_current], m_buffer[m_current], m_bytesRead, NULL);
        m_current = 1 - m_current;
    }

}
    else
    {
AbortIO((struct IORequest*)m_req[0]);
    WaitIO((struct IORequest*)m_req[0]);
    AbortIO((struct IORequest*)m_req[1]);
    WaitIO((struct IORequest*)m_req[1]);
    }
}

bool AHIPlayback::fillBufferFromStream(short *buf, int maxBytes, int &m_bytesRead)
{
    int samplesToRead = maxBytes / sizeof(short);
    int read = m_stream->readSamples(buf, samplesToRead);

    m_bytesRead = read * 2 * sizeof(short); // Stereo-Multiplikator
    return (read > 0);
}

void AHIPlayback::sendRequest(struct AHIRequest *r, short *data, int length, struct AHIRequest *link)
{
    r->ahir_Std.io_Command = CMD_WRITE;
    r->ahir_Std.io_Data = data;
    r->ahir_Std.io_Length = length;
    r->ahir_Type = AHI_TYPE;
    r->ahir_Frequency = m_stream->getSampleRate();
    r->ahir_Volume = 0x10000;  // Volle Lautstärke
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
