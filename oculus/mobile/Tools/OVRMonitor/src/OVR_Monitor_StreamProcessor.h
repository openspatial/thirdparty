#ifndef OVR_MONITOR_STREAMPROCESSOR_H
#define OVR_MONITOR_STREAMPROCESSOR_H

#include <OVR_Capture_LegacyPackets.h>
#include <wx/wx.h>
#include <vector>

namespace OVR
{
namespace Monitor
{

    class SessionData;

    class StreamProcessor
    {
        private:
            typedef std::vector<char> DataVector;

        public:
            StreamProcessor(SessionData &sessionData);
            ~StreamProcessor(void);

            bool ProcessData(const void *buffer, size_t bufferSize);

            void Close(void);

        private:
            void ProcessPacket(const Capture::ThreadNamePacket   &packet, const void *payloadData, size_t payloadSize);
            void ProcessPacket(const Capture::LabelPacket        &packet, const void *payloadData, size_t payloadSize);
            void ProcessPacket(const Capture::FramePacket        &packet);
            void ProcessPacket(const Capture::VSyncPacket        &packet);
            void ProcessPacket(const Capture::CPUZoneEnterPacket &packet);
            void ProcessPacket(const Capture::CPUZoneLeavePacket &packet);
            void ProcessPacket(const Capture::SensorRangePacket  &packet);
            void ProcessPacket(const Capture::SensorPacket       &packet);
            void ProcessPacket(const Capture::FrameBufferPacket  &packet, const void *payloadData, size_t payloadSize);
            void ProcessPacket(const Capture::LogPacket          &packet, const void *payloadData, size_t payloadSize);

            template<typename PacketType>
            size_t ProcessPacket(DataVector::const_iterator curr, DataVector::const_iterator bufferEnd)
            {
                OVR_CAPTURE_STATIC_ASSERT(PacketType::s_hasPayload == false);

                PacketType packet = {0};

                // Check to see if we have enough room for the packet...
                if(sizeof(packet) > std::distance(curr, bufferEnd))
                    return 0;

                // Load the packet...
                memcpy(&packet, &*curr, sizeof(packet));

                // Process...
                ProcessPacket(packet);

                // Return the total size of the packet, including the header...
                return sizeof(Capture::PacketHeader) + sizeof(packet);
            }

            template<typename PacketType>
            size_t ProcessPacketWithPayload(DataVector::const_iterator curr, DataVector::const_iterator bufferEnd)
            {
                OVR_CAPTURE_STATIC_ASSERT(PacketType::s_hasPayload == true);

                PacketType                           packet      = {0};
                typename PacketType::PayloadSizeType payloadSize = 0;

                // Check to see if we have enough room for the packet...
                if(sizeof(packet) > std::distance(curr, bufferEnd))
                    return 0;

                // Load the packet...
                memcpy(&packet, &*curr, sizeof(packet));
                curr += sizeof(packet);

                // Check to see if we have enough room for the payload header...
                if(sizeof(payloadSize) > std::distance(curr, bufferEnd))
                    return 0;

                // Load the payload header...
                memcpy(&payloadSize, &*curr, sizeof(payloadSize));
                curr += sizeof(payloadSize);

                // Check to see if we have enough room for the payload...
                if(payloadSize > (size_t)std::distance(curr, bufferEnd))
                    return 0;

                void *payload = NULL;
                if(payloadSize > 0)
                {
                    payload = malloc(payloadSize);
                    memcpy(payload, &*curr, payloadSize);
                }

                // Process...
                ProcessPacket(packet, payload, (size_t)payloadSize);

                if(payload)
                {
                    free(payload);
                }

                // Return the total size of the packet, including the header...
                return sizeof(Capture::PacketHeader) + sizeof(packet) + sizeof(payloadSize) + payloadSize;
            }

            size_t LoadAndProcessNextPacket(DataVector::const_iterator curr, DataVector::const_iterator bufferEnd);

        private:
            SessionData      &m_sessionData;

            DataVector        m_buffer;

            Capture::UInt32   m_streamThreadID;
            size_t            m_streamBytesRemaining;
    };

} // namespace Monitor
} // namespace OVR

#endif
