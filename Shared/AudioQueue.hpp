#include <proto/exec.h>
#include <string.h>
#include "Common.h"

class AudioQueue
{
private:
    short *m_buffer;
    unsigned int m_size;              // Größe in Samples
    volatile unsigned int m_readPtr;  // Nur vom Consumer
    volatile unsigned int m_writePtr; // Nur vom Producer

public:
    AudioQueue(unsigned int size)
        : m_size(size), m_readPtr(0), m_writePtr(0)
    {
        // WICHTIG: Chip-Memory für AHI DMA Kompatibilität
        m_buffer = (short *)AllocVec(size * sizeof(short), MEMF_CHIP | MEMF_CLEAR);
    }

    ~AudioQueue()
    {
        if (m_buffer)
            FreeVec(m_buffer);
    }

    /// @brief Wie viel Platz ist zum Schreiben da?
    /// @return 
    unsigned int getFreeSpace() const
    {
        unsigned int r = m_readPtr;
        unsigned int w = m_writePtr;
        if (w >= r)
            return m_size - (w - r) - 1;
        return r - w - 1;
    }

    /// @brief Wie viele Samples liegen bereit?
    unsigned int getAvailable() const volatile
    {
        unsigned int r = m_readPtr;
        unsigned int w = m_writePtr;
        if (w >= r)
            return w - r;
        return m_size - (r - w);
    }

    /// @brief Wird nur vom Producer aufgerufen
    bool put(const short *samples, unsigned int count)
    {
        if (getFreeSpace() < count)
            return false;

        unsigned int w = m_writePtr;
        for (unsigned int i = 0; i < count; ++i)
        {
            m_buffer[w] = samples[i];
            w = (w + 1) % m_size;
        }

        // Erst ganz am Ende den Pointer aktualisieren.
        // Die Zuweisung ist auf dem 68k atomar.
        m_writePtr = w;
        return true;
    }

    /// @brief Wird nur vom Consumer aufgerufen
    /// @param outBuffer 
    /// @param count 
    /// @return actual bytes read from buffer
    unsigned int get(short *outBuffer, unsigned int count)
    {
        unsigned int avail = getAvailable();
        if (avail == 0)
            return 0;

        unsigned int actual = (count > avail) ? avail : count;
        unsigned int r = m_readPtr;

        for (unsigned int i = 0; i < actual; ++i)
        {
            outBuffer[i] = m_buffer[r];
            r = (r + 1) % m_size;
        }

        m_readPtr = r; // Pointer für Producer wieder freigeben
        return actual;
    }

    /// @brief Is buffer initialized?
    bool isValid() const { return m_buffer != NULL; }
};