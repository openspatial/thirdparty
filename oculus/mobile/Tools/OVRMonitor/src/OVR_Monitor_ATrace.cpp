#include "OVR_Monitor_ATrace.h"
#include "OVR_Monitor_Settings.h"

#include <stdio.h> // for popen() used in ThreadSafeProcess

namespace OVR
{
namespace Monitor
{

    // wxProcess isn't thread safe... so we created our own!
    // TODO: Windows Support!!!
    class ThreadSafeProcess
    {
        public:
            static ThreadSafeProcess *Open(const wxString &cmd)
            {
                ThreadSafeProcess *process = NULL;
            #if defined(__WXMSW__)
                // TODO!!!!
            #else
                FILE *p = popen(cmd.ToAscii(), "r");
                wxASSERT(p);
                if(p)
                {
                    process = new ThreadSafeProcess();
                    process->m_pipe = p;
                }
            #endif
                wxPrintf("ThreadSafeProcess::Open(%s) = %p\n", cmd, process);
                return process;
            }

            static int Exec(const wxString &cmd)
            {
            #if defined(__WXMSW__)
                // TODO!!!!
                return -1;
            #else
                int ret = system(cmd.ToAscii());
                wxPrintf("ThreadSafeProcess::Exec(%s) = %d\n", cmd, ret);
                return ret;
            #endif
            }

        public:
            void Release(void)
            {
                wxPrintf("ThreadSafeProcess::Release()\n");
                delete this;
            }

            size_t Read(void *buffer, size_t bufferSize)
            {
            #if defined(__WXMSW__)
                return 0;
            #else
                return fread(buffer, 1, bufferSize, m_pipe);
            #endif
            }
    
        private:
            ThreadSafeProcess(void)
            {
            }

            ~ThreadSafeProcess(void)
            {
            #if defined(__WXMSW__)
            
            #else
                if(m_pipe) pclose(m_pipe);
            #endif
            }

        private:
        #if defined(__WXMSW__)
            
        #else
            FILE *m_pipe;
        #endif
    };


    enum ATraceTag
    {
        ATraceTag_Never            = 0,
        ATraceTag_Always           = (1<<0),
        ATraceTag_Graphics         = (1<<1),
        ATraceTag_Input            = (1<<2),
        ATraceTag_View             = (1<<3),
        ATraceTag_WebView          = (1<<4),
        ATraceTag_WindowManager    = (1<<5),
        ATraceTag_ActivityManager  = (1<<6),
        ATraceTag_SyncManager      = (1<<7),
        ATraceTag_Audio            = (1<<8),
        ATraceTag_Video            = (1<<9),
    };

    // Android Trace File Paths...
    static const char *g_traceClockPath             = "/sys/kernel/debug/tracing/trace_clock";
  //static const char *g_traceBufferSizePath        = "/sys/kernel/debug/tracing/buffer_size_kb";
  //static const char *g_tracingOverwriteEnablePath = "/sys/kernel/debug/tracing/options/overwrite";
  //static const char *g_currentTracerPath          = "/sys/kernel/debug/tracing/current_tracer";
    static const char *g_printTgidPath              = "/sys/kernel/debug/tracing/options/print-tgid";
  //static const char *g_funcgraphAbsTimePath       = "/sys/kernel/debug/tracing/options/funcgraph-abstime";
  //static const char *g_funcgraphCpuPath           = "/sys/kernel/debug/tracing/options/funcgraph-cpu";
  //static const char *g_funcgraphProcPath          = "/sys/kernel/debug/tracing/options/funcgraph-proc";
  //static const char *g_funcgraphFlatPath          = "/sys/kernel/debug/tracing/options/funcgraph-flat";
  //static const char *g_funcgraphDurationPath      = "/sys/kernel/debug/tracing/options/funcgraph-duration";
  //static const char *g_ftraceFilterPath           = "/sys/kernel/debug/tracing/set_ftrace_filter";
    static const char *g_tracingOnPath              = "/sys/kernel/debug/tracing/tracing_on";
  //static const char *g_tracePath                  = "/sys/kernel/debug/tracing/trace";
    static const char *g_tracePipePath              = "/sys/kernel/debug/tracing/trace_pipe";

    // Android System Properties...
    static const char *g_traceTagsProperty          = "debug.atrace.tags.enableflags";
  //static const char *g_traceAppCmdlineProperty    = "debug.atrace.app_cmdlines";


    ATraceThread::ATraceThread(SessionData &sessionData) :
        wxThread(wxTHREAD_JOINABLE)
    {
    }

    ATraceThread::~ATraceThread(void)
    {
    }

    void *ATraceThread::Entry(void)
    {
        const wxFileName adbFileName      = Settings().GetADBPath();
        const wxString   clockCmd         = adbFileName.GetFullPath() + wxString(" shell echo global \\> ") + wxString(g_traceClockPath);
        const wxString   tgidCmd          = adbFileName.GetFullPath() + wxString(" shell echo 0 \\> ")      + wxString(g_printTgidPath);
        const wxString   traceOnCmd       = adbFileName.GetFullPath() + wxString(" shell echo 1 \\> ")      + wxString(g_tracingOnPath);
        const wxString   traceOffCmd      = adbFileName.GetFullPath() + wxString(" shell echo 0 \\> ")      + wxString(g_tracingOnPath);
        const wxString   tracePropsOnCmd  = adbFileName.GetFullPath() + wxString(" shell setprop ")         + wxString(g_traceTagsProperty) + wxString::Format(" 0x%x", (unsigned int)(ATraceTag_Graphics | ATraceTag_Always));
        const wxString   tracePropsOffCmd = adbFileName.GetFullPath() + wxString(" shell setprop ")         + wxString(g_traceTagsProperty) + wxString::Format(" 0x%x", (unsigned int)(ATraceTag_Never));
        const wxString   tracePipeCmd     = adbFileName.GetFullPath() + wxString(" shell cat ")             + wxString(g_tracePipePath);

        // Scratch buffer for reading process output stream...
        static const size_t bufferSize = 128;
        char buffer[bufferSize];

        // Data pending processing...
        DataVector pendingData;

        // Enable tracing...
        ThreadSafeProcess::Exec(clockCmd);
        ThreadSafeProcess::Exec(tgidCmd);
        ThreadSafeProcess::Exec(tracePropsOnCmd);
        ThreadSafeProcess::Exec(traceOnCmd);

        ThreadSafeProcess *tracePipeProc = ThreadSafeProcess::Open(tracePipeCmd);
        while(tracePipeProc && !TestDestroy())
        {
            // Read pending data from the pipe...
            const size_t readCount = tracePipeProc->Read(buffer, bufferSize);
            if(readCount > 0)
            {
                // append read data...
                pendingData.insert(pendingData.end(), buffer, buffer+readCount);
            }

            // Process pending data...
            DataVector::iterator begin = pendingData.begin();
            DataVector::iterator end   = pendingData.end();
            for(DataVector::iterator curr=begin; curr<end; curr++)
            {
                if(*curr == '\n')
                {
                    const wxString line(&*begin, std::distance(begin, curr));
                    begin = curr+1;
                    ProcessLine(line);
                }
            }
            if(begin > pendingData.begin())
            {
                pendingData.erase(pendingData.begin(), begin);
            }

            if(readCount < bufferSize)
            {
                // end of file...
                // http://pubs.opengroup.org/onlinepubs/009695399/functions/fread.html
                // "Upon successful completion, fread() shall return the number of elements successfully read which is less than nitems only if a read error or end-of-file is encountered."
                wxPrintf("Process closed unexpectedly...\n");
                break;
            }
        }
        if(tracePipeProc) tracePipeProc->Release();

        // Turn off tracing...
        ThreadSafeProcess::Exec(traceOffCmd);
        ThreadSafeProcess::Exec(tracePropsOffCmd);

        return NULL;
    }

    void ATraceThread::ProcessLine(const wxString &line)
    {
        wxPrintf("ATrace: %s\n", line);
    }

} // namespace Monitor
} // namespace OVR
