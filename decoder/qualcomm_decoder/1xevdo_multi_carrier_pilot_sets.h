/*
 * 1xEV-DO Multi Carrier Pilot Sets
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt _1xEvdoMcps_Fmt [] = {
    {UINT, "Version", 1},
};

const Fmt _1xEvdoMcps_Payload_v2 [] = {
    {UINT, "Pilot PN Increment", 1},
    {UINT, "Searcher State", 1},
    {UINT, "Active Set Count", 1},
    {UINT, "Active Set Window", 1},
    {UINT, "Slot Count", 2},
    {UINT, "Candidate Set Count", 1},
    {UINT, "Candidate Set Window", 1},
    {UINT, "Neighbor Set Count", 1},
    {UINT, "Neighbor Set Window", 1},
    {UINT, "Remaining Set Channel", 2},
};

const Fmt _1xEvdoMcps_ASET [] = {
    {UINT, "PN Offset", 2},
    {UINT, "Total Energy", 2},
    {UINT, "OFS Condition Energy", 2},
    {UINT, "Pilot Group ID", 1},
    {UINT, "Channel Number", 2},    // 11 bits
    {PLACEHOLDER, "Band Class", 0}, // 5 bits
    {UINT, "Link ID", 1},
    {UINT, "Demod Carrier Index", 1},   // 2 bits
    {PLACEHOLDER, "Sector Reportable", 0},  // 1 bit
    {PLACEHOLDER, "Subactive Set Index", 0},    // 2 bits
    {PLACEHOLDER, "Scheduler Tag", 0},  // 3 bits
    {UINT, "ASP Index", 1}, // 4 bits
    {PLACEHOLDER, "RPC Index", 0},  // 4 bits
    {UINT, "DRC Cover", 1}, // 3 bits
    {PLACEHOLDER, "Drop Timer Expired", 0}, // 1 bit
    {PLACEHOLDER, "Drop Timer Active", 0},  // 1 bit
    {UINT, "Forward Link MAC Index", 2},    // 10 bits
    {PLACEHOLDER, "DSC Value", 0},  // 4 bits
    {PLACEHOLDER, "Auxiliary DRC Cover", 0},    // 2 bits
    {UINT, "RAB MAC Index", 2}, // 7 bits
    {PLACEHOLDER, "Reverse Link MAC Index", 0}, // 9 bits
    {UINT, "Window Center X2", 2},
};

const Fmt _1xEvdoMcps_CSET [] = {
    {UINT, "PN Offset", 2},
    {UINT, "Total Energy", 2},
    {UINT, "OFS Condition Energy", 2},
    {UINT, "Pilot Group ID", 1},
    {UINT, "Channel Number", 2},    // 11 bits
    {PLACEHOLDER, "Band Class", 0}, // 5 bits
    {UINT, "Drop Timer Expired", 1},    // 1 bit
    {PLACEHOLDER, "Drop Timer Active", 0}, // 1 bit
    {UINT, "Window Center X2", 2},
};
const Fmt _1xEvdoMcps_NSET [] = {
    {UINT, "PN Offset", 2},
    {UINT, "Total Energy", 2},
    {UINT, "OFS Condition Energy", 2},
    {UINT, "Pilot Group ID", 1},
    {UINT, "Channel Number", 2},    // 11 bits
    {PLACEHOLDER, "Band Class", 0}, // 5 bits
    {UINT, "Window Size", 2},
    {UINT, "Window Offset", 1}, // 3 bits
    {UINT, "Age of Sector", 2},
};

static int _decode_1xevdo_mcps_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int start = offset;
    int pkt_ver = _search_result_int(result, "Version");

    // PyObject *pyfloat;
    int temp;

    switch (pkt_ver) {
    case 2:
        {
            offset += _decode_by_fmt(_1xEvdoMcps_Payload_v2,
                    ARRAY_SIZE(_1xEvdoMcps_Payload_v2, Fmt),
                    b, offset, length, result);
            (void)_map_result_field_to_name(result, "Searcher State",
                    ValueNameSearcherState,
                    ARRAY_SIZE(ValueNameSearcherState, ValueName),
                    "(MI)Unknown");
            int num_asets = _search_result_int(result, "Active Set Count");
            int num_csets = _search_result_int(result, "Candidate Set Count");
            int num_nsets = _search_result_int(result, "Neighbor Set Count");

            FieldList result_asets;
            for (int i = 0; i < num_asets; i++) {
                FieldList result_aset_item;
                offset += _decode_by_fmt(_1xEvdoMcps_ASET,
                        ARRAY_SIZE(_1xEvdoMcps_ASET, Fmt),
                        b, offset, length, result_aset_item);

                temp = _search_result_int(result_aset_item, "Channel Number");
                int iChannelNumber = temp & 2047;   // 11 bits
                int iBandClass = (temp >> 11) & 31;
                _replace_result_int(result_aset_item,
                        "Channel Number", iChannelNumber);
                _replace_result_int(result_aset_item,
                        "Band Class", iBandClass);
                (void)_map_result_field_to_name(result_aset_item, "Band Class",
                        ValueNameBandClassCDMA,
                        ARRAY_SIZE(ValueNameBandClassCDMA, ValueName),
                        "(MI)Unknown");

                temp = _search_result_int(result_aset_item,
                        "Demod Carrier Index");
                int iDemodCarrierIndex = temp & 3;  // 2 bits
                int iSectorReportable = (temp >> 2) & 1;    // 1 bit
                int iSubactiveSetIndex = (temp >> 3) & 3;   // 2 bits
                int iSchedulerTag = (temp >> 5) & 7;    // 3 bits
                _replace_result_int(result_aset_item,
                        "Demod Carrier Index", iDemodCarrierIndex);
                _replace_result_int(result_aset_item,
                        "Sector Reportable", iSectorReportable);
                _replace_result_int(result_aset_item,
                        "Subactive Set Index", iSubactiveSetIndex);
                _replace_result_int(result_aset_item,
                        "Scheduler Tag", iSchedulerTag);

                temp = _search_result_int(result_aset_item,
                        "ASP Index");
                int iASPIndex = temp & 15;  // 4 bits
                int iRPCIndex = (temp >> 4) & 15;   // 4 bits
                _replace_result_int(result_aset_item,
                        "ASP Index", iASPIndex);
                _replace_result_int(result_aset_item,
                        "RPC Index", iRPCIndex);

                temp = _search_result_int(result_aset_item,
                        "DRC Cover");
                int iDRCCover = temp & 7;   // 3 bits
                int iDropTimerExpired = (temp >> 3) & 1;    // 1 bit
                int iDropTimerActive = (temp >> 4) & 1; // 1 bit
                _replace_result_int(result_aset_item,
                        "DRC Cover", iDRCCover);
                _replace_result_int(result_aset_item,
                        "Drop Timer Expired", iDropTimerExpired);
                _replace_result_int(result_aset_item,
                        "Drop Timer Active", iDropTimerActive);

                temp = _search_result_int(result_aset_item,
                        "Forward Link MAC Index");
                int iForwardLinkMACIndex = temp & 1023; // 10 bits
                int iDSCValue = (temp >> 10) & 15;  // 4 bits
                int iAuxiliaryDRCCover = (temp >> 14) & 3;  // 2 bits
                _replace_result_int(result_aset_item,
                        "Forward Link MAC Index", iForwardLinkMACIndex);
                _replace_result_int(result_aset_item,
                        "DSC Value", iDSCValue);
                _replace_result_int(result_aset_item,
                        "Auxiliary DRC Cover", iAuxiliaryDRCCover);

                temp = _search_result_int(result_aset_item,
                        "RAB MAC Index");
                int iRABMACIndex = temp & 127;  // 7 bits
                int iReverseLinkMACIndex = (temp >> 7) & 511;   // 9 bits
                _replace_result_int(result_aset_item,
                        "RAB MAC Index", iRABMACIndex);
                _replace_result_int(result_aset_item,
                        "Reverse Link MAC Index", iReverseLinkMACIndex);

                result_asets.push_back({"Ignored", FieldValue{std::move(result_aset_item)}, "dict"});
            }
            result.push_back({"ASET Pilots", FieldValue{std::move(result_asets)}, "list"});

            FieldList result_csets;
            for (int i = 0; i < num_csets; i++) {
                FieldList result_cset_item;
                offset += _decode_by_fmt(_1xEvdoMcps_CSET,
                        ARRAY_SIZE(_1xEvdoMcps_CSET, Fmt),
                        b, offset, length, result_cset_item);

                temp = _search_result_int(result_cset_item, "Channel Number");
                int iChannelNumber = temp & 2047;   // 11 bits
                int iBandClass = (temp >> 11) & 31;
                _replace_result_int(result_cset_item,
                        "Channel Number", iChannelNumber);
                _replace_result_int(result_cset_item,
                        "Band Class", iBandClass);
                (void)_map_result_field_to_name(result_cset_item, "Band Class",
                        ValueNameBandClassCDMA,
                        ARRAY_SIZE(ValueNameBandClassCDMA, ValueName),
                        "(MI)Unknown");

                temp = _search_result_int(result_cset_item,
                        "Drop Timer Expired");
                int iDropTimerExpired = temp & 1;   // 1 bit
                int iDropTimerActive = (temp >> 1) & 1; // 1 bit
                _replace_result_int(result_cset_item,
                        "Drop Timer Expired", iDropTimerExpired);
                _replace_result_int(result_cset_item,
                        "Drop Timer Active", iDropTimerActive);

                result_csets.push_back({"Ignored", FieldValue{std::move(result_cset_item)}, "dict"});
            }
            result.push_back({"CSET Pilots", FieldValue{std::move(result_csets)}, "list"});

            FieldList result_nsets;
            for (int i = 0; i < num_nsets; i++) {
                FieldList result_nset_item;
                offset += _decode_by_fmt(_1xEvdoMcps_NSET,
                        ARRAY_SIZE(_1xEvdoMcps_NSET, Fmt),
                        b, offset, length, result_nset_item);

                temp = _search_result_int(result_nset_item, "Channel Number");
                int iChannelNumber = temp & 2047;   // 11 bits
                int iBandClass = (temp >> 11) & 31;
                _replace_result_int(result_nset_item,
                        "Channel Number", iChannelNumber);
                _replace_result_int(result_nset_item,
                        "Band Class", iBandClass);
                (void)_map_result_field_to_name(result_nset_item, "Band Class",
                        ValueNameBandClassCDMA,
                        ARRAY_SIZE(ValueNameBandClassCDMA, ValueName),
                        "(MI)Unknown");

                temp = _search_result_int(result_nset_item, "Window Offset");
                int iWindowOffset = temp & 7;   // 3 bits
                _replace_result_int(result_nset_item,
                        "Window Offset", iWindowOffset);

                result_nsets.push_back({"Ignored", FieldValue{std::move(result_nset_item)}, "dict"});
            }
            result.push_back({"NSET Pilots", FieldValue{std::move(result_nsets)}, "list"});

            return offset - start;
        }
    default:
        printf("(MI)Unknown 1xEV-DO Multi Carrier Pilot Sets version: 0x%x\n", pkt_ver);
        return 0;
    }
}
