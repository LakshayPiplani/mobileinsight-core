/*
 * LTE PDCP UL Cipher Data PDU
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt LtePdcpUlCipherDataPdu_Fmt [] = {
    {UINT, "Version", 1},
    {UINT, "Num Subpkts", 1},
    {SKIP, NULL, 2},
};

const Fmt LtePdcpUlCipherDataPdu_SubpktHeader_v1 [] = {
    {UINT, "Subpacket ID", 1},
    {UINT, "Subpacket Version", 1},
    {UINT, "Subpacket Size", 2},
};

const Fmt LtePdcpUlCipherDataPdu_SubpktPayload_v26 [] = {
    {SKIP, NULL, 16},
    {SKIP, NULL, 16},
    {UINT, "SRB Cipher Algorithm", 1},
    {UINT, "DRB Cipher Algorithm", 1},
    {UINT, "Num PDUs", 2},
};

const Fmt LtePdcpUlCipherDataPdu_SubpktPayload_v3 [] = {
    {UINT, "SRB Ciphering Keys", 16},
    {UINT, "DRB Ciphering Keys", 16},
    {UINT, "SRB Cipher Algorithm", 1},
    {UINT, "DRB Cipher Algorithm", 1},
    {UINT, "Num PDUs", 2},
};

const Fmt LtePdcpUlCipherDataPdu_SubpktPayload_v1 [] = {
    {SKIP, NULL, 16},
    {SKIP, NULL, 16},
    {UINT, "SRB Cipher Algorithm", 1},
    {UINT, "DRB Cipher Algorithm", 1},
    {UINT, "Num PDUs", 2},
};

const Fmt LtePdcpUlCipherDataPdu_Data_v26 [] = {
    {UINT, "Cfg Idx", 2},   // 6 bits
    {PLACEHOLDER, "Mode", 0},   // 1 bit
    {PLACEHOLDER, "SN Length", 0},  // 2 bits
    {PLACEHOLDER, "Bearer ID", 0},  // 5 bits
    {PLACEHOLDER, "Valid PDU", 0},  // 1 bit
    {UINT, "PDU Size", 2},
    {UINT, "Logged Bytes", 2},
    {UINT, "Sub FN", 2},    // 4 bits
    {PLACEHOLDER, "Sys FN", 0}, // 10 bits
    {UINT, "SN", 4},    // 12 bits
    {SKIP, NULL, 1},    // comporessed pdu, pdu type
};

const Fmt LtePdcpUlCipherDataPdu_Data_v3 [] = {
    {UINT, "Cfg Idx", 2},   // 6 bits
    {PLACEHOLDER, "Mode", 0},   // 1 bit
    {PLACEHOLDER, "SN Length", 0},  // 2 bits
    {PLACEHOLDER, "Bearer ID", 0},  // 5 bits
    {PLACEHOLDER, "Valid PDU", 0},  // 1 bit
    {UINT, "PDU Size", 2},
    {UINT, "Logged Bytes", 2},
    {UINT, "Sub FN", 2},    // 4 bits
    {PLACEHOLDER, "Sys FN", 0}, // 10 bits
    {UINT, "SN", 4},    // 12 bits
    {SKIP, NULL, 1},    // comporessed pdu, pdu type
};

const Fmt LtePdcpUlCipherDataPdu_Data_v1 [] = {
    {UINT, "Cfg Idx", 2},   // 6 bits
    {PLACEHOLDER, "Mode", 0},   // 1 bit
    {PLACEHOLDER, "SN Length", 0},  // 2 bits
    {PLACEHOLDER, "Bearer ID", 0},  // 5 bits
    {PLACEHOLDER, "Valid PDU", 0},  // 1 bit
    {UINT, "PDU Size", 2},
    {UINT, "Logged Bytes", 2},
    {UINT, "Sub FN", 2},    // 4 bits
    {PLACEHOLDER, "Sys FN", 0}, // 10 bits
    {UINT, "SN", 4},    // 12 bits
};

const Fmt LtePdcpUlCipherDataPdu_SubpktPayload_v40 [] = {
    {SKIP, NULL, 16},
    {SKIP, NULL, 16},
    {UINT, "SRB Cipher Algorithm", 1},
    {UINT, "DRB Cipher Algorithm", 1},
    {UINT, "Num PDUs", 2},
};

const Fmt LtePdcpUlCipherDataPdu_Data_v40 [] = {
    {UINT, "Cfg Idx", 2},   // 6 bits
    {PLACEHOLDER, "Mode", 0},   // 1 bit
    {PLACEHOLDER, "SN Length", 0},  // 2 bits
    {PLACEHOLDER, "Bearer ID", 0},  // 5 bits
    {PLACEHOLDER, "Valid PDU", 0},  // 1 bit
    {UINT, "PDU Size", 2},
    {UINT, "Logged Bytes", 2},
    {UINT, "Sub FN", 2},    // 4 bits
    {PLACEHOLDER, "Sys FN", 0}, // 10 bits
    {UINT, "count (hex)", 4},
    {SKIP, NULL, 1},     // compressed pdu, pdu type, buffer
};

static int _decode_lte_pdcp_ul_cipher_data_pdu_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int start = offset;
    int pkt_ver = _search_result_int(result, "Version");
    int n_subpkt = _search_result_int(result, "Num Subpkts");


    switch (pkt_ver) {
    case 1:
        {
            FieldList result_allpkts;
            for (int i = 0; i < n_subpkt; i++) {
                FieldList result_subpkt;
                int start_subpkt = offset;
                // decode subpacket header
                offset += _decode_by_fmt(LtePdcpUlCipherDataPdu_SubpktHeader_v1,
                        ARRAY_SIZE(LtePdcpUlCipherDataPdu_SubpktHeader_v1, Fmt),
                        b, offset, length, result_subpkt);
                int subpkt_id = _search_result_int(result_subpkt,
                        "Subpacket ID");
                int subpkt_ver = _search_result_int(result_subpkt,
                        "Subpacket Version");
                int subpkt_size = _search_result_int(result_subpkt,
                        "Subpacket Size");
                if (subpkt_id == 195 && subpkt_ver == 26) {
                    // PDCP PDU with Ciphering 0xC3
                    offset += _decode_by_fmt(
                            LtePdcpUlCipherDataPdu_SubpktPayload_v26,
                            ARRAY_SIZE(LtePdcpUlCipherDataPdu_SubpktPayload_v26, Fmt),
                            b, offset, length, result_subpkt);
                    (void) _map_result_field_to_name(result_subpkt, "SRB Cipher Algorithm",
                            ValueNameCipherAlgo,
                            ARRAY_SIZE(ValueNameCipherAlgo, ValueName),
                            "(MI)Unknown");
                    (void) _map_result_field_to_name(result_subpkt, "DRB Cipher Algorithm",
                            ValueNameCipherAlgo,
                            ARRAY_SIZE(ValueNameCipherAlgo, ValueName),
                            "(MI)Unknown");
                    int iNumPDUs = _search_result_int(result_subpkt,
                            "Num PDUs");

                    FieldList result_PDUs;
                    for (int j = 0; j < iNumPDUs; j++) {
                        FieldList result_pdu_item;
                        offset += _decode_by_fmt(LtePdcpUlCipherDataPdu_Data_v26,
                                ARRAY_SIZE(LtePdcpUlCipherDataPdu_Data_v26, Fmt),
                                b, offset, length, result_pdu_item);
                        int temp = _search_result_int(result_pdu_item,
                                "Cfg Idx");
                        int iCfgIdx = temp & 63;    // 6 bits
                        int iMode = (temp >> 6) & 1;    // 1 bit
                        int iSNLength = (temp >> 7) & 3;    // 2 bits
                        int iBearerId = (temp >> 9) & 31;   // 5 bits
                        int iValidPdu = (temp >> 14) & 1;   // 1 bit

                        _replace_result_int(result_pdu_item,
                                "Cfg Idx", iCfgIdx);
                        _replace_result_int(result_pdu_item,
                                "Mode", iMode);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "Mode",
                                ValueNamePdcpCipherDataPduMode,
                                ARRAY_SIZE(ValueNamePdcpCipherDataPduMode,
                                    ValueName),
                                "(MI)Unknown");
                        _replace_result_int(result_pdu_item,
                                "SN Length", iSNLength);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "SN Length",
                                ValueNamePdcpSNLength,
                                ARRAY_SIZE(ValueNamePdcpSNLength,
                                    ValueName),
                                "(MI)Unknown");
                        _replace_result_int(result_pdu_item,
                                "Bearer ID", iBearerId);
                        _replace_result_int(result_pdu_item,
                                "Valid PDU", iValidPdu);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "Valid PDU",
                                ValueNameYesOrNo,
                                ARRAY_SIZE(ValueNameYesOrNo,
                                    ValueName),
                                "(MI)Unknown");

                        temp = _search_result_int(result_pdu_item, "Sub FN");
                        int iSubFN = temp & 15; // 4 bits
                        int iSysFN = (temp >> 4) & 1023;    // 10 bits
                        _replace_result_int(result_pdu_item,
                                "Sub FN", iSubFN);
                        _replace_result_int(result_pdu_item,
                                "Sys FN", iSysFN);

                        temp = _search_result_int(result_pdu_item, "SN");
                        int iSN = temp & 4095;  // 12 bits
                        _replace_result_int(result_pdu_item,
                                "SN", iSN);

                        result_PDUs.push_back({"Ignored", FieldValue{std::move(result_pdu_item)}, "dict"});

                        int iLoggedBytes = _search_result_int(result_pdu_item,
                                "Logged Bytes");
                        offset += iLoggedBytes;

                    }
                    result_subpkt.push_back({"PDCPUL CIPH DATA", FieldValue{std::move(result_PDUs)}, "list"});
                }
                else if (subpkt_id == 195 && subpkt_ver == 3) {
                    // PDCP PDU with Ciphering 0xC3
                    offset += _decode_by_fmt(
                            LtePdcpUlCipherDataPdu_SubpktPayload_v3,
                            ARRAY_SIZE(LtePdcpUlCipherDataPdu_SubpktPayload_v3, Fmt),
                            b, offset, length, result_subpkt);
                    (void) _map_result_field_to_name(result_subpkt, "SRB Cipher Algorithm",
                            ValueNameCipherAlgo,
                            ARRAY_SIZE(ValueNameCipherAlgo, ValueName),
                            "(MI)Unknown");
                    (void) _map_result_field_to_name(result_subpkt, "DRB Cipher Algorithm",
                            ValueNameCipherAlgo,
                            ARRAY_SIZE(ValueNameCipherAlgo, ValueName),
                            "(MI)Unknown");
                    int iNumPDUs = _search_result_int(result_subpkt,
                            "Num PDUs");

                    FieldList result_PDUs;
                    for (int j = 0; j < iNumPDUs; j++) {
                        FieldList result_pdu_item;
                        offset += _decode_by_fmt(LtePdcpUlCipherDataPdu_Data_v3,
                                ARRAY_SIZE(LtePdcpUlCipherDataPdu_Data_v3, Fmt),
                                b, offset, length, result_pdu_item);
                        int temp = _search_result_int(result_pdu_item,
                                "Cfg Idx");
                        int iCfgIdx = temp & 63;    // 6 bits
                        int iMode = (temp >> 6) & 1;    // 1 bit
                        int iSNLength = (temp >> 7) & 3;    // 2 bits
                        int iBearerId = (temp >> 9) & 31;   // 5 bits
                        int iValidPdu = (temp >> 14) & 1;   // 1 bit

                        _replace_result_int(result_pdu_item,
                                "Cfg Idx", iCfgIdx);
                        _replace_result_int(result_pdu_item,
                                "Mode", iMode);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "Mode",
                                ValueNamePdcpCipherDataPduMode,
                                ARRAY_SIZE(ValueNamePdcpCipherDataPduMode,
                                    ValueName),
                                "(MI)Unknown");
                        _replace_result_int(result_pdu_item,
                                "SN Length", iSNLength);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "SN Length",
                                ValueNamePdcpSNLength,
                                ARRAY_SIZE(ValueNamePdcpSNLength,
                                    ValueName),
                                "(MI)Unknown");
                        _replace_result_int(result_pdu_item,
                                "Bearer ID", iBearerId);
                        _replace_result_int(result_pdu_item,
                                "Valid PDU", iValidPdu);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "Valid PDU",
                                ValueNameYesOrNo,
                                ARRAY_SIZE(ValueNameYesOrNo,
                                    ValueName),
                                "(MI)Unknown");

                        temp = _search_result_int(result_pdu_item, "Sub FN");
                        int iSubFN = temp & 15; // 4 bits
                        int iSysFN = (temp >> 4) & 1023;    // 10 bits
                        _replace_result_int(result_pdu_item,
                                "Sub FN", iSubFN);
                        _replace_result_int(result_pdu_item,
                                "Sys FN", iSysFN);

                        temp = _search_result_int(result_pdu_item, "SN");
                        int iSN = temp & 4095;  // 12 bits
                        _replace_result_int(result_pdu_item,
                                "SN", iSN);

                        result_PDUs.push_back({"Ignored", FieldValue{std::move(result_pdu_item)}, "dict"});

                        int iLoggedBytes = _search_result_int(result_pdu_item,
                                "Logged Bytes");
                        offset += iLoggedBytes;

                    }
                    result_subpkt.push_back({"PDCPUL CIPH DATA", FieldValue{std::move(result_PDUs)}, "list"});
                }
                else if (subpkt_id == 195 && subpkt_ver == 1) {
                    // PDCP PDU with Ciphering 0xC3
                    offset += _decode_by_fmt(
                            LtePdcpUlCipherDataPdu_SubpktPayload_v1,
                            ARRAY_SIZE(LtePdcpUlCipherDataPdu_SubpktPayload_v1, Fmt),
                            b, offset, length, result_subpkt);
                    (void) _map_result_field_to_name(result_subpkt, "SRB Cipher Algorithm",
                            ValueNameCipherAlgo,
                            ARRAY_SIZE(ValueNameCipherAlgo, ValueName),
                            "(MI)Unknown");
                    (void) _map_result_field_to_name(result_subpkt, "DRB Cipher Algorithm",
                            ValueNameCipherAlgo,
                            ARRAY_SIZE(ValueNameCipherAlgo, ValueName),
                            "(MI)Unknown");
                    int iNumPDUs = _search_result_int(result_subpkt,
                            "Num PDUs");

                    FieldList result_PDUs;
                    for (int j = 0; j < iNumPDUs; j++) {
                        FieldList result_pdu_item;
                        offset += _decode_by_fmt(LtePdcpUlCipherDataPdu_Data_v1,
                                ARRAY_SIZE(LtePdcpUlCipherDataPdu_Data_v1, Fmt),
                                b, offset, length, result_pdu_item);
                        int temp = _search_result_int(result_pdu_item,
                                "Cfg Idx");
                        int iCfgIdx = temp & 63;    // 6 bits
                        int iMode = (temp >> 6) & 1;    // 1 bit
                        int iSNLength = (temp >> 7) & 3;    // 2 bits
                        int iBearerId = (temp >> 9) & 31;   // 5 bits
                        int iValidPdu = (temp >> 14) & 1;   // 1 bit

                        _replace_result_int(result_pdu_item,
                                "Cfg Idx", iCfgIdx);
                        _replace_result_int(result_pdu_item,
                                "Mode", iMode);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "Mode",
                                ValueNamePdcpCipherDataPduMode,
                                ARRAY_SIZE(ValueNamePdcpCipherDataPduMode,
                                    ValueName),
                                "(MI)Unknown");
                        _replace_result_int(result_pdu_item,
                                "SN Length", iSNLength);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "SN Length",
                                ValueNamePdcpSNLength,
                                ARRAY_SIZE(ValueNamePdcpSNLength,
                                    ValueName),
                                "(MI)Unknown");
                        _replace_result_int(result_pdu_item,
                                "Bearer ID", iBearerId);
                        _replace_result_int(result_pdu_item,
                                "Valid PDU", iValidPdu);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "Valid PDU",
                                ValueNameYesOrNo,
                                ARRAY_SIZE(ValueNameYesOrNo,
                                    ValueName),
                                "(MI)Unknown");

                        temp = _search_result_int(result_pdu_item, "Sub FN");
                        int iSubFN = temp & 15; // 4 bits
                        int iSysFN = (temp >> 4) & 1023;    // 10 bits
                        _replace_result_int(result_pdu_item,
                                "Sub FN", iSubFN);
                        _replace_result_int(result_pdu_item,
                                "Sys FN", iSysFN);

                        temp = _search_result_int(result_pdu_item, "SN");
                        int iSN = temp & 4095;  // 12 bits
                        _replace_result_int(result_pdu_item,
                                "SN", iSN);

                        result_PDUs.push_back({"Ignored", FieldValue{std::move(result_pdu_item)}, "dict"});

                        int iLoggedBytes = _search_result_int(result_pdu_item,
                                "Logged Bytes");
                        offset += iLoggedBytes;

                    }
                    result_subpkt.push_back({"PDCPUL CIPH DATA", FieldValue{std::move(result_PDUs)}, "list"});


                }
                else if (subpkt_id == 195 && subpkt_ver == 40) {
                    // PDCP PDU with Ciphering 0xC3
                    offset += _decode_by_fmt(
                            LtePdcpUlCipherDataPdu_SubpktPayload_v40,
                            ARRAY_SIZE(LtePdcpUlCipherDataPdu_SubpktPayload_v40, Fmt),
                            b, offset, length, result_subpkt);
                    (void) _map_result_field_to_name(result_subpkt, "SRB Cipher Algorithm",
                            ValueNameCipherAlgo,
                            ARRAY_SIZE(ValueNameCipherAlgo, ValueName),
                            "(MI)Unknown");
                    (void) _map_result_field_to_name(result_subpkt, "DRB Cipher Algorithm",
                            ValueNameCipherAlgo,
                            ARRAY_SIZE(ValueNameCipherAlgo, ValueName),
                            "(MI)Unknown");
                    int iNumPDUs = _search_result_int(result_subpkt,
                            "Num PDUs");

                    FieldList result_PDUs;
                    for (int j = 0; j < iNumPDUs; j++) {
                        FieldList result_pdu_item;
                        offset += _decode_by_fmt(LtePdcpUlCipherDataPdu_Data_v40,
                                ARRAY_SIZE(LtePdcpUlCipherDataPdu_Data_v40, Fmt),
                                b, offset, length, result_pdu_item);
                        int temp = _search_result_int(result_pdu_item,
                                "Cfg Idx");
                        int iCfgIdx = temp & 63;    // 6 bits
                        int iMode = (temp >> 6) & 1;    // 1 bit
                        int iSNLength = (temp >> 7) & 3;    // 2 bits
                        int iBearerId = (temp >> 9) & 31;   // 5 bits
                        int iValidPdu = (temp >> 14) & 1;   // 1 bit

                        _replace_result_int(result_pdu_item,
                                "Cfg Idx", iCfgIdx);
                        _replace_result_int(result_pdu_item,
                                "Mode", iMode);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "Mode",
                                ValueNamePdcpCipherDataPduMode,
                                ARRAY_SIZE(ValueNamePdcpCipherDataPduMode,
                                    ValueName),
                                "(MI)Unknown");
                        _replace_result_int(result_pdu_item,
                                "SN Length", iSNLength);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "SN Length",
                                ValueNamePdcpSNLength,
                                ARRAY_SIZE(ValueNamePdcpSNLength,
                                    ValueName),
                                "(MI)Unknown");
                        _replace_result_int(result_pdu_item,
                                "Bearer ID", iBearerId);
                        _replace_result_int(result_pdu_item,
                                "Valid PDU", iValidPdu);
                        (void) _map_result_field_to_name(result_pdu_item,
                                "Valid PDU",
                                ValueNameYesOrNo,
                                ARRAY_SIZE(ValueNameYesOrNo,
                                    ValueName),
                                "(MI)Unknown");

                        temp = _search_result_int(result_pdu_item, "Sub FN");
                        int iSubFN = temp & 15; // 4 bits
                        int iSysFN = (temp >> 4) & 1023;    // 10 bits
                        _replace_result_int(result_pdu_item,
                                "Sub FN", iSubFN);
                        _replace_result_int(result_pdu_item,
                                "Sys FN", iSysFN);

                        result_PDUs.push_back({"Ignored", FieldValue{std::move(result_pdu_item)}, "dict"});

                        int iLoggedBytes = _search_result_int(result_pdu_item,
                                "Logged Bytes");
                        offset += iLoggedBytes;

                    }
                    result_subpkt.push_back({"PDCPUL CIPH DATA", FieldValue{std::move(result_PDUs)}, "list"});
                }
                else {
                    printf("(MI)Unknown LTE PDCP UL Cipher Data PDU subpkt id and version:"
                            " 0x%x - %d\n", subpkt_id, subpkt_ver);
                }
                result_allpkts.push_back({"Ignored", FieldValue{std::move(result_subpkt)}, "dict"});
                offset += subpkt_size - (offset - start_subpkt);
            }
            result.push_back({"Subpackets", FieldValue{std::move(result_allpkts)}, "list"});
            return offset - start;
        }
    default:
        printf("(MI)Unknown LTE PDCP UL Cipher Data PDU Packet version: %d\n",
                pkt_ver);
        return 0;
    }
}
