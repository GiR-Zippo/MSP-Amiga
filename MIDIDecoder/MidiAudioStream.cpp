#include "MidiAudioStream.hpp"
#include "MidiAudioStreamRunner.hpp"
#include "SF2Parser.hpp"
#include "../Shared/AudioQueue.hpp"

MidiAudioStream::MidiAudioStream()
{
    m_stop = true;
    m_workerProc = NULL;
    m_q = NULL;
    m_title[0] = 0;
    m_artist[0] = 0;
    m_currentTime =0;
    m_duration = 0;
    m_seekSeconds = -1;
    strcpy(m_title, "Unknown Title");
    strcpy(m_artist, "Unknown Artist");
}

MidiAudioStream::~MidiAudioStream()
{
    Forbid();
    m_stop = true;
    Permit();

    if (m_workerProc)
    {
        int timeout = 100; // max 2 Sekunden
        while (FindTask(m_workerProc->pr_Task.tc_Node.ln_Name) != NULL && timeout-- > 0)
            Delay(2);  // Warte 1 Tick
        m_workerProc = NULL;
    }
}

bool MidiAudioStream::open(const char *file)
{
    m_file = std::string(file);
    // Create our task
    struct TagItem playerTags[] = {
        {NP_Entry, (IPTR)MidiAudioStream::taskEntry},
        {NP_Name, (IPTR) "MidiStreamWorker"},
        {NP_Priority, 0},
        {NP_StackSize, 65536},
        {TAG_DONE, 0}};
    m_workerProc = (struct Process *)CreateNewProc(playerTags);
    if (m_workerProc)
        m_workerProc->pr_Task.tc_UserData = (APTR)this;

    return (m_workerProc != NULL);
}

int MidiAudioStream::readSamples(short *buffer, int samplesToRead)
{
    Forbid();
    if (m_stop || m_q == NULL)
    {
        Permit();
        memset(buffer, 0, samplesToRead * sizeof(short));
        return samplesToRead / 2;
    }
    
    // m_q nutzen WÄHREND Forbid() aktiv ist
    unsigned int read = m_q->get(buffer, samplesToRead);
    Permit();
    
    if (read < (unsigned int)samplesToRead)
        memset(buffer + read, 0, (samplesToRead - read) * sizeof(short));
    
    return samplesToRead / 2;
}

bool MidiAudioStream::seek(uint32_t sec)
{
    return true;
    if (sec < 0.0) return false;
    if (sec > m_duration) return false;

    m_seekSeconds = sec;
    return true;
}

void MidiAudioStream::taskEntry()
{
    struct Task *me = FindTask(NULL);
    MidiAudioStream *self = (MidiAudioStream *)me->tc_UserData;

    if (!self)
        return;

    // Wir initialisieren die AudioQueue einmal hier,
    // bevor wir in den Worker springen
    self->m_q = new AudioQueue(176400);
    self->m_stop = false;
    MidiAudioStreamRunner::Run(self);

    // Wenn der Worker fertig ist (Stream Ende oder Terminate)
    Forbid();
    self->m_stop = true;
    if (self->m_q)
    {
        delete self->m_q;
        self->m_q = NULL;
    }
    Permit();
    self->m_workerProc = NULL;
}