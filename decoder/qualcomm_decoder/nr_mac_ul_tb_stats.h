/*
 * NR5G MAC UL TB Stats
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt NrMacUlTbStats_Fmt[] = {
    {UINT, "Minor Version", 2},
    {UINT, "Major Version", 2},
    {UINT, "Sleep",1},
    {UINT, "Beam Change", 1},
    {UINT, "Signal Change",1},
    {UINT, "DL Dynamic Cfg Change", 1},
    {UINT, "DL Config", 1},
    {UINT, "UL Config", 1},//4 bit
    {PLACEHOLDER, "ML1_State_Change", 0},//4bit
    {SKIP, NULL, 2},
    {UINT, "Log Fields Change BMask", 2},
    {SKIP, NULL, 1},
    {UINT, "Num Records", 1},
    {SKIP, NULL, 4},
};

const Fmt Records_v2_0 [] = {
    {BYTE_STREAM_LITTLE_ENDIAN, "TB New Tx Bytes", 8},
    {BYTE_STREAM_LITTLE_ENDIAN, "TB ReTx Bytes", 8},
    {BYTE_STREAM_LITTLE_ENDIAN, "Num MCS", 8}, 
    {BYTE_STREAM_LITTLE_ENDIAN, "Num PRB", 8},
    {BYTE_STREAM_LITTLE_ENDIAN, "PHR", 8},
    {UINT, "Total Power", 4}, 
    {UINT, "Num New TX TB", 4},
    {UINT, "Num ReTx TB", 4},
    {UINT, "Num DTX ", 4}, 
    {UINT, "Num RI", 4},
    {UINT, "RI", 4},
    {UINT, "Num CQI", 4},
    {UINT, "CQI", 4},
    {UINT, "Num PHR", 4},
    {UINT, "TPC Accum", 4},
    {UINT, "Num ULSCH Sched", 4},
    {UINT, "Num No ULSCH Sched", 4},
    {UINT, "Pcmax", 2},
    {UINT, "Flush Gap Count", 2},
           
};
const Fmt NrMacUlTbStatsRecord_v2_1[] = {
    {UINT, "TB New Tx Bytes",     8},
    {UINT, "TB ReTx Bytes",     8},
    {UINT, "Num MCS",     8},
    {UINT, "Num PRB",     8},
    {UINT, "PHR",     8},
    {UINT, "Total Power",     8},
    {UINT, "Num New Tx TB",     4},
    {UINT, "Num ReTx TB",     4},
    {UINT, "Num RI",     4},
    {UINT, "RI",     4},
    {UINT, "Num CQI",     4},
    {UINT, "CQI",     4},
    {UINT, "Num PHR",     4},
    {UINT, "TPC Accum",     4},
    {UINT, "Num ULSCH Sched",     4},
    {UINT, "Num No ULSCH Sched",     4},
    {UINT, "Num Pcmax (dBm10)",     2},
    {UINT, "Flush Gap Count",     2},
    {SKIP, NULL,                  4},
};


static int _decode_nr_mac_ul_tb_stats_subpkt(const char* b,
    int offset, size_t length, FieldList &result) {
    bool success = false;
    int start = offset;
    int major_ver = _search_result_int(result, "Major Version");
    int minor_ver = _search_result_int(result, "Minor Version");
    int n_record = _search_result_int(result, "Num Records");
    switch (major_ver) {
    case 2:
    {
        switch (minor_ver) {
        case 1:
        {
            FieldList result_allrecords;
            int temp = _search_result_uint(result, "UL Config");
            int iUL = temp & 15; // 4 bits
            int iML = (temp >> 4) & 15;    // 4 bits	
            _replace_result_int(result,
                "UL Config", iUL);
            _replace_result_int(result,
                "ML1_State_Change", iML);
            for (int i = 0; i < n_record; i++) {
                FieldList result_record;
                offset += _decode_by_fmt(NrMacUlTbStatsRecord_v2_1,
                    ARRAY_SIZE(NrMacUlTbStatsRecord_v2_1, Fmt),
                    b, offset, length, result_record);


                char name[64];
                sprintf(name, "Record[%d]", i);
                result_allrecords.push_back({name, FieldValue{std::move(result_record)}, "dict"});
            }

            result.push_back({"Records List", FieldValue{std::move(result_allrecords)}, "list"});
            success = true;
            break;

        }
        case 0:
        {
            FieldList result_allrecords;
            int temp = _search_result_uint(result, "UL Config");
            int iUL = temp & 15; // 4 bits
            int iML = (temp >> 4) & 15;    // 4 bits	
            _replace_result_int(result,
                "UL Config", iUL);
            _replace_result_int(result,
                "ML1_State_Change", iML);

            for (int i = 0; i < n_record; i++) {
                //Records
                FieldList result_Record;
                offset += _decode_by_fmt(Records_v2_0,
                    ARRAY_SIZE(Records_v2_0, Fmt),
                    b, offset, length, result_Record);


		                               
//            	long long int temp1 = _search_result_ulongint(result_Record, "PHR");
//		        long long int iPHR = temp1*4294967296;  	
//		        _replace_result_ulongint(result_Record,
//		            "PHR", iPHR);
//		        Py_DECREF(old_object); 
		                            
                char name[64];
                sprintf(name, "Record[%d]", i);
                result_allrecords.push_back({name, FieldValue{std::move(result_Record)}, "dict"});
            }
            //Records > result
            result.push_back({"Records List", FieldValue{std::move(result_allrecords)}, "list"});
            success = true;
            break;
        }
        default:
            break;
        }
    }
    default:
        break;

    }
    if (!success) {
        printf("(MI)Unknown 5G NR MAC UL TB Stats: %d.%d\n", major_ver, minor_ver);
    }
    return offset - start;
}



















        
