/*
 * LTE PHY System Scan Results
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt LtePhySystemScanResults_Fmt [] = {
    {UINT, "Version", 1},
};

const Fmt LtePhySystemScanResults_Payload_v2 [] = {
    {SKIP, NULL, 1},
    {UINT, "Num Candidates", 2}
};

const Fmt LtePhySystemScanResults_Candidate_v2 [] = {
    {UINT, "EARFCN", 4},
    {UINT, "Band", 2},
    {UINT, "Bandwidth (MHz)", 2},   // (x-1)*5
    {UINT, "Energy (dBm/100KHz)", 2},   // x-65536
    {SKIP, NULL, 2},
};

const Fmt LtePhySystemScanResults_Payload_v32 [] = {
    {UINT, "Use Init Search", 1},
    {UINT, "Num Candidates", 2}
};

const Fmt LtePhySystemScanResults_Candidate_v32 [] = {
    {UINT, "EARFCN", 4},
    {UINT, "Band", 2},
    {UINT, "Bandwidth (MHz)", 2},   // (x-1)*5
    {UINT, "Energy (dBm/100KHz)", 2},   // x-65536
    {SKIP, NULL, 2},
};

static int _decode_lte_phy_system_scan_results_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int start = offset;
    int pkt_ver = _search_result_int(result, "Version");

    switch (pkt_ver) {
    case 2:
        {
            offset += _decode_by_fmt(LtePhySystemScanResults_Payload_v2,
                    ARRAY_SIZE(LtePhySystemScanResults_Payload_v2, Fmt),
                    b, offset, length, result);
            int num_candidate = _search_result_int(result, "Num Candidates");

            FieldList result_record;
            for (int i = 0; i < num_candidate; i++) {
                FieldList result_record_item;
                offset += _decode_by_fmt(LtePhySystemScanResults_Candidate_v2,
                        ARRAY_SIZE(LtePhySystemScanResults_Candidate_v2, Fmt),
                        b, offset, length, result_record_item);
                int iBandwidth = _search_result_int(result_record_item,
                        "Bandwidth (MHz)");
                iBandwidth = (iBandwidth - 1) * 5;
                _replace_result_int(result_record_item,
                        "Bandwidth (MHz)", iBandwidth);
                int iEnergy = _search_result_int(result_record_item,
                        "Energy (dBm/100KHz)");
                iEnergy -= 65536;
                _replace_result_int(result_record_item,
                        "Energy (dBm/100KHz)", iEnergy);

                result_record.push_back({"Ignored", FieldValue{std::move(result_record_item)}, "dict"});
            }
            result.push_back({"Candidates", FieldValue{std::move(result_record)}, "list"});
            return offset - start;
        }
    case 32:
        {
            offset += _decode_by_fmt(LtePhySystemScanResults_Payload_v32,
                    ARRAY_SIZE(LtePhySystemScanResults_Payload_v32, Fmt),
                    b, offset, length, result);


            int temp =_search_result_int(result, "Use Init Search");
            int iUseInitSearch = temp & 0x1;

            _replace_result_int(result, "Use Init Search",
                    iUseInitSearch);

            int num_candidate = _search_result_int(result, "Num Candidates");

            FieldList result_record;
            for (int i = 0; i < num_candidate; i++) {
                FieldList result_record_item;
                offset += _decode_by_fmt(LtePhySystemScanResults_Candidate_v32,
                        ARRAY_SIZE(LtePhySystemScanResults_Candidate_v32, Fmt),
                        b, offset, length, result_record_item);
                int iBandwidth = _search_result_int(result_record_item,
                        "Bandwidth (MHz)");
                iBandwidth = (iBandwidth - 1) * 5;
                _replace_result_int(result_record_item,
                        "Bandwidth (MHz)", iBandwidth);
                int iEnergy = _search_result_int(result_record_item,
                        "Energy (dBm/100KHz)");
                iEnergy -= 65536;
                _replace_result_int(result_record_item,
                        "Energy (dBm/100KHz)", iEnergy);

                result_record.push_back({"Ignored", FieldValue{std::move(result_record_item)}, "dict"});
            }
            result.push_back({"Candidates", FieldValue{std::move(result_record)}, "list"});
            return offset - start;
        }
    default:
        printf("(MI)Unknown LTE PHY System Scan Result version: 0x%x\n", pkt_ver);
        return 0;
    }
}
