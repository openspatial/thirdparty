#include "OVR_Monitor_LogPanel.h"

namespace OVR
{
namespace Monitor
{

    /***********
    * LogPanel *
    ***********/

    BEGIN_EVENT_TABLE(LogPanel, wxPanel)
    END_EVENT_TABLE()

    LogPanel::LogPanel(wxWindow &parent) :
        wxPanel(&parent)
    {
        m_listbox = NULL;

        SetLabel("Log Viewer");

        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        
        m_listbox = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);
        m_listbox->InsertColumn(ID_ThreadColumn,   "Thread");
        m_listbox->InsertColumn(ID_PriorityColumn, "Priority");
        m_listbox->InsertColumn(ID_TimeColumn,     "Time");
        m_listbox->InsertColumn(ID_MessageColumn,  "Message");
        sizer->Add(m_listbox, 1, wxEXPAND);

        SetSizer(sizer);
    }

    LogPanel::~LogPanel(void)
    {
    }

    void LogPanel::pushLog(wxString threadName, Capture::LogPriority priority, double timeInSeconds, wxString msg)
    {
        wxString priorityString = "UNKNOWN";
        switch(priority)
        {
            case Capture::Log_Info:    priorityString="Info";    break;
            case Capture::Log_Warning: priorityString="Warning"; break;
            case Capture::Log_Error:   priorityString="Error";   break;
        }
        const long itemIndex = m_listbox->InsertItem(m_listbox->GetItemCount(), "");
        m_listbox->SetItem(itemIndex, ID_ThreadColumn,   threadName);
        m_listbox->SetItem(itemIndex, ID_PriorityColumn, priorityString);
        m_listbox->SetItem(itemIndex, ID_TimeColumn,     wxString::Format("%f", timeInSeconds));
        m_listbox->SetItem(itemIndex, ID_MessageColumn,  msg);
        m_listbox->EnsureVisible(itemIndex);
    }

} // namespace Monitor
} // namespace OVR
