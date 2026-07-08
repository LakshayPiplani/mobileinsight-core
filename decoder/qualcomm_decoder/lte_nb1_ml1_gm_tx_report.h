/*
 * LTE NB1 ML1 GM TX Report
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt LteNb1Ml1GmTxReport[] = {
    {UINT, "Version", 1},
    {SKIP, NULL, 1}, //Reserved
};

const Fmt LteNb1Ml1GmTxReport_v2[] = {
	{UINT, "Subcarrier Space", 1},    //left 3 bits
    {UINT, "Num of Records", 1},
};

const Fmt LteNb1Ml1GmTxReport_Record_v2[] = {
    {UINT, "NPDCCH Timing HSFN", 4},         // 10 bits
	{PLACEHOLDER, "NPDCCH Timing SFN",0},    // 10 bits
    {PLACEHOLDER, "NPDCCH Timing Sub FN",0}, // 4 bits
    {PLACEHOLDER, "NPUSCH Format",0}, // 1 bit
    {PLACEHOLDER, "Is MSG3",0}, // 1 bit
    {PLACEHOLDER, "ITBS",0}, // 6 bits
    {UINT, "Repetition Number", 4},         // 8 bits
    {PLACEHOLDER, "Num RU",0}, // 4 bits
    {PLACEHOLDER, "RV Index",0}, // 1 bit
    {PLACEHOLDER, "Num Tone",0}, // 4 bits
    {PLACEHOLDER, "Start Tone",0}, // 7 bits
    {PLACEHOLDER, "TX Power",0}, // 8 bits
    {UINT, "NPUSCH format 1 TX Type", 4},         // 1 bit
    {PLACEHOLDER, "ACK NACK",0}, // 1 bit
    {PLACEHOLDER, "RFLM",0}, // 8 bits
    {PLACEHOLDER, "PRACH Collision Valid",0}, // 1 bits
    {PLACEHOLDER, "PRACH Collision 7680ms",0}, // 3 bits
    {PLACEHOLDER, "Reserved",0}, // 18 bits
    {UINT, "Scrambling Mask",4},

};

static int _decode_lte_nb1_ml1_gm_tx_report_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int start = offset;
    int pkt_ver = _search_result_int(result, "Version");


    switch (pkt_ver) {
        case 2:
        {
        	offset += _decode_by_fmt(LteNb1Ml1GmTxReport_v2,
                    ARRAY_SIZE(LteNb1Ml1GmTxReport_v2, Fmt),
                    b, offset, length, result);
            int num_record = _search_result_int(result, "Num of Records");

            unsigned int iNonDecodeSubcarrierSpace = _search_result_uint(result, "Subcarrier Space");
            int iSubcarrierSpace = (iNonDecodeSubcarrierSpace >> 5) & 7;
            _replace_result_int(result, "Subcarrier Space",
                        iSubcarrierSpace);
            (void) _map_result_field_to_name(result, "Subcarrier Space",
                        ValueNameNB1_GM_TX_Report_Subcarrier_Space_Type,
                        ARRAY_SIZE(ValueNameNB1_GM_TX_Report_Subcarrier_Space_Type, ValueName),
                        "(MI)Unknown");

            FieldList result_record;
            for (int i = 0; i < num_record; i++) {
                FieldList result_record_item;
                offset += _decode_by_fmt(LteNb1Ml1GmTxReport_Record_v2,
                        ARRAY_SIZE(LteNb1Ml1GmTxReport_Record_v2, Fmt),
                        b, offset, length, result_record_item);

                unsigned int iNonDecodeHSFN = _search_result_uint(result_record_item, "NPDCCH Timing HSFN");
                int iHSFN = iNonDecodeHSFN & 1023;          // 10 bits
                int iSFN = (iNonDecodeHSFN >> 10) & 1023;   // 10 bits
                int iSubFN = (iNonDecodeHSFN >> 20) & 15;   // 4 bits
                int iNPUSCH_Fmt = (iNonDecodeHSFN >> 24) & 1;              // 1 bit
                int isMSG3 = (iNonDecodeHSFN >> 25) & 1;         // 1 bit
                int iITBS = (iNonDecodeHSFN >> 26) & 63;           // 6 bits

                _replace_result_int(result_record_item, "NPDCCH Timing HSFN",
                        iHSFN);
                _replace_result_int(result_record_item, "NPDCCH Timing SFN",
                        iSFN);
                _replace_result_int(result_record_item, "NPDCCH Timing Sub FN",
                        iSubFN);

                _replace_result_int(result_record_item, "NPUSCH Format",
                        iNPUSCH_Fmt);
                (void) _map_result_field_to_name(result_record_item, "NPUSCH Format",
                        ValueNameNPUSCHFormat,
                        ARRAY_SIZE(ValueNameNPUSCHFormat, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "Is MSG3",
                        isMSG3);
                (void) _map_result_field_to_name(result_record_item, "Is MSG3",
                        ValueNameTrueOrFalse,
                        ARRAY_SIZE(ValueNameTrueOrFalse, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "ITBS",
                        iITBS);

                int iNonDecodeRepNum = _search_result_int(result_record_item, "Repetition Number");
                int iRepNum = iNonDecodeRepNum & 255;          // 8 bits
                int iRUNum = (iNonDecodeRepNum >> 8) & 15;   // 4 bits
                int iRVInd = (iNonDecodeRepNum >> 12) & 1;   // 1 bits
                int iNumTone = (iNonDecodeRepNum >> 13) & 15;              // 4 bit
                int iStartTone = (iNonDecodeRepNum >> 17) & 127;         // 7 bit
                int iTxPower = iNonDecodeRepNum >> 24;           // 8 bits, but it is signed 8 bits

                _replace_result_int(result_record_item, "Repetition Number",
                        iRepNum);
                _replace_result_int(result_record_item, "Num RU",
                        iRUNum);
                _replace_result_int(result_record_item, "PRACH Collision Valid",
                        iRVInd);
                _replace_result_int(result_record_item, "Num Tone",
                        iNumTone);
                _replace_result_int(result_record_item, "Start Tone",
                        iStartTone);
                _replace_result_int(result_record_item, "TX Power",
                        iTxPower);

                unsigned int iNonDecodeNPUSCHFmtTX = _search_result_uint(result_record_item, "NPUSCH format 1 TX Type");
                int iNPUSCH_Fmt_TX = iNonDecodeNPUSCHFmtTX & 1;          // 1 bits
                int iACK = (iNonDecodeNPUSCHFmtTX >> 1) & 1;   // 1 bits
                int iRFLM = (iNonDecodeNPUSCHFmtTX >> 2) & 255;   // 8 bits
                int iPRACHColVal = (iNonDecodeNPUSCHFmtTX >> 10) & 1;              // 1 bit
                int iPRACHCol7680 = (iNonDecodeNPUSCHFmtTX >> 11) & 7;         // 3 bit
                int iReserved = (iNonDecodeNPUSCHFmtTX >> 14) & 0x3ffff;           // 18 bits

                _replace_result_int(result_record_item, "NPUSCH format 1 TX Type",
                        iNPUSCH_Fmt_TX);
                (void) _map_result_field_to_name(result_record_item, "NPUSCH format 1 TX Type",
                        ValueNameNPUSCHFormat_1_TX_Type,
                        ARRAY_SIZE(ValueNameNPUSCHFormat_1_TX_Type, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "ACK NACK",
                        iACK);
                (void) _map_result_field_to_name(result_record_item, "ACK NACK",
                        ValueNameAckOrNack,
                        ARRAY_SIZE(ValueNameAckOrNack, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "RFLM",
                        iRFLM);
                _replace_result_int(result_record_item, "PRACH Collision Valid",
                        iPRACHColVal);
                _replace_result_int(result_record_item, "PRACH Collision 7680ms",
                        iPRACHCol7680);
                _replace_result_int(result_record_item, "Reserved",
                        iReserved);

                result_record.push_back({"Ignored", FieldValue{std::move(result_record_item)}, "dict"});

            }
            result.push_back({"Records", FieldValue{std::move(result_record)}, "list"});
            return offset - start;
        }
        default:
            printf("(MI)Unknown LTE NB1 ML1 GM TX Report version: 0x%x\n", pkt_ver);
            return 0;
    }
}