#ifndef OVR_MONITOR_COLLECTIONTHREAD_H
#define OVR_MONITOR_COLLECTIONTHREAD_H

#include "OVR_Monitor_StreamProcessor.h"

#include <wx/wx.h>
#include <wx/socket.h>
#include <wx/filename.h>

namespace OVR
{
namespace Monitor
{

    class SessionData;

    class CollectionThread : public wxThread
    {
        public:
            CollectionThread(void);
            virtual ~CollectionThread(void);

            virtual void Close(void) = 0;
    };

    class SocketCollectionThread : public CollectionThread
    {
        public:
            SocketCollectionThread(wxIPV4address &addr, wxFileName outFileName, SessionData &sessionData, Capture::UInt32 connectionFlags);
            virtual ~SocketCollectionThread(void);

            virtual void Close(void);

        private:
            virtual void *Entry(void);

        private:
            const Capture::UInt32 m_collectionFlags;
            StreamProcessor       m_streamProcessor;
            wxFileName            m_outFileName;
            wxSocketClient       *m_socket;
    };

    class FileCollectionThread : public CollectionThread
    {
        public:
            FileCollectionThread(wxFileName inFileName, SessionData &sessionData);
            virtual ~FileCollectionThread(void);

            virtual void Close(void);

        private:
            virtual void *Entry(void);

        private:
            StreamProcessor     m_streamProcessor;
            wxFileName          m_inFileName;
    };

} // namespace Monitor
} // namespace OVR

#endif
