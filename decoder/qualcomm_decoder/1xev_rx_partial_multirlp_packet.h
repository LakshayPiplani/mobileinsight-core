/* 
 * 1xEV Rx Partial MultiRLP Packet
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt _1xEVRxPartialMultiRLPPacket_Fmt [] = {
    {UINT, "RLP Flow", 1},
    {PLACEHOLDER, "Sequence Length", 0},
    {UINT, "Header Length", 1},
    {UINT, "Number Of Partial Packets", 1},
};

const Fmt _1xEVRxPartialMultiRLPPacket_Packet [] = {
    {UINT, "Physical Channel Rate", 4},
    {PLACEHOLDER, "Channel", 0},
    {PLACEHOLDER, "Payload Length", 0},
    {PLACEHOLDER, "RLP ID Length", 0},
    {PLACEHOLDER, "Slot Time Reference", 0},
    {BYTE_STREAM, "RLP Payload", 6},
};

const ValueName _1xEVRxPartialMultiRLPPacket_Packet_DataRate [] = {
    {0, "Rate not determined"},
    {1, "38.4 Kbps"},
    {2, "76.8 Kbps"},
    {3, "153.6 Kbps"},
    {4, "307.2 short Kbps"},
    {5, "307.2 long Kbps"},
    {6, "614.4 short Kbps"},
    {7, "614.4 long Kbps"},
    {8, "921.6 Kbps"},
    {9, "1.2288 short Mbps"},
    {10, "1.2288 long Mbps"},
    {11, "1.843 Mbps"},
    {12, "2.456 Mbps"},
    {13, "1.536 Mbps"}, // weird
    {14, "3.072 Mbps"},
    {15, "Invalid"},
};

const ValueName _1xEVRxPartialMultiRLPPacket_Packet_Channel [] = {
    {0, "FTC"},
    {1, "Control"},
};

static int _decode_1xev_rx_partial_multirlp_packet_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int start = offset;
    int iNonDecodeP1 = _search_result_int(result, "RLP Flow");
    int iRlpFlow = iNonDecodeP1 & 15; // last 4 bits
    int iSequenceLength = (iNonDecodeP1 >> 4) & 15; // next 4 bits
    _replace_result_int(result, "RLP Flow", iRlpFlow);
    int num_packet = _search_result_int(result, "Number Of Partial Packets");
    iSequenceLength = iSequenceLength * num_packet + 2;
    _replace_result_int(result, "Sequence Length", iSequenceLength);

    FieldList result_packet;
    for (int i = 0; i < num_packet; i++) {
        FieldList result_packet_item;
        offset += _decode_by_fmt(_1xEVRxPartialMultiRLPPacket_Packet,
                ARRAY_SIZE(_1xEVRxPartialMultiRLPPacket_Packet, Fmt),
                b, offset, length, result_packet_item);
        unsigned int iNonDecodeP2 = _search_result_uint(result_packet_item,
                "Physical Channel Rate");
        int iDataRate = iNonDecodeP2 & 15; // last 4 bits
        int iChannel = (iNonDecodeP2 >> 4) & 1; // next 1 bit
        int iPayloadLength = (iNonDecodeP2 >> 5) & 4095; // next 13 bits
        int iRlpIdLength = (iNonDecodeP2 >> 18) & 63; // next 6 bits
        int iSlotTimeReference = (iNonDecodeP2 >> 25) & 255; // next 8 bits
        _replace_result_int(result_packet_item,
                "Physical Channel Rate", iDataRate);
        (void) _map_result_field_to_name(result_packet_item,
                "Physical Channel Rate",
                _1xEVRxPartialMultiRLPPacket_Packet_DataRate,
                ARRAY_SIZE(_1xEVRxPartialMultiRLPPacket_Packet_DataRate,
                    ValueName),
                "(MI)Unknown");
        _replace_result_int(result_packet_item,
                "Channel", iChannel);
        (void) _map_result_field_to_name(result_packet_item,
                "Channel",
                _1xEVRxPartialMultiRLPPacket_Packet_Channel,
                ARRAY_SIZE(_1xEVRxPartialMultiRLPPacket_Packet_Channel,
                    ValueName),
                "(MI)Unknown");
        _replace_result_int(result_packet_item,
                "Payload Length", iPayloadLength);
        _replace_result_int(result_packet_item,
                "RLP ID Length", iRlpIdLength);
        _replace_result_int(result_packet_item,
                "Slot Time Reference", iSlotTimeReference);

        result_packet.push_back({"Ignored", FieldValue{std::move(result_packet_item)}, "dict"});
    }
    result.push_back({"Packets", FieldValue{std::move(result_packet)}, "list"});

    offset += num_packet;
    return offset - start;
};
