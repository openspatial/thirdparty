#ifndef OVR_MONITOR_LOGPANEL_H
#define OVR_MONITOR_LOGPANEL_H

#include <OVR_Capture_Types.h>

#include <wx/wx.h>
#include <wx/listctrl.h>

namespace OVR
{
namespace Monitor
{

    class LogPanel : public ::wxPanel
    {
        DECLARE_EVENT_TABLE()
        public:
            LogPanel(wxWindow &parent);
            virtual ~LogPanel(void);

            void pushLog(wxString threadName, Capture::LogPriority priority, double timeInSeconds, wxString msg);

        private:
            enum ColumnID
            {
                ID_ThreadColumn = 0,
                ID_PriorityColumn,
                ID_TimeColumn,
                ID_MessageColumn
            };

        private:
            wxListCtrl *m_listbox;
    };

} // namespace Monitor
} // namespace OVR

#endif
