#include "OVR_Monitor_Settings.h"

#include <wx/config.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace OVR
{
namespace Monitor
{

    Settings::Settings(void) :
        m_config("OculusRemoteMonitor")
    {
        // Initialize defaults...
        if(!GetADBPath().IsFileExecutable())
        {
            wxFileName adbFileName;
            wxString   homeVar = wxGetenv("ANDROID_HOME");
            wxString   adbVar  = wxGetenv("ADB");
            if(!adbFileName.IsFileExecutable() && !adbVar.IsEmpty())
            {
                adbFileName = wxFileName(adbVar);
            }
            if(!adbFileName.IsFileExecutable() && !homeVar.IsEmpty())
            {
                adbFileName = wxFileName(homeVar + "/platform-tools/adb");
            }
            if(adbFileName.IsFileExecutable())
            {
                SetADBPath(adbFileName);
            }
        }
        if(!GetRecordingsDir().IsOk())
        {
            wxFileName recordingsDir;
            recordingsDir.AssignDir(wxStandardPaths::Get().GetAppDocumentsDir());
            recordingsDir.AppendDir("OVRMonitorRecordings");
            SetRecordingsDir(recordingsDir);
        }
        if(GetFrameBufferCompressionQuality() < 0)
        {
            SetFrameBufferCompressionQuality(80);
        }
    }

    Settings::~Settings(void)
    {
    }

    wxFileName Settings::GetADBPath(void) const
    {
        wxString raw;
        if(m_config.Read("ADBPath", &raw))
            return wxFileName(raw);
        return wxFileName();
    }

    void Settings::SetADBPath(wxFileName path)
    {
        wxASSERT(path.IsFileExecutable());
        m_config.Write("ADBPath", path.GetFullPath());
    }

    wxFileName Settings::GetRecordingsDir(void) const
    {
        wxString raw;
        if(m_config.Read("RecordingsDir", &raw))
            return wxFileName::DirName(raw);
        return wxFileName();
    }

    void Settings::SetRecordingsDir(wxFileName filename)
    {
        m_config.Write("RecordingsDir", filename.GetPath());
    }

    int Settings::GetFrameBufferCompressionQuality(void) const
    {
        return (int)m_config.ReadLong("FrameBufferCompressionQuality", -1);
    }

    void Settings::SetFrameBufferCompressionQuality(int quality)
    {
        m_config.Write("FrameBufferCompressionQuality", (long)quality);
    }

} // namespace Monitor
} // namespace OVR
