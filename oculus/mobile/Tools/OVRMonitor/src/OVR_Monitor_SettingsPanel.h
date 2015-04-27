#ifndef OVRSETTINGSPANEL_H
#define OVRSETTINGSPANEL_H

#include <wx/wx.h>

namespace OVR
{
namespace Monitor
{

    class SettingsPanel : public ::wxPanel
    {
        DECLARE_EVENT_TABLE()
        public:
            SettingsPanel(::wxWindow &parent);
            virtual ~SettingsPanel(void);

        private:
            void onLocateADB(wxCommandEvent &event);
            void onLocateRecordingsDir(wxCommandEvent &event);
            void onFrameBufferCompressionChanged(wxCommandEvent &event);

        private:
            enum
            {
                ID_FrameBufferCompressionSlider = wxID_HIGHEST + 100, // +100 to avoid collisions with Frame
                ID_LocateADBButton,
                ID_LocateRecordingsDirButton,
            };

        private:
            wxTextCtrl *m_adbPath;
            wxTextCtrl *m_recordingsDir;
            wxSlider   *m_fbCompressionQuality;
    };

} // namespace Monitor
} // namespace OVR

#endif
