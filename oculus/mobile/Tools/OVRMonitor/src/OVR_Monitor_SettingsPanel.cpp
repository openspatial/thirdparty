#include "OVR_Monitor_SettingsPanel.h"
#include "OVR_Monitor_Settings.h"

#include <wx/sizer.h>
#include <wx/filename.h>

namespace OVR
{
namespace Monitor
{

    BEGIN_EVENT_TABLE(SettingsPanel, ::wxPanel)
        EVT_BUTTON(ID_LocateADBButton,           SettingsPanel::onLocateADB)
        EVT_BUTTON(ID_LocateRecordingsDirButton, SettingsPanel::onLocateRecordingsDir)
        EVT_SLIDER(ID_FrameBufferCompressionSlider, SettingsPanel::onFrameBufferCompressionChanged)
    END_EVENT_TABLE()

    SettingsPanel::SettingsPanel(::wxWindow &parent) :
        ::wxPanel(&parent)
    {
        SetLabel("Settings");

        Settings settings;

        wxFlexGridSizer *sizer = new wxFlexGridSizer(3);
        sizer->AddGrowableCol(0, 1);
        sizer->AddGrowableCol(1, 4);

        const int sizerFlags = wxTOP | wxLEFT | wxRIGHT | wxEXPAND;

        m_adbPath = new wxTextCtrl(  this, wxID_ANY, settings.GetADBPath().GetFullPath());
        m_adbPath->SetEditable(false);
        sizer->Add(new wxStaticText(this, wxID_ANY, "ADB Path"),  0, sizerFlags, 20);
        sizer->Add(m_adbPath,                                     1, sizerFlags, 20);
        sizer->Add(new wxButton(this, ID_LocateADBButton, "Browse"), 0, sizerFlags, 20);

        m_recordingsDir = new wxTextCtrl(  this, wxID_ANY, settings.GetRecordingsDir().GetPath());
        m_recordingsDir->SetEditable(false);
        sizer->Add(new wxStaticText(this, wxID_ANY, "Recordings Directory"), 0, sizerFlags, 20);
        sizer->Add(m_recordingsDir,                                          1, sizerFlags, 20);
        sizer->Add(new wxButton(this, ID_LocateRecordingsDirButton, "Browse"),  0, sizerFlags, 20);

        m_fbCompressionQuality = new wxSlider(  this, ID_FrameBufferCompressionSlider, settings.GetFrameBufferCompressionQuality(), 0, 100);
        sizer->Add(new wxStaticText(this, wxID_ANY, "Frame Buffer Compression Quality"), 0, sizerFlags, 20);
        sizer->Add(m_fbCompressionQuality,                                               1, sizerFlags, 20);

        SetSizer(sizer);
    }

    SettingsPanel::~SettingsPanel(void)
    {
    }

    void SettingsPanel::onLocateADB(wxCommandEvent &event)
    {
        Settings settings;
        const wxString defaultDir = settings.GetADBPath().GetPath();
    #if defined(__WXMSW__)
        const wxString exeWildcard = "*.exe";
    #else
        const wxString exeWildcard = wxFileSelectorDefaultWildcardStr;
    #endif
        wxFileDialog openFileDialog(this, "Locate Android Debug Bridge (ADB)", defaultDir, wxEmptyString, exeWildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if(openFileDialog.ShowModal() != wxID_CANCEL)
        {
            wxFileName adbFileName = wxFileName(openFileDialog.GetPath());
            if(adbFileName.IsFileExecutable())
            {
                settings.SetADBPath(adbFileName);
                m_adbPath->SetValue(adbFileName.GetFullPath());
            }
            else
            {
                wxMessageDialog dialog(this, adbFileName.GetFullPath() + " is not a valid executable file!", "Invalid Executable");
                dialog.ShowModal();
            }
        }
    }

    void SettingsPanel::onLocateRecordingsDir(wxCommandEvent &event)
    {
        Settings settings;
        const wxString defaultDir = settings.GetRecordingsDir().GetPath();
        wxDirDialog openDirDialog(this, "Set Recordings Directory", defaultDir, wxDD_NEW_DIR_BUTTON);
        if(openDirDialog.ShowModal() != wxID_CANCEL)
        {
            wxFileName recordingsDir = wxFileName::DirName(openDirDialog.GetPath());
            settings.SetRecordingsDir(recordingsDir);
            m_recordingsDir->SetValue(recordingsDir.GetFullPath());
        }
    }

    void SettingsPanel::onFrameBufferCompressionChanged(wxCommandEvent &event)
    {
        Settings().SetFrameBufferCompressionQuality((int)m_fbCompressionQuality->GetValue());
    }

} // namespace Monitor
} // namespace OVR
