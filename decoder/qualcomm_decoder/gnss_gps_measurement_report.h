/*
 * GNSS_GPS_Measurement_Report
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt GnssGps_Fmt [] = {
    {UINT, "Version", 1},
    {UINT, "FCount", 4},
    {UINT, "GpsWeek", 2},
    {UINT, "GpsMsec", 4},
    {UINT, "TimeBias", 4},
    {UINT, "ClkTimeUnc", 4},
    {UINT, "ClkFreqBias", 4},
    {UINT, "ClkFreqUnc", 4},
    {UINT, "NumValidReports", 1},
};

const Fmt GnssGpsSv_Fmt [] = {
    {UINT, "SV", 1},
    {UINT, "ObsState", 1},
    {UINT, "Tot", 1},
    {UINT, "Gd", 1},
    {BYTE_STREAM_LITTLE_ENDIAN,"PrtyEM",2},
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
    {BYTE_STREAM, "MeasStat", 4},
    {UINT, "MiscStat", 1},
    {UINT, "MP", 4},
    {UINT, "AziDeg", 4},
    {UINT, "ElvDeg", 4},
    {UINT, "CarrPhaseInt", 4},
    {UINT, "CarrPhaseFrac", 2},
    {UINT, "FnSpd", 4},
    {UINT, "FnSpdU", 4},
    {UINT, "CSlip", 1},
    {BYTE_STREAM, "Reserved", 4},
   
};
static int _decode_gnss_gps_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int temp;
    float tempf;

     temp = _search_result_int(result, "TimeBias");
     float TimeBias=itof(temp);
     _replace_result(result,
             "TimeBias", FieldValue{double(TimeBias)});

     temp = _search_result_int(result, "ClkTimeUnc");
     float TimeUnc=itof(temp);
     _replace_result(result,
             "ClkTimeUnc", FieldValue{double(TimeUnc)});

     temp = _search_result_int(result, "ClkFreqBias");
     float FreqBias=itof(temp);
     _replace_result(result,
             "ClkFreqBias", FieldValue{double(FreqBias)});

     temp = _search_result_int(result, "ClkFreqUnc");
     float FreqUnc=itof(temp);
     _replace_result(result,
             "ClkFreqUnc", FieldValue{double(FreqUnc)});


    int start = offset;

    int numberSvs=_search_result_int(result, "NumValidReports");

    FieldList result_record;
    for (int i = 0; i < numberSvs; i++) {
        FieldList result_record_item;
        offset += _decode_by_fmt(GnssGpsSv_Fmt,
                ARRAY_SIZE(GnssGpsSv_Fmt, Fmt),
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
