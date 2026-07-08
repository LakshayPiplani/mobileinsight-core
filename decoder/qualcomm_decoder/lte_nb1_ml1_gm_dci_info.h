/*
 * LTE NB1 ML1 GM DCI Info
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt LteNb1Ml1GmDciInfoFmt[] = {
    {UINT, "Version", 1},
    {SKIP, NULL, 2}, //Reserved
};

const Fmt LteNb1Ml1GmDciInfoFmt_v3[] = {
    {UINT, "Num of Records", 1},

};

const Fmt LteNb1Ml1GmDciInfoFmt_Record_v3[] = {
    {UINT, "NPDCCH Timing HSFN", 4},          // 10 bits
    {PLACEHOLDER, "NPDCCH Timing SFN",0},    // 10 bits
    {PLACEHOLDER, "NPDCCH Timing Sub FN",0}, // 4 bits
    {PLACEHOLDER, "RNTI Type Data", 0},             // 1 bit
    {PLACEHOLDER, "RNTI Type",0},            // 2 bits
    {PLACEHOLDER, "UL Grant Present",0},     // 1 bit
    {PLACEHOLDER, "DL Grant Present",0},     // 1 bit
    {PLACEHOLDER, "PDCCH Order Present",0},  // 1 bit
    {PLACEHOLDER, "NDI",0},                  // 1 bit
    {PLACEHOLDER, "Reserved",0},             // 1 bit
    {UINT, "SC Index",4},                    // 5 bits
    {PLACEHOLDER, "Redundancy Version",0},   // 2 bit
    {PLACEHOLDER, "Resource Assignment",0},  // 3 bit
    {PLACEHOLDER, "Scheduling Delay",0},     // 3 bit
    {PLACEHOLDER, "MCS",0},                    // 5 bits
    {PLACEHOLDER,"Repetition Number",0},       // 3 bits
    {PLACEHOLDER,"DCI Repetition Number",0},    // 2 bits
    {PLACEHOLDER,"HARQ Resource",0},            // 4 bits
    {PLACEHOLDER,"Reserved2",0},               // 5 bits
};


static int _decode_lte_nb1_ml1_gm_dci_info_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int start = offset;
    int pkt_ver = _search_result_int(result, "Version");


    switch (pkt_ver) {
        case 3:
        {
            offset += _decode_by_fmt(LteNb1Ml1GmDciInfoFmt_v3,
                    ARRAY_SIZE(LteNb1Ml1GmDciInfoFmt_v3, Fmt),
                    b, offset, length, result);
            int num_record = _search_result_int(result, "Num of Records");

            FieldList result_record;
            for (int i = 0; i < num_record; i++) {
                FieldList result_record_item;
                offset += _decode_by_fmt(LteNb1Ml1GmDciInfoFmt_Record_v3,
                        ARRAY_SIZE(LteNb1Ml1GmDciInfoFmt_Record_v3, Fmt),
                        b, offset, length, result_record_item);

                unsigned int iNonDecodeHSFN = _search_result_uint(result_record_item, "NPDCCH Timing HSFN");
                int iHSFN = iNonDecodeHSFN & 1023;          // 10 bits
                int iSFN = (iNonDecodeHSFN >> 10) & 1023;   // 10 bits
                int iSubFN = (iNonDecodeHSFN >> 20) & 15;   // 4 bits
                int iRNTI_T_D = (iNonDecodeHSFN >> 24) & 1;              // 1 bit
                int iRNTI_T = (iNonDecodeHSFN >> 25) & 3;         // 2 bit
                int iUL_P = (iNonDecodeHSFN >> 27) & 1;           // 1 bit
                int iDL_P = (iNonDecodeHSFN >> 28) & 1;           // 1 bit
                int iPDCCH_P = (iNonDecodeHSFN >> 29) & 1;        // 1 bit
                int iNDI = (iNonDecodeHSFN >> 30) & 1;            // 1 bit
                int iReserved = (iNonDecodeHSFN >> 31) & 1;       // 1 bit

                _replace_result_int(result_record_item, "NPDCCH Timing HSFN",
                        iHSFN);
                _replace_result_int(result_record_item, "NPDCCH Timing SFN",
                        iSFN);
                _replace_result_int(result_record_item, "NPDCCH Timing Sub FN",
                        iSubFN);

                _replace_result_int(result_record_item, "RNTI Type Data",
                        iRNTI_T_D);
                _replace_result_int(result_record_item, "RNTI Type",
                        iRNTI_T);
                (void) _map_result_field_to_name(result_record_item, "RNTI Type",
                        ValueNameNBIoT_RNTIType,
                        ARRAY_SIZE(ValueNameNBIoT_RNTIType, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "UL Grant Present",
                        iUL_P);
                (void) _map_result_field_to_name(result_record_item, "UL Grant Present",
                        ValueNameTrueOrFalse,
                        ARRAY_SIZE(ValueNameTrueOrFalse, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "DL Grant Present",
                        iDL_P);
                (void) _map_result_field_to_name(result_record_item, "DL Grant Present",
                        ValueNameTrueOrFalse,
                        ARRAY_SIZE(ValueNameTrueOrFalse, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "PDCCH Order Present",
                        iPDCCH_P);
                (void) _map_result_field_to_name(result_record_item, "PDCCH Order Present",
                        ValueNameTrueOrFalse,
                        ARRAY_SIZE(ValueNameTrueOrFalse, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "NDI",
                        iNDI);
                _replace_result_int(result_record_item, "Reserved",
                        iReserved);

                unsigned int iNonDecodeSC_I = _search_result_uint(result_record_item, "SC Index");
                int iSC_I = iNonDecodeSC_I & 31;                   // 5 bits
                int iRedund_v = (iNonDecodeSC_I >> 5) & 3;         // 2 bits
                int iRes_a = (iNonDecodeSC_I >> 7) & 7;            // 3 bits
                int iSched_delay = (iNonDecodeSC_I >> 10) & 7;     // 3 bits
                int iMCS = (iNonDecodeSC_I >> 13) & 31;            // 5 bits
                int iRepetition_N = (iNonDecodeSC_I >> 18) & 7;    // 3 bits
                int iDCI_Repetition_N = (iNonDecodeSC_I >> 21) & 3;// 2 bits
                int iHARQ_R = (iNonDecodeSC_I >> 23) & 15;         // 4 bits
                int iReserved2 = (iNonDecodeSC_I >> 27) & 31;      // 5 bits

                _replace_result_int(result_record_item, "SC Index",
                        iSC_I);
                _replace_result_int(result_record_item, "Redundancy Version",
                        iRedund_v);
                _replace_result_int(result_record_item, "Resource Assignment",
                        iRes_a);
                _replace_result_int(result_record_item, "Scheduling Delay",
                        iSched_delay);
                _replace_result_int(result_record_item, "MCS",
                        iMCS);
                _replace_result_int(result_record_item, "Repetition Number",
                        iRepetition_N);
                _replace_result_int(result_record_item, "DCI Repetition Number",
                        iDCI_Repetition_N);
                _replace_result_int(result_record_item, "HARQ Resource",
                        iHARQ_R);
                _replace_result_int(result_record_item, "Reserved2",
                        iReserved2);

                result_record.push_back({"Ignored", FieldValue{std::move(result_record_item)}, "dict"});
            }
            result.push_back({"Records", FieldValue{std::move(result_record)}, "list"});
            return offset - start;
        }
        default:
            printf("(MI)Unknown LTE NB1 ML1 GM DCI Info version: 0x%x\n", pkt_ver);
            return 0;
    }
}

