/*
 * LTE NB1 ML1 GM PDSCH STAT Ind
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt LteNb1Ml1GmPdschStatIndFmt[] = {
    {UINT, "Version", 1},
    {SKIP, NULL, 2}, //Reserved
};

const Fmt LteNb1Ml1GmPdschStatIndFmt_v2[] = {
    {UINT, "Num of Records", 1},

};

const Fmt LteNb1Ml1GmPdschStatIndFmt_Record_v2[] = {
    {UINT, "SFN", 4},         //  10 bits
    {PLACEHOLDER, "Sub FN",0},    // 4 bits
    {PLACEHOLDER, "RNTI Type",0},    // 3 bits 
    {PLACEHOLDER, "CRC",0},    // 1 bit 
    {PLACEHOLDER, "TB Size",0},    // 14 bits
    {UINT, "Hyper SFN Data", 4},         //  10 bits
    {PLACEHOLDER, "MCS",0},    // 4 bits
    {PLACEHOLDER, "Isf",0},    // 3 bits
    {PLACEHOLDER, "Auto ACK",0},    // 1 bits
    {PLACEHOLDER, "NDI",0},    // 14 bits
};

static int _decode_lte_nb1_ml1_gm_pdsch_stat_ind_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int start = offset;
    int pkt_ver = _search_result_int(result, "Version");


    switch (pkt_ver) {
        case 2:
        {
        	offset += _decode_by_fmt(LteNb1Ml1GmPdschStatIndFmt_v2,
                    ARRAY_SIZE(LteNb1Ml1GmPdschStatIndFmt_v2, Fmt),
                    b, offset, length, result);
            int num_record = _search_result_int(result, "Num of Records");

            FieldList result_record;
            for (int i = 0; i < num_record; i++) {
                FieldList result_record_item;
                offset += _decode_by_fmt(LteNb1Ml1GmPdschStatIndFmt_Record_v2,
                        ARRAY_SIZE(LteNb1Ml1GmPdschStatIndFmt_Record_v2, Fmt),
                        b, offset, length, result_record_item);

                unsigned int iNonDecodeSFN = _search_result_uint(result_record_item, "SFN");
                int iSFN = iNonDecodeSFN & 1023;          // 10 bits
                int iSubFN = (iNonDecodeSFN >> 10) & 15;   // 4 bits
                int iRNTI_T = (iNonDecodeSFN >> 14) & 7;   // 3 bits
                int iCRC = (iNonDecodeSFN >> 17) & 1;   // 1 bits
                int iTB_S = (iNonDecodeSFN >> 18) & 0x3fff;   // 14 bits

                _replace_result_int(result_record_item, "SFN",
                        iSFN);
                _replace_result_int(result_record_item, "Sub FN",
                        iSubFN);
                _replace_result_int(result_record_item, "RNTI Type",
                        iRNTI_T);
                (void) _map_result_field_to_name(result_record_item, "RNTI Type",
                        ValueNameNB1_PDSCH_RNTIType,
                        ARRAY_SIZE(ValueNameNB1_PDSCH_RNTIType, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "CRC",
                        iCRC);
                (void) _map_result_field_to_name(result_record_item, "CRC",
                        ValueNamePassOrFail,
                        ARRAY_SIZE(ValueNamePassOrFail, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "TB Size",
                        iTB_S);

                unsigned int iNonDecodeHyperSFN = _search_result_uint(result_record_item, "Hyper SFN Data");
                int iHyperSFN = iNonDecodeHyperSFN & 1023;          // 10 bits
                int iMCS = (iNonDecodeHyperSFN >> 10) & 15;   // 4 bits
                int iISF = (iNonDecodeHyperSFN >> 14) & 7;   // 3 bits
                int iAutoACK = (iNonDecodeHyperSFN >> 17) & 1;   // 1 bits
                int iNDI = (iNonDecodeHyperSFN >> 18) & 0x3fff;   // 14 bits

                _replace_result_int(result_record_item, "Hyper SFN Data",
                        iHyperSFN);
                _replace_result_int(result_record_item, "MCS",
                        iMCS);
                _replace_result_int(result_record_item, "Isf",
                        iISF);
                _replace_result_int(result_record_item, "Auto ACK",
                        iAutoACK);
                (void) _map_result_field_to_name(result_record_item, "Auto ACK",
                        ValueNameTrueOrFalse,
                        ARRAY_SIZE(ValueNameTrueOrFalse, ValueName),
                        "(MI)Unknown");
                _replace_result_int(result_record_item, "NDI",
                        iNDI);


                result_record.push_back({"Ignored", FieldValue{std::move(result_record_item)}, "dict"});

            }
            result.push_back({"Records", FieldValue{std::move(result_record)}, "list"});
            return offset - start;
        }
        default:
            printf("(MI)Unknown LTE NB1 ML1 GM PDSCH Stat Ind version: 0x%x\n", pkt_ver);
            return 0;
    }
}