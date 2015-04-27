#ifndef OVR_MONITOR_ATRACE_H
#define OVR_MONITOR_ATRACE_H

#include <wx/wx.h>
#include <vector>

namespace OVR
{
namespace Monitor
{

    class SessionData;

    class ATraceThread : public wxThread
    {
        public:
            ATraceThread(SessionData &sessionData);
            virtual ~ATraceThread(void);

        private:
            virtual void *Entry(void);

            void ProcessLine(const wxString &line);

        private:
            typedef std::vector<char> DataVector;
    };

} // namespace Monitor
} // namespace OVR

#endif
