/*
 *GNSS_BDS_Measurement_Report 
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt GnssBds_Fmt [] = {
    {UINT, "Version", 1},
    {UINT, "Engine Type", 1},
    {UINT, "Hardware Version", 1},
    {UINT, "FCount", 4},
    {UINT, "BdsWeek", 2},
    {UINT, "BdsMsec", 4},
    {UINT, "TimeBias", 4},
    {UINT, "TimeUnc", 4},
    {UINT, "FreqBias", 4},
    {UINT, "FreqUnc", 4},
    {UINT, "NumberSvs", 1},
};

const Fmt GnssBdsSvInfo_Fmt [] = {
    {UINT, "SV", 1},
    {UINT, "ObsState", 1},
    {UINT, "Tot", 1},
    {UINT, "Gd", 1},
    {BYTE_STREAM, "PrtyEM", 2},
    {UINT, "FiltN", 1},
    {UINT, "CNo", 2},
    {UINT, "Latency", 2},
    {UINT, "Pre", 1},
    {UINT, "Post", 2},
    {UINT, "Ms", 4},
    {UINT, "SubMs", 4},
    {UINT, "TUnc", 4},
    {UINT, "Speed", 4},
    {UINT, "SpdUnc", 4},
    {BYTE_STREAM_LITTLE_ENDIAN, "MeasStat", 4},
    {UINT, "MiscStat", 1},
    {UINT, "MP", 4},
    {UINT, "AziDeg", 4},
    {UINT, "ElvDeg", 4},
    {UINT, "CarrPhaseInt", 4},
    {UINT, "CarrPhaseFrac", 2},
    {UINT, "FnSpd", 4},
    {UINT, "FnSpdU", 4},
    {UINT, "CSlip", 1},
    {UINT, "Reserved1", 4},
    
};

static float itof(int i){
	float b=0;
	void *pf;
	pf=&b;
	int * px=&i;
	*(int*)pf=*px;
	return b;
}

static int _decode_gnss_bds_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int temp;
    float tempf;

    temp = _search_result_int(result, "TimeBias");
    float TimeBias=itof(temp);
    _replace_result(result,
            "TimeBias", FieldValue{double(TimeBias)});

    temp = _search_result_int(result, "TimeUnc");
    float TimeUnc=itof(temp);
    _replace_result(result,
            "TimeUnc", FieldValue{double(TimeUnc)});

    temp = _search_result_int(result, "FreqBias");
    float FreqBias=itof(temp);
    _replace_result(result,
            "FreqBias", FieldValue{double(FreqBias)});

    temp = _search_result_int(result, "FreqUnc");
    float FreqUnc=itof(temp);
    _replace_result(result,
            "FreqUnc", FieldValue{double(FreqUnc)});

    (void)_map_result_field_to_name(result, "Engine Type",
                ValueNameGNSSBDSEngineType,
                ARRAY_SIZE(ValueNameGNSSBDSEngineType, ValueName),
                "(MI)Unknown");
    int start = offset;

    int numberSvs=_search_result_int(result, "NumberSvs");

    FieldList result_record;
    for (int i = 0; i < numberSvs; i++) {
        FieldList result_record_item;
        offset += _decode_by_fmt(GnssBdsSvInfo_Fmt,
                ARRAY_SIZE(GnssBdsSvInfo_Fmt, Fmt),
                b, offset, length, result_record_item);
        (void)_map_result_field_to_name(result_record_item, "ObsState",
                ValueNameBdsObsState,
                ARRAY_SIZE(ValueNameBdsObsState, ValueName),
                "(MI)Unknown");

        temp = _search_result_int(result_record_item, "CNo");
        float CNo=temp*1.0/100.0;
        _replace_result(result_record_item,
                "CNo", FieldValue{double(CNo)});

        temp = _search_result_int(result_record_item, "SubMs");
        tempf=itof(temp);
        _replace_result(result_record_item,
                "SubMs", FieldValue{double(tempf)});

        temp = _search_result_int(result_record_item, "TUnc");
        tempf=itof(temp);
        _replace_result(result_record_item,
                "TUnc", FieldValue{double(tempf)});

        temp = _search_result_int(result_record_item, "SpdUnc");
        tempf=itof(temp);
        _replace_result(result_record_item,
                "SpdUnc", FieldValue{double(tempf)});

        temp = _search_result_int(result_record_item, "Speed");
        tempf=itof(temp);
        _replace_result(result_record_item,
                "Speed", FieldValue{double(tempf)});


        temp = _search_result_int(result_record_item, "AziDeg");
        tempf=itof(temp);
        tempf=tempf*57.3;
        _replace_result(result_record_item,
                "AziDeg", FieldValue{double(tempf)});

        temp = _search_result_int(result_record_item, "ElvDeg");
        tempf=itof(temp);
        tempf=tempf*57.3;
        _replace_result(result_record_item,
                "ElvDeg", FieldValue{double(tempf)});

        temp = _search_result_int(result_record_item, "FnSpd");
        tempf=itof(temp);
        _replace_result(result_record_item,
                "FnSpd", FieldValue{double(tempf)});

        temp = _search_result_int(result_record_item, "FnSpdU");
        tempf=itof(temp);
        _replace_result(result_record_item,
                "FnSpdU", FieldValue{double(tempf)});

        temp=_search_result_int(result_record_item,"Latency");
        if(temp>32768){
            temp=temp-65536;
            _replace_result_int(result_record_item, "Latency", temp);
        }

        result_record.push_back({"Ignored", FieldValue{std::move(result_record_item)}, "dict"});
    }
    result.push_back({"SvInfo", FieldValue{std::move(result_record)}, "list"});
    return offset - start;
}
