#ifndef __AREXX_HPP__
#define __AREXX_HPP__

#include "Common.h"

class ARexx
{
    public:
        static ARexx* getInstance()
        {
            if (instance == NULL)
                instance = new ARexx();
            return instance;
        }
        
        bool Init();
        void Update();
        void Cleanup();
        //Signalling
        ULONG GetSignal() { return m_rexxSignal;}

    private:
        static ARexx* instance;
        ARexx(); // Private constructor
        ARexx(const ARexx&); // Prevent copy
        ARexx& operator=(const ARexx&); // Prevent assignment
        struct Library *m_rexxSysBase;
        struct MsgPort *m_rexxPort;
        ULONG           m_rexxSignal;
};
#define sArexx ARexx::getInstance()
#endif