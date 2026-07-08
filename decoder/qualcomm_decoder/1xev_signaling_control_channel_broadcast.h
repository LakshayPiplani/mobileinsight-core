/*
 * 1xev_signaling_control_channel_broadcast.h
 */

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <cstring>

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt _1xEVSignalingFmt [] = {
    {UINT, "Band", 1},
    {UINT, "Channel Number", 2},
    {UINT, "Pilot PN", 2},
    {UINT, "HSTR", 2},
    {SKIP, NULL, 4}, // Unknown
    {UINT, "Protocol Type", 1},
    {UINT, "Message ID", 1},
    // Total 13 bytes
};

const Fmt _1xEVSignaling_SectorParameters [] = {
    {UINT_BIG_ENDIAN, "Country Code", 2}, // 12 bits
    // 4 bits remaining
    {UINT_BIG_ENDIAN, "Subnet ID", 4},
    {UINT_BIG_ENDIAN, "Subnet ID[2]", 4},
    {UINT_BIG_ENDIAN, "Subnet ID[3]", 4},
    {UINT_BIG_ENDIAN, "Sector ID", 4}, // 4 bits for subnet ID, 24 bits for sector id
    // 4 bits remaining
    {UINT_BIG_ENDIAN, "Subnet Mask", 1}, // 4+4 bits
    // 4 bits remaining
    {UINT_BIG_ENDIAN, "Sector Signature", 2}, // 4 + 12 bits
    // 4 bits remaining
    {UINT_BIG_ENDIAN, "Latitude", 4},    // 4 + 18 bits
    // 14 bits remaining
    {UINT_BIG_ENDIAN, "Longitude", 2},  // 14 + 9 bits
    // 7 bits remaining
};

const Fmt _1xEVSignaling_OtherRATNeighborList [] = {
    {PLACEHOLDER, "Other RAT Signature", 0},    // 6 bits
    {PLACEHOLDER, "Num Other RAT", 0},          // 4 bits
};

const Fmt _1xEVSignaling_RATFmt [] = {
    {PLACEHOLDER, "RAT Type", 0},               // 6 bits
    {PLACEHOLDER, "RAT Record Length", 0},      // 8 bits
};

const Fmt _1xEVSignaling_LteFmt [] = {
    // ServPriorityIncluded 1 bit
    {PLACEHOLDER, "Serv Priority", 0},          // 3 bits
    {PLACEHOLDER, "ThreshServ", 0},             // 6 bits
    // perEarfcnParamsIncluded 1 bit
    // maxReselectionTimerIncluded 1 bit
    {PLACEHOLDER, "MaxReselectionTimer", 0},    // 4 bits
    // SearchBackOffTimerIncluded 1 bit
    {PLACEHOLDER, "SearchBackOffTimer", 0},     // Unknown
    // PLMNIDIncluded 1 bit
    {PLACEHOLDER, "NumEUTRAFreq", 0},           // 3 bits
};

const Fmt _1xEVSignaling_EUTRAFreqFmt [] = {
    {PLACEHOLDER, "EARFCN", 0},                 // 16 bits
    {PLACEHOLDER, "EARFCNPriority", 0},         // 3 bits
    {PLACEHOLDER, "ThreshX", 0},                // 4 bits
    {PLACEHOLDER, "RxLevMinEUTRA", 0},          // 8 bits
    {PLACEHOLDER, "PeMax", 0},                  // 6 bits
    {PLACEHOLDER, "RxLevMinOffset", 0},         // Unknown
    {PLACEHOLDER, "MeasurementBandWidth", 0},   // 3 bits
};

static int _decode_1xev_signaling_control_channel_broadcast (const char *b,
        int offset, size_t length, FieldList &result) {
    int iMessageID = _search_result_int(result, "Message ID");
    int iProtocolType = _search_result_int(result, "Protocol Type");

    if (iProtocolType == 15 && iMessageID == 1) {
        // Sector Parameters

        // Only support for System Parameters Msg
        offset += _decode_by_fmt(_1xEVSignaling_SectorParameters,
                ARRAY_SIZE(_1xEVSignaling_SectorParameters, Fmt),
                b, offset, length, result);

        unsigned int utemp = _search_result_int(result, "Country Code");
        int iCountryCode = ((utemp >> 12) & 15) * 100;
        iCountryCode += ((utemp >> 8) & 15) * 10;
        iCountryCode += ((utemp >> 4) & 15) * 1;
        _replace_result_int(result, "Country Code", iCountryCode);

        char hex[10] = {};
        std::string strSubnetID = "0x";
        sprintf(hex, "%02x", utemp & 15);
        strSubnetID += hex;
        utemp = _search_result_uint(result, "Subnet ID");
        for (int k = 0; k < 4; k++) {
            sprintf(hex, "%02x", (utemp >> ((3-k) * 8)) & 255);
            strSubnetID += hex;
        }
        utemp = _search_result_uint(result, "Subnet ID[2]");
        for (int k = 0; k < 4; k++) {
            sprintf(hex, "%02x", (utemp >> ((3-k) * 8)) & 255);
            strSubnetID += hex;
        }
        utemp = _search_result_uint(result, "Subnet ID[3]");
        for (int k = 0; k < 4; k++) {
            sprintf(hex, "%02x", (utemp >> ((3-k) * 8)) & 255);
            strSubnetID += hex;
        }
        utemp = _search_result_uint(result, "Sector ID");
        sprintf(hex, "%02x", (utemp >> 28) & 15);
        strSubnetID += hex;
        FieldValue pystr = FieldValue{std::string(strSubnetID.c_str())};
        _replace_result(result, "Subnet ID", pystr);

        int iSectorID = (utemp >> 4) & 16777215;
        _replace_result_int(result, "Sector ID", iSectorID);

        int iSubnetMask = (utemp & 15) * 16;
        utemp = _search_result_int(result, "Subnet Mask");
        iSubnetMask += (utemp >> 4) & 15;
        _replace_result_int(result, "Subnet Mask", iSubnetMask);

        int iSectorSignature = (utemp & 15) * 4096;
        utemp = _search_result_int(result, "Sector Signature");
        iSectorSignature += (utemp >> 4) & 4095;
        _replace_result_int(result, "Sector Signature", iSectorSignature);

        int iLatitude = (utemp & 15) * 262144;
        utemp = _search_result_uint(result, "Latitude");
        iLatitude += (utemp >> 14) & 262143;
        if (((iLatitude >> 21) & 1) == 1) {
            iLatitude = iLatitude - 4194304;
        }
        float fLatitude = iLatitude * 1.0 / 14400;
        FieldValue pyfloat = FieldValue{double(fLatitude)};
        _replace_result(result, "Latitude", pyfloat);

        int iLongitude = (utemp & 16383) * 512;
        utemp = _search_result_uint(result, "Longitude");
        iLongitude += (utemp >> 7) & 511;
        if (((iLongitude >> 22) & 1) == 1) {
            iLongitude = iLongitude - 8388608;
        }
        float fLongitude = iLongitude * 1.0 / 14400;
        _replace_result(result, "Longitude", FieldValue{double(fLongitude)});

        return offset;

    } else if (iProtocolType == 15 && iMessageID == 3) {
        // OtherRATNeighborList
        _decode_by_fmt(_1xEVSignaling_OtherRATNeighborList,
                ARRAY_SIZE(_1xEVSignaling_OtherRATNeighborList, Fmt),
                b, offset, length, result);
        const char *p = b + offset;
        int bitOffset = 0;
        int iOtherRATSignature = _decode_by_bit(bitOffset, 6, p);
        bitOffset += 6;
        _replace_result_int(result, "Other RAT Signature",
                iOtherRATSignature);
        int iNumOtherRAT = _decode_by_bit(bitOffset, 4, p);
        bitOffset += 4;
        _replace_result_int(result, "Num Other RAT",
                iNumOtherRAT);

        FieldList result_otherRATs;
        for (int i = 0; i < iNumOtherRAT; i++) {
            FieldList result_otherRAT_item;
            _decode_by_fmt(_1xEVSignaling_RATFmt,
                    ARRAY_SIZE(_1xEVSignaling_RATFmt, Fmt),
                    b, offset, length, result_otherRAT_item);

            int iRATType = _decode_by_bit(bitOffset, 6, p);
            bitOffset += 6;
            int iRATRecordLength = _decode_by_bit(bitOffset, 8, p);
            bitOffset += 8;
            _replace_result_int(result_otherRAT_item, "RAT Type",
                    iRATType);
            _replace_result_int(result_otherRAT_item,
                    "RAT Record Length", iRATRecordLength);
            int bitOffsetSave = bitOffset;

            if (iRATType == 0) {
                _decode_by_fmt(_1xEVSignaling_LteFmt,
                        ARRAY_SIZE(_1xEVSignaling_LteFmt, Fmt),
                        b, offset, length, result_otherRAT_item);

                int iServPriorityIncluded = _decode_by_bit(bitOffset, 1, p);
                bitOffset += 1;
                if (iServPriorityIncluded == 1) {
                    int iServPriority = _decode_by_bit(bitOffset, 3, p);
                    bitOffset += 3;
                    _replace_result_int(result_otherRAT_item,
                            "Serv Priority", iServPriority);
                } else {
                    _replace_result(result_otherRAT_item,
                            "Serv Priority", FieldValue{std::string("Not Present")});
                }

                int iThreshServ = _decode_by_bit(bitOffset, 6, p);
                bitOffset += 6;
                _replace_result_int(result_otherRAT_item,
                        "ThreshServ", iThreshServ);

                int iPerEarfcnParamsIncluded = _decode_by_bit(bitOffset, 1, p);
                bitOffset += 1;

                int iMaxReselectionTimerIncluded = _decode_by_bit(bitOffset, 1, p);
                bitOffset += 1;
                if (iMaxReselectionTimerIncluded == 1) {
                    int iMaxReselectionTimer = _decode_by_bit(bitOffset, 4, p);
                    bitOffset += 4;
                    _replace_result_int(result_otherRAT_item,
                            "MaxReselectionTimer", iMaxReselectionTimer);
                } else {
                    _replace_result(result_otherRAT_item,
                            "MaxReselectionTimer", FieldValue{std::string("Not Present")});
                }

                int iSearchBackOffTimerIncluded = _decode_by_bit(bitOffset, 1, p);
                bitOffset += 1;
                if (iSearchBackOffTimerIncluded == 1) {
                    _replace_result(result_otherRAT_item,
                            "SearchBackOffTimer", FieldValue{std::string("MI(Unknown)")});
                } else {
                    _replace_result(result_otherRAT_item,
                            "SearchBackOffTimer", FieldValue{std::string("Not Present")});
                }

                int iPLMNIDIncluded = _decode_by_bit(bitOffset, 1, p);
                bitOffset += 1;

                if (iPerEarfcnParamsIncluded == 1) {

                    int iNumEUTRAFreq = _decode_by_bit(bitOffset, 3, p);
                    bitOffset += 3;
                    _replace_result_int(result_otherRAT_item,
                            "NumEUTRAFreq", iNumEUTRAFreq);

                    FieldList result_EUTRAFreqs;
                    for (int j = 0; j < iNumEUTRAFreq; j++) {
                        FieldList result_EUTRAFreq_item;
                        _decode_by_fmt(_1xEVSignaling_EUTRAFreqFmt,
                                ARRAY_SIZE(_1xEVSignaling_EUTRAFreqFmt, Fmt),
                                b, offset, length, result_EUTRAFreq_item);

                        int iEARFCN = _decode_by_bit(bitOffset, 16, p);
                        bitOffset += 16;
                        _replace_result_int(result_EUTRAFreq_item,
                                "EARFCN", iEARFCN);

                        int iEarfcnPriority = _decode_by_bit(bitOffset, 3, p);
                        bitOffset += 3;
                        _replace_result_int(result_EUTRAFreq_item,
                                "EARFCNPriority", iEarfcnPriority);

                        int iThreshX = _decode_by_bit(bitOffset, 4, p);
                        bitOffset += 4;
                        _replace_result_int(result_EUTRAFreq_item,
                                "ThreshX", iThreshX);

                        int iRxLevMin = _decode_by_bit(bitOffset, 8, p);
                        bitOffset += 8;
                        _replace_result_int(result_EUTRAFreq_item,
                                "RxLevMinEUTRA", iRxLevMin);

                        int iPeMax = _decode_by_bit(bitOffset, 6, p);
                        bitOffset += 6;
                        _replace_result_int(result_EUTRAFreq_item,
                                "PeMax", iPeMax);

                        int iRxLevMinOffsetIncluded = _decode_by_bit(bitOffset, 1, p);
                        bitOffset += 1;
                        if (iRxLevMinOffsetIncluded == 1) {
                            _replace_result(result_EUTRAFreq_item,
                                    "RxLevMinOffset", FieldValue{std::string("MI(Unknown)")});
                        } else {
                            _replace_result(result_EUTRAFreq_item,
                                    "RxLevMinOffset", FieldValue{std::string("Not Present")});
                        }

                        int iMeasurementBandWidth = _decode_by_bit(bitOffset, 3, p);
                        bitOffset += 3;
                        _replace_result_int(result_EUTRAFreq_item,
                                "MeasurementBandWidth", iMeasurementBandWidth);

                        // PLMNAMWaPrevChannel
                        bitOffset += 2;
                        // NumPLMNIDs and PLMNID
                        if (iPLMNIDIncluded == 1) {
                            bitOffset += (4 + 22);
                        }

                        result_EUTRAFreqs.push_back({"Ignored", FieldValue{std::move(result_EUTRAFreq_item)}, "dict"});
                    }
                    result_otherRAT_item.push_back({"EUTRAFreqs", FieldValue{std::move(result_EUTRAFreqs)}, "list"});
                }
            }
            else {
                // Unknown RAT Type
            }

            bitOffset = bitOffsetSave + iRATRecordLength * 8;
            result_otherRATs.push_back({"Ignored", FieldValue{std::move(result_otherRAT_item)}, "dict"});
        }
        result.push_back({"Other RATs", FieldValue{std::move(result_otherRATs)}, "list"});
        return length;
    } else {
        return offset;
    }

}
