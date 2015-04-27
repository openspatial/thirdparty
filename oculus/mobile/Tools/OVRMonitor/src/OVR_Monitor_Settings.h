#ifndef OVR_MONITOR_SETTINGS_H
#define OVR_MONITOR_SETTINGS_H

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/filename.h>

namespace OVR
{
namespace Monitor
{

    class Settings
    {
        public:
            Settings(void);
            ~Settings(void);

            wxFileName GetADBPath(void) const;
            void       SetADBPath(wxFileName path);

            wxFileName GetRecordingsDir(void) const;
            void       SetRecordingsDir(wxFileName filename);

            int        GetFrameBufferCompressionQuality(void) const;
            void       SetFrameBufferCompressionQuality(int quality);

        private:
            wxConfig m_config;
    };

} // namespace Monitor
} // namespace OVR

#endif
