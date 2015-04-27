#include "OVR_Monitor_StreamProcessor.h"
#include "OVR_Monitor_SessionData.h"

namespace OVR
{
namespace Monitor
{

    static double timestampToSeconds(Capture::UInt64 nanoseconds)
    {
        return ((double)nanoseconds) * (1.0/1000000000.0);
    }



    StreamProcessor::StreamProcessor(SessionData &sessionData) :
        m_sessionData(sessionData)
    {
        m_streamThreadID       = 0;
        m_streamBytesRemaining = 0;
    }

    StreamProcessor::~StreamProcessor(void)
    {
    }

    bool StreamProcessor::ProcessData(const void *buffer, size_t bufferSize)
    {
        // First, append the incoming data to our unprocessed data buffer...
        m_buffer.insert(m_buffer.end(), (const char*)buffer, ((const char*)buffer)+bufferSize);

        const DataVector::iterator begin = m_buffer.begin();
        const DataVector::iterator end   = m_buffer.end();
        DataVector::iterator       curr  = begin;

        while(curr < end)
        {
            // Compute the end of the readable stream...
            // We take the minimum end point between end of buffer actually read in and stream size...
            // because we don't want to overrun the stream or the buffer that is actually read in...
            const DataVector::iterator streamEnd = curr + std::min<size_t>(std::distance(curr, end), m_streamBytesRemaining);

            // If we are currently in a stream... try parsing packets out of it...
            while(curr < streamEnd)
            {
                const size_t s = LoadAndProcessNextPacket(curr, streamEnd);
                if(!s)
                    break;
                wxASSERT(curr+s <= streamEnd);
                curr                   += s;
                m_streamBytesRemaining -= s;
            }

            // If we are at the end of the stream... that means the next data available will be a stream header...
            // so load that if we can. And once loaded stay in the loop and try parsing the following packets...
            if(curr == streamEnd && sizeof(Capture::StreamHeaderPacket) <= std::distance(curr, end))
            {
                Capture::StreamHeaderPacket streamHeader = {0};
                memcpy(&streamHeader, &*curr, sizeof(streamHeader));
                m_streamThreadID       = streamHeader.threadID;
                m_streamBytesRemaining = streamHeader.streamSize;
                curr += sizeof(streamHeader);
            }
            else
            {
                // If we ran out of parsable data we need to exit and allow the CaptureThread to load more data...
                break;
            }
        }

        // Finally, remove processed data...
        if(curr > begin)
        {
            // This might seem wasteful doing a memmove rather than a circular buffer... but
            // keep in mind this only ever occurs when only a partial packet is left in the buffer
            // so its typically a very tiny move.
            m_buffer.erase(begin, curr);
        }

        return true;
    }

    void StreamProcessor::Close(void)
    {
        m_sessionData.lock();
        m_sessionData.onConnectionClose();
        m_sessionData.unlock();
        m_buffer.clear();
        m_streamThreadID       = 0;
        m_streamBytesRemaining = 0;
    }

    void StreamProcessor::ProcessPacket(const Capture::ThreadNamePacket &packet, const void *payloadData, size_t payloadSize)
    {
        const wxString name = wxString((const char*)payloadData, payloadSize);
        m_sessionData.lock();
        m_sessionData.setThreadName(m_streamThreadID, name);
        m_sessionData.unlock();
    }

    void StreamProcessor::ProcessPacket(const Capture::LabelPacket &packet, const void *payloadData, size_t payloadSize)
    {
        const wxString name = wxString((const char*)payloadData, payloadSize);
        m_sessionData.lock();
        m_sessionData.setLabel(packet.labelID, name);
        m_sessionData.unlock();
    }

    void StreamProcessor::ProcessPacket(const Capture::FramePacket &packet)
    {
        const double seconds = timestampToSeconds(packet.timestamp);
        m_sessionData.lock();
        m_sessionData.pushFrame(seconds);
        m_sessionData.unlock();
    }

    void StreamProcessor::ProcessPacket(const Capture::VSyncPacket &packet)
    {
        const double seconds = timestampToSeconds(packet.timestamp);
        m_sessionData.lock();
        m_sessionData.pushVSync(seconds);
        m_sessionData.unlock();
    }

    void StreamProcessor::ProcessPacket(const Capture::CPUZoneEnterPacket &packet)
    {
        const double seconds = timestampToSeconds(packet.timestamp);
        m_sessionData.lock();
        m_sessionData.pushCPUZoneEnter(m_streamThreadID, packet.labelID, seconds);
        m_sessionData.unlock();
    }

    void StreamProcessor::ProcessPacket(const Capture::CPUZoneLeavePacket &packet)
    {
        const double seconds = timestampToSeconds(packet.timestamp);
        m_sessionData.lock();
        m_sessionData.pushCPUZoneLeave(m_streamThreadID, seconds);
        m_sessionData.unlock();
    }

    void StreamProcessor::ProcessPacket(const Capture::SensorRangePacket &packet)
    {
        m_sessionData.lock();
        m_sessionData.setSensorRangeData(packet.labelID, packet.minValue, packet.maxValue, (Capture::SensorInterpolator)packet.interpolator, (Capture::SensorUnits)packet.units);
        m_sessionData.unlock();
    }

    void StreamProcessor::ProcessPacket(const Capture::SensorPacket &packet)
    {
        const double seconds = timestampToSeconds(packet.timestamp);
        m_sessionData.lock();
        m_sessionData.pushSensorData(packet.labelID, seconds, packet.value);
        m_sessionData.unlock();
    }

    void StreamProcessor::ProcessPacket(const Capture::FrameBufferPacket &packet, const void *payloadData, size_t payloadSize)
    {
        wxImage framebuffer(packet.width, packet.height, false);
        // wx documentation states that wxImage::GetData() always returns 24-bit color.... R8G8B8
        // so making that assumption this code path ends up being >2x as fast as the old wxImage::SetRGB()
        // version.
        const unsigned short *srcCurr  = (const unsigned short*)payloadData;
        unsigned char        *dstBegin = framebuffer.GetData();
        unsigned char        *dstEnd   = dstBegin + (packet.width*packet.height*3);
        for(unsigned char *dstCurr=dstBegin; dstCurr<dstEnd; )
        {
            const unsigned short src = *(srcCurr++);
            *(dstCurr++) = (src>>8) & 0xF8; // RRRRRGGGGGGBBBBB >> 8 = 00000000RRRRRGGG & 11111000 = RRRRR000
            *(dstCurr++) = (src>>3) & 0xFC; // RRRRRGGGGGGBBBBB >> 3 = 000RRRRRGGGGGGBB & 11111100 = GGGGGG00
            *(dstCurr++) = (src<<3) & 0xF8; // RRRRRGGGGGGBBBBB << 3 = RRGGGGGGBBBBB000 & 11111000 = BBBBB000
        }
        const double seconds = timestampToSeconds(packet.timestamp);
        m_sessionData.lock();
        m_sessionData.pushFrameBuffer(seconds, framebuffer);
        m_sessionData.unlock();
    }

    void StreamProcessor::ProcessPacket(const Capture::LogPacket &packet, const void *payloadData, size_t payloadSize)
    {
        const double seconds = timestampToSeconds(packet.timestamp);
        m_sessionData.lock();
        m_sessionData.pushLog(m_streamThreadID, (Capture::LogPriority)packet.priority, seconds, wxString((const char*)payloadData, payloadSize));
        m_sessionData.unlock();
    }

    size_t StreamProcessor::LoadAndProcessNextPacket(DataVector::const_iterator curr, DataVector::const_iterator bufferEnd)
    {
        Capture::PacketHeader packetHeader;

        // Check to see if we have enough room for the header...
        if(curr + sizeof(packetHeader) > bufferEnd)
            return 0;

        // Peek at the header...
        memcpy(&packetHeader, &*curr, sizeof(packetHeader));
        curr += sizeof(packetHeader);

        // Load the packet payload if we can...
        switch(packetHeader.packetID)
        {
            case Capture::ThreadNamePacket::s_packetID:   return ProcessPacketWithPayload <Capture::ThreadNamePacket>   (curr, bufferEnd);
            case Capture::LabelPacket::s_packetID:        return ProcessPacketWithPayload <Capture::LabelPacket>        (curr, bufferEnd);
            case Capture::FramePacket::s_packetID:        return ProcessPacket            <Capture::FramePacket>        (curr, bufferEnd);
            case Capture::VSyncPacket::s_packetID:        return ProcessPacket            <Capture::VSyncPacket>        (curr, bufferEnd);
            case Capture::CPUZoneEnterPacket::s_packetID: return ProcessPacket            <Capture::CPUZoneEnterPacket> (curr, bufferEnd);
            case Capture::CPUZoneLeavePacket::s_packetID: return ProcessPacket            <Capture::CPUZoneLeavePacket> (curr, bufferEnd);
            case Capture::SensorRangePacket::s_packetID:  return ProcessPacket            <Capture::SensorRangePacket>  (curr, bufferEnd);
            case Capture::SensorPacket::s_packetID:       return ProcessPacket            <Capture::SensorPacket>       (curr, bufferEnd);
            case Capture::FrameBufferPacket::s_packetID:  return ProcessPacketWithPayload <Capture::FrameBufferPacket>  (curr, bufferEnd);
            case Capture::LogPacket::s_packetID:          return ProcessPacketWithPayload <Capture::LogPacket>          (curr, bufferEnd);
        }

        // We should never reach here...
        // typically means unknown packet type, so any further reading from the socket will yield undefined results...
        wxASSERT(0);
        return 0;
    }


} // namespace Monitor
} // namespace OVR
