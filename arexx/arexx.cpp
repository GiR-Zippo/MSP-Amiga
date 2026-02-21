#include "arexx.hpp"
#include "../PlaybackRunner.hpp"
#include <proto/rexxsyslib.h>
#include <rexx/storage.h>


#define RC_OK    0
#define RC_WARN  5
#define RC_ERROR 10
#define RC_FATAL 20

ARexx* ARexx::instance = NULL;

bool ARexx::Init()
{
    m_rexxPort = CreateMsgPort();
    if (m_rexxPort)
    {
        m_rexxPort->mp_Node.ln_Name = (char*)"SHITTYPLAYER";
        m_rexxPort->mp_Node.ln_Pri = 0;
        m_rexxPort->mp_Node.ln_Type = NT_MSGPORT;
        m_rexxSignal = 1L << m_rexxPort->mp_SigBit;
        AddPort(m_rexxPort);
        return true;
    }
    return false;
}

void ARexx::Update()
{
    if (!m_rexxPort) return;
    struct RexxMsg *rmsg;
    while ((rmsg = (struct RexxMsg *)GetMsg(m_rexxPort)))
    {
        char *command = (char *)rmsg->rm_Args[0];
        if (command)
        {
            if (strcasestr(command, "gettitle") != NULL)
            {
                if (PlaybackRunner::getInstance()->GetStream() == NULL)
                    rmsg->rm_Result1 = 10;
                else
                    rmsg->rm_Result2 = (LONG)CreateArgstring((STRPTR)PlaybackRunner::getInstance()->GetStream()->getTitle(), 
                                                            strlen(PlaybackRunner::getInstance()->GetStream()->getTitle()));
            }
            else if (strcasestr(command, "getartist") != NULL)
            {
                if (PlaybackRunner::getInstance()->GetStream() == NULL)
                    rmsg->rm_Result1 = 10;
                else
                    rmsg->rm_Result2 = (LONG)CreateArgstring((STRPTR)PlaybackRunner::getInstance()->GetStream()->getArtist(), 
                                                            strlen(PlaybackRunner::getInstance()->GetStream()->getArtist()));
            }
            else if (strcasestr(command, "getlaptime") != NULL)
            {
                if (PlaybackRunner::getInstance()->GetStream() == NULL)
                    rmsg->rm_Result1 = 10;
                else
                {
                    char buf[32];
                    itoa(PlaybackRunner::getInstance()->GetStream()->getCurrentSeconds(), buf);
                    rmsg->rm_Result2 = (LONG)CreateArgstring((STRPTR)buf, strlen(buf));
                }
            }
            else if (strcasestr(command, "getduration") != NULL)
            {
                if (PlaybackRunner::getInstance()->GetStream() == NULL)
                    rmsg->rm_Result1 = 10;
                else
                {
                    char buf[32];
                    itoa(PlaybackRunner::getInstance()->GetStream()->getDuration(), buf);
                    rmsg->rm_Result2 = (LONG)CreateArgstring((STRPTR)buf, strlen(buf));
                }
            }
            else
                rmsg->rm_Result1 = 10; //error
        }
        ReplyMsg((struct Message *)rmsg);
    }
}

void ARexx::Cleanup()
{
    if (m_rexxPort)
    {
        RemPort(m_rexxPort);
        struct Message *msg;
        while ((msg = GetMsg(m_rexxPort)))
            ReplyMsg(msg);
        DeleteMsgPort(m_rexxPort);
        m_rexxPort = NULL;
    }
}

ARexx::ARexx()
{
    m_rexxPort = NULL;
}

