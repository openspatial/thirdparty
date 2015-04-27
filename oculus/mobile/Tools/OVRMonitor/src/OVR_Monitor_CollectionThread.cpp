#include "OVR_Monitor_CollectionThread.h"
#include "OVR_Monitor_SessionData.h"

#include <wx/wfstream.h> // for wxFileOutputStream
#include <wx/zstream.h>  // for wxZlibOutputStream

namespace OVR
{
namespace Monitor
{

    /*******************
    * CollectionThread *
    *******************/

    CollectionThread::CollectionThread(void) :
        wxThread(wxTHREAD_JOINABLE)
    {
    }

    CollectionThread::~CollectionThread(void)
    {
    }

    /*************************
    * SocketCollectionThread *
    *************************/

    SocketCollectionThread::SocketCollectionThread(wxIPV4address &addr, wxFileName outFileName, SessionData &sessionData, Capture::UInt32 connectionFlags) :
        m_streamProcessor(sessionData),
        m_outFileName(outFileName),
        m_collectionFlags(connectionFlags)
    {
        printf("Attempting to connect to %s\n", addr.IPAddress().mb_str().data());

        m_socket = new wxSocketClient(wxSOCKET_BLOCK);
        m_socket->SetTimeout(2.0); // we don't want to hang forever if the connection is going to fail

        if(m_socket->Connect(addr))
        {
            printf("Connected!\n");
        }
        else
        {
            printf("Connection failed!\n");
            m_socket->Destroy();
            m_socket = NULL;
        }
    }

    SocketCollectionThread::~SocketCollectionThread(void)
    {
        printf("Socket Collection Thread Destroyed...\n");
        if(m_socket) m_socket->Destroy();
    }

    void SocketCollectionThread::Close(void)
    {
        if(m_socket) m_socket->Close();
    }

    void *SocketCollectionThread::Entry(void)
    {
        // Check to make sure the socket was successfully created in the first place...
        if(!m_socket)
            return NULL;

        printf("Socket Collection Thread starting...\n");

        printf("Performing handshake...\n");

        // Start by sending Client Request Header...
        Capture::ConnectionHeaderPacket clientHeader = {0};
        clientHeader.size    = sizeof(clientHeader);
        clientHeader.version = Capture::ConnectionHeaderPacket::s_version;
        clientHeader.flags   = m_collectionFlags;
        m_socket->Write(&clientHeader, sizeof(clientHeader));
        if(m_socket->Error() || m_socket->LastWriteCount() != sizeof(clientHeader))
        {
            printf("Socket Write Failure on handshake! Aborting!\n");
            return NULL;
        }
        
        // Read the response header from server...
        Capture::ConnectionHeaderPacket serverHeader = {0};
        m_socket->Read(&serverHeader, sizeof(serverHeader));
        if(m_socket->Error() || m_socket->LastReadCount() != sizeof(serverHeader))
        {
            printf("Socket Read Failure on handshake! Aborting!\n");
            return NULL;
        }

        // Check the header size...
        if(clientHeader.size != serverHeader.size)
        {
            printf("Header Size mismatch!\n");
            return NULL;
        }

        // Check version number...
        if(clientHeader.version != serverHeader.version)
        {
            printf("Version mismatch!\n");
            return NULL;
        }

        // Validate feature selection...
        if(!serverHeader.flags)
        {
            printf("No Features enabled!\n");
            // Not a fatal error though, so lets continue just in case something slips through the cracks...
        }

        // Read packet descriptors...
        Capture::PacketDescriptorHeaderPacket packetDescHeader = {0};
        m_socket->Read(&packetDescHeader, sizeof(packetDescHeader));
        if(m_socket->Error() || m_socket->LastReadCount() != sizeof(packetDescHeader))
        {
            printf("Failed to read Packet Descriptor Header! Aborting!\n");
            return NULL;
        }
        std::vector<Capture::PacketDescriptorPacket> packetDescs;
        for(Capture::UInt32 i=0; i<packetDescHeader.numPacketTypes; i++)
        {
            Capture::PacketDescriptorPacket packetDesc = {0};
            m_socket->Read(&packetDesc, sizeof(packetDesc));
            if(m_socket->Error() || m_socket->LastReadCount() != sizeof(packetDesc))
            {
                printf("Failed to read Packet Descriptor! Aborting!\n");
                return NULL;
            }
            packetDescs.push_back(packetDesc);
        }
        wxASSERT(packetDescs.size() == packetDescHeader.numPacketTypes);
        // TODO: initialize some sort of parser function pointer table based on the packet descriptor table
        // m_streamProcessor.InitPacketDescTable(packetDescs);

        printf("Starting Capture...\n");

        wxUint32  bufferSize      = 1*1024*1024;
        char     *buffer          = (char*)malloc(bufferSize);

        if(!m_outFileName.DirExists())
        {
            // Create parent directly if it doesn't exist...
            m_outFileName.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        }

        wxFileOutputStream fstream(m_outFileName.GetLongPath());
        wxZlibOutputStream zstream(fstream, wxZ_BEST_COMPRESSION, wxZLIB_ZLIB);

        // Write server header to output stream...
        zstream.Write(&serverHeader, sizeof(serverHeader));

        // Write packet descriptor table...
        zstream.Write(&packetDescHeader, sizeof(packetDescHeader));
        for(Capture::UInt32 i=0; i<packetDescHeader.numPacketTypes; i++)
        {
            zstream.Write(&packetDescs[i], sizeof(packetDescs[i]));
        }

        while(m_socket && m_socket->IsConnected() && !TestDestroy())
        {
            // Read as much data as we can or until the buffer is full....
            m_socket->Read(buffer, bufferSize);
            const wxUint32 bytesRead = m_socket->LastReadCount();
            if(bytesRead > 0)
            {
                // Data available... feed it into the stream processor!
                m_streamProcessor.ProcessData(buffer, bytesRead);
                // And push the data data into the compressed output stream...
                zstream.Write(buffer, bytesRead);
            }
            if(m_socket->Error())
            {
                printf("Socket Error: Disconnected...\n");
                break;
            }
        }

        free(buffer);

        // Signal that no more data will be received...
        m_streamProcessor.Close();

        // Make sure the socket is closed at the end of the thread...
        if(m_socket)
        {
            printf("Closing Connection...\n");
            m_socket->Close();
        }

        printf("Socket Collection Thread Ending Gracefully...\n");

        return NULL;
    }


    /***********************
    * FileCollectionThread *
    ***********************/

    FileCollectionThread::FileCollectionThread(wxFileName inFileName, SessionData &sessionData) :
        m_streamProcessor(sessionData),
        m_inFileName(inFileName)
    {
    }

    FileCollectionThread::~FileCollectionThread(void)
    {
    }

    void FileCollectionThread::Close(void)
    {
    }

    void *FileCollectionThread::Entry(void)
    {
        printf("File Collection Thread starting...\n");

        wxUint32  bufferSize      = 1*1024*1024;
        char     *buffer          = (char*)malloc(bufferSize);

        wxFileInputStream fstream(m_inFileName.GetLongPath());
        wxZlibInputStream zstream(fstream);

        printf("Checking Stream Header\n");

        // Read the bitstream header...
        Capture::ConnectionHeaderPacket serverHeader = {0};
        zstream.Read(&serverHeader, sizeof(serverHeader));
        if(zstream.LastRead() != sizeof(serverHeader))
        {
            printf("Header Read Error! Aborting!\n");
            return NULL;
        }

        // Check the header size...
        if(serverHeader.size != sizeof(serverHeader))
        {
            printf("Header Size mismatch!\n");
            return NULL;
        }

        // Check version number...
        if(serverHeader.version != Capture::ConnectionHeaderPacket::s_version)
        {
            printf("Version mismatch!\n");
            return NULL;
        }

        // Validate feature selection...
        if(!serverHeader.flags)
        {
            printf("No Features enabled!\n");
            // Not a fatal error though, so lets continue just in case something slips through the cracks...
        }

        // Read packet descriptors...
        Capture::PacketDescriptorHeaderPacket packetDescHeader = {0};
        zstream.Read(&packetDescHeader, sizeof(packetDescHeader));
        if(zstream.LastRead() != sizeof(packetDescHeader))
        {
            printf("Failed to read Packet Descriptor Header! Aborting!\n");
            return NULL;
        }
        std::vector<Capture::PacketDescriptorPacket> packetDescs;
        for(Capture::UInt32 i=0; i<packetDescHeader.numPacketTypes; i++)
        {
            Capture::PacketDescriptorPacket packetDesc = {0};
            zstream.Read(&packetDesc, sizeof(packetDesc));
            if(zstream.LastRead() != sizeof(packetDesc))
            {
                printf("Failed to read Packet Descriptor! Aborting!\n");
                return NULL;
            }
            packetDescs.push_back(packetDesc);
        }
        wxASSERT(packetDescs.size() == packetDescHeader.numPacketTypes);
        // TODO: initialize some sort of parser function pointer table based on the packet descriptor table
        // m_streamProcessor.InitPacketDescTable(packetDescs);

        printf("Starting Capture...\n");

        while(!zstream.Eof() && !TestDestroy())
        {
            zstream.Read(buffer, bufferSize);
            const size_t bytesRead = zstream.LastRead();
            if(bytesRead > 0)
            {
                m_streamProcessor.ProcessData(buffer, bytesRead);
            }
            else
            {
                printf("File Read Error!\n");
                break;
            }
        }

        free(buffer);

        // Signal that no more data will be received...
        m_streamProcessor.Close();

        printf("File Collection Thread Ending Gracefully...\n");

        return NULL;
    }


} // namespace Monitor
} // namespace OVR

