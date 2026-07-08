/*
 * NR_MAC_UL_Physical_Channel_Schedule_Report
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"
#include <string>


const Fmt NrMacVersion_Fmt [] = {
    {UINT, "Minor Version",                 2},
    {UINT, "Major Version",                 2},
    {UINT, "Sleep",        1},
    {UINT, "Beam Change",        1},
    {UINT, "Signal Change",        1},
    {UINT, "DL Dynamic Cfg Change",        1},
    {UINT, "DL Config", 1},
    {UINT, "UL Config", 1},
    {SKIP, NULL,                   2},
    {UINT, "Log Fields Change BMask", 2},
    {SKIP, NULL,                   1},
    {UINT, "Num Records", 1},
};

const Fmt NrMacUlSchedRecord_v2_8 [] = {
    {UINT, "Slot", 1},
    {UINT, "Numerology (kHz)", 1},
    {UINT, "Frame", 2},
    {UINT, "Num Carrier", 1},
    {SKIP, NULL,3},
};

const Fmt NrMacUlSchedCarrierRecord_v2_8 [] = {
    // {UINT, "Carrier ID", 1},   // Carrier ID + RNTI type
    // {UINT, "PhyChan BitMask", 1},
    {UINT, "PhyChan BitMask", 2},
    {SKIP, NULL,2},
};

const Fmt NrMacUlSchedPUSCHRecord_v2_8 [] = {
    {BIT_STREAM_LITTLE_ENDIAN, "PUSCH Raw Data", 40},
};

const Fmt NrMacUlSchedPUCCHRecord_v2_8 [] = {
    {BIT_STREAM_LITTLE_ENDIAN, "PUCCH Raw Data", 20},
};

const Fmt NrMacUlSchedSrsRecord_v2_8 [] = {
    {BIT_STREAM_LITTLE_ENDIAN, "SRS Raw Data", 20},
};

const Fmt NrMacUlPhyChannelScheduleSystime_Fmt_v2_11[] = {
    {UINT,"Slot",1},
    {UINT,"Numerology",1},
    {UINT,"Frame",2},
};

const Fmt NrMacUlPhyChannelScheduleRecords_Fmt_v2_11[] = {
    {UINT,"Num Carrier",1},
    {SKIP,NULL,3},
};

const Fmt NrMacUlPhyChannelScheduleCarriers_Fmt_v2_11[] = {
    {UINT,"Carrier ID",1},
    {PLACEHOLDER,"RNTI Type",0},
    {UINT,"Phychan Bit Mask",1},
    {PLACEHOLDER,"Dual Pol Status",0},
    {SKIP,NULL,2},
};

const Fmt NrMacUlPhyChannelSchedulePUSCH_Fmt_v2_11[] = {
    {UINT,"Is Second Phychan",1},
    {PLACEHOLDER,"Start Symbol",0},
    {UINT,"Num Symbols",1},
    {PLACEHOLDER,"HARQ ID",0},
    {UINT,"MCS",1},
    {PLACEHOLDER,"MCS Table",0},
    {PLACEHOLDER,"DMRS Add Pos",0},
    {UINT,"RB Start",1},
    {UINT,"Num RBs",2},
    {PLACEHOLDER,"RA Type",0},
    {PLACEHOLDER,"Mapping Type",0},
    {UINT,"TB Size( bytes)",2},
    {PLACEHOLDER,"DMRS Config Type",0},
    {PLACEHOLDER,"Antenna Switch Indicator",0},
    {UINT,"TX Mode",1},
    {PLACEHOLDER,"RV Index",0},
    {PLACEHOLDER,"DMRS PTRS Asoc",0},
    {PLACEHOLDER,"Beta Oddset Ind",0},
    {UINT,"TX Type",1},
    {PLACEHOLDER,"BWP Idx",0},
    {UINT,"Code Rate",2},
    {PLACEHOLDER,"UCI Request Mask",0},
    {UINT,"Num CSF P1 Bits",2},
    {UINT,"Num CSF P2 Bits",1},
    {UINT,"Num HARQ ACK Bits",1},
    {PLACEHOLDER,"Transform Precoding",0},
    {UINT,"K Prime Value",2},
    {UINT,"ZC Value",1},
    {UINT,"Num CBs",1},
    {UINT,"DMRS Symbol Bitmask",2},
    {PLACEHOLDER,"SRS Res Ind",0},
    {UINT,"CB Size",2},
    {PLACEHOLDER,"Freq Hop Flag",0},
    {UINT,"Tx Slot Offset",1},
    {UINT,"TPC Accum 0",1},
    {UINT,"TPC Accum 1",1},
    {UINT,"TPMI",1},
    {PLACEHOLDER,"Data Scrambling Selection",0},
    {PLACEHOLDER,"DMRS Ports[0]",0},
    {UINT,"DMRS Ports[1]",1},
    {UINT,"RB Start Hop",1},
    {PLACEHOLDER,"MOD_TYPE",0},
    {UINT,"RNTI Value",2},
    {UINT,"CDM Groups",1},
    {PLACEHOLDER,"DMRS Scrambing Selection",0},
    {SKIP,NULL ,3},
    {UINT,"RBG Bitmap",4},
};

const Fmt NrMacUlPhyChannelSchedulePUCCH_Fmt_v2_11[] = {
    {UINT,"Num PUCCH",1},
    {SKIP,NULL,3},
};

const Fmt NrMacUlPhyChannelSchedulePerPUCCH_Fmt_v2_11[] = {
    {UINT,"Is Second Phychan",1},
    {PLACEHOLDER,"PUCCH Format",0},
    {PLACEHOLDER,"UCI Request BMask",0},
    {UINT,"Start symbol",1},
    {PLACEHOLDER,"Num Symbols",0},
    {UINT,"Starting RB",2},
    {UINT,"Num RB",1},
    {UINT,"Freq Hopping Flag",1},
    {UINT,"Second Hop RB",1},
    {UINT,"Num HARQ ACK Bits",1},
    {PLACEHOLDER,"Num SR Bits",0},
    {UINT,"Num UCI P1 Bits",2},
    {UINT,"Num UCI P2 Bits",2},
    {PLACEHOLDER,"Spatial Relation Id",0},
    {PLACEHOLDER,"Antenna Switch Indictor",0},
    {UINT,"M0",1},
    {PLACEHOLDER,"Time OCC Index",0},
    {UINT,"I DMRS",1},
    {PLACEHOLDER,"DFT 0CC Length",0},
    {PLACEHOLDER,"DFT 0CC Index",0},
    {UINT,"TPC Accum 0",1},
    {UINT,"TPC Accum 1",1},
};
const Fmt NrMacUlPhyChannelSchedulePrach_Fmt_v2_11[] = {
	{UINT,"Resource Allocation",2},
	{UINT,"ZC Root Seq",1},
	{PLACEHOLDER,"Preamble Format",0},
	{UINT,"Symbol Offset",1},
	{PLACEHOLDER,"Numerology",0},
	{UINT,"PRACH Number",2},
	{UINT,"ZC Cyclic Shift",2},
};

const ValueName prach_Numerology_v2_11[] = {
	{1,"30KHZ"},
};

const ValueName PCSRSystemTimeNumerology_v2_11[] = {
	{0,"15KHZ"},
};
const ValueName NRMacULPhyCHAScheRntiType_v2_11[] = {
    {0b0000,"C_RNTI"},
    {0B0010,"T_C_RNTI"},
};
const ValueName NRMacULPhyCHASchePhychanBitMask_v2_11[] = {
    {0b000010,"PUSCH"},
    {0b000100,"PUCCH"},
    {0b000110,"PUSCH|PUCCH"},
	{0b010000,"PRACH"},
};
const ValueName NRMacULPhyCHAScheMCSTable_v2_11[] = {
    {0b0000,"64QAM"},
};
const  ValueName NRMacULPhyCHAScheMappingType_v2_11[] = {
    {1,"Type B"},
};
const ValueName NRMacULPhyCHAScheTxMode_v2_11[] = {
    {0,"SISO"},
};
const  ValueName NRMacULPhyCHAScheTXType_v2_11[] = {
    {0,"NEW_TX"},
};

const  ValueName NRMacULPhyCHAScheDataScramblingSelection_v2_11[] = {
    {0b01,"FROM_CELL_ID"},
    {0,"FROM_CFG"},
};
const  ValueName NRMacULPhyCHAScheMOD_TYPE_v2_11[] = {
    {0b010,"16QAM"},
    {0b001,"QPSK"},
};

const  ValueName NRMacULPhyCHAScheMDMRSScrambingSelection_v2_11[] = {
    {0,"NSCID_0"},
    {0b0010,"CELL_ID"},
};

const  ValueName NRMacULPhyCHASchePUCCHFormat_v2_11[] = {
    {0b1000,"PUCCH_FORMAT_F0"},
};

const  ValueName NRMacULPhyCHAScheUCIRequestBMask_v2_11[] = {
    {0,"ACK_NACK_PRT"},
};
const  ValueName NRMacULPhyCHAScheMFreqHoppingFlag_v2_11[] = {
    {0,"HOP_MODE_NEITHER"},
};
static int
_decode_nr_mac_ul_pcsr_system_time_v2_11(const char* b, int offset, size_t length,
	FieldList &result, const char* lc_name) {
	int start = offset;
	FieldList records;
	offset += _decode_by_fmt(NrMacUlPhyChannelScheduleSystime_Fmt_v2_11,
		ARRAY_SIZE(NrMacUlPhyChannelScheduleSystime_Fmt_v2_11, Fmt),
		b, offset, length, records);
	int tmp = _search_result_int(records, "Numerology");
	int num = tmp & 0x0f;
	_replace_result_int(records, "Numerology", num);
    (void)_map_result_field_to_name(records,
		"Numerology", PCSRSystemTimeNumerology_v2_11,
		ARRAY_SIZE(PCSRSystemTimeNumerology_v2_11, ValueName),
		"(MI)Unknown");
	tmp = _search_result_int(records, "Frame");
	int frame = tmp & 0x3ff;
	_replace_result_int(records, "Frame", frame);

	result.push_back({lc_name, FieldValue{std::move(records)}, "dict"});
	return offset - start;
}

static int
_decode_nr_mac_ul_pcsr_pusch_v2_11(const char* b, int offset, size_t length,
	FieldList &result, const char* lc_name) {
	int start = offset;
	FieldList records;
	offset += _decode_by_fmt(NrMacUlPhyChannelSchedulePUSCH_Fmt_v2_11,
		ARRAY_SIZE(NrMacUlPhyChannelSchedulePUSCH_Fmt_v2_11, Fmt),
		b, offset, length, records);

	int tmp = _search_result_int(records, "Is Second Phychan");
	int value = tmp & 0b1;
	_replace_result_int(records, "Is Second Phychan", value);

	value = (tmp >> 1) & 0x0f;
	_replace_result_int(records, "Start Symbol", value);

	int tmp2 = _search_result_int(records, "Num Symbols");
	value = ((tmp2 & 0b1) << 3) + ((tmp >> 5) & 0b111);
	_replace_result_int(records, "Num Symbols", value);

	value = ((tmp2 >> 1) & 0x0f);
	_replace_result_int(records, "HARQ ID", value);

	tmp = _search_result_int(records, "MCS");
	value = ((tmp & 0b1) << 3) + ((tmp2 >> 5) & 0b111);
	_replace_result_int(records, "MCS", value);

	value = (tmp >> 1) & 0x0f;
	_replace_result_int(records, "MCS Table", value);
	(void)_map_result_field_to_name(records,
		"MCS Table", NRMacULPhyCHAScheMCSTable_v2_11,
		ARRAY_SIZE(NRMacULPhyCHAScheMCSTable_v2_11, ValueName),
		"(MI)Unknown");

	value = (tmp >> 5) & 0b11;
	_replace_result_int(records, "DMRS Add Pos", value);

	tmp2 = _search_result_int(records, "RB Start");
	value = (tmp2 << 1) + (tmp >> 7);
	_replace_result_int(records, "RB Start", value);

	tmp = _search_result_int(records, "Num RBs");
	value = tmp & 0x1ff;
	_replace_result_int(records, "Num RBs", value);

	value = (tmp >> 9) & 0b1;
	_replace_result_int(records, "RA Type", value);

	value = (tmp >> 10) & 0b1;
	_replace_result_int(records, "Mapping Type", value);
	(void)_map_result_field_to_name(records,
		"Mapping Type", NRMacULPhyCHAScheMappingType_v2_11,
		ARRAY_SIZE(NRMacULPhyCHAScheMappingType_v2_11, ValueName),
		"(MI)Unknown");

	tmp2 = _search_result_int(records, "TB Size( bytes)");
	value = ((tmp2 & 0x1fff) << 5) + (tmp >> 3);
	_replace_result_int(records, "TB Size( bytes)", value);

	value = (tmp2 >> 13) & 0b11;
	_replace_result_int(records, "DMRS Config Type", value);

	value = (tmp2 >> 15);
	_replace_result_int(records, "Antenna Switch Indicator", value);

	tmp = _search_result_int(records, "TX Mode");
	value = tmp & 0b11;
	_replace_result_int(records, "TX Mode", value);
	(void)_map_result_field_to_name(records,
		"TX Mode", NRMacULPhyCHAScheTxMode_v2_11,
		ARRAY_SIZE(NRMacULPhyCHAScheTxMode_v2_11, ValueName),
		"(MI)Unknown");

	value = (tmp >> 2) & 0b111;
	_replace_result_int(records, "RV Index", value);

	value = (tmp >> 5) & 0b1;
	_replace_result_int(records, "DMRS PTRS Asoc", value);

	value = (tmp >> 6) & 0b11;
	_replace_result_int(records, "Beta Oddset Ind", value);

	tmp = _search_result_int(records, "TX Type");
	value = tmp & 0b11;
	_replace_result_int(records, "TX Type", value);
	(void)_map_result_field_to_name(records,
		"TX Type", NRMacULPhyCHAScheTXType_v2_11,
		ARRAY_SIZE(NRMacULPhyCHAScheTXType_v2_11, ValueName),
		"(MI)Unknown");
	value = (tmp >> 2) & 0b11;
	_replace_result_int(records, "BWP Idx", value);

	tmp2 = _search_result_int(records, "Code Rate");
	value = ((tmp2 & 0xfff) << 4) + (tmp >> 4);
	_replace_result_int(records, "Code Rate", value);

	value = tmp2 >> 12;
	_replace_result_int(records, "UCI Request Mask", value);

	tmp = _search_result_int(records, "Num CSF P1 Bits");
	value = tmp & 0x3ff;
	_replace_result_int(records, "Num CSF P1 Bits", value);

	tmp2 = _search_result_int(records, "Num CSF P2 Bits");
	value = (tmp2 << 6) + (tmp >> 10);
	_replace_result_int(records, "Num CSF P2 Bits", value);

	tmp = _search_result_int(records, "Num HARQ ACK Bits");
	value = tmp & 0x3f;
	_replace_result_int(records, "Num HARQ ACK Bits", value);

	value = (tmp >> 6) & 0b11;
	_replace_result_int(records, "Transform Precoding", value);

	tmp = _search_result_int(records, "K Prime Value");
	value = tmp & 0x7fff;
	_replace_result_int(records, "K Prime Value", value);

	tmp2 = _search_result_int(records, "ZC Value");
	value = (tmp2 << 1) + (tmp >> 15);
	_replace_result_int(records, "ZC Value", value);

	tmp = _search_result_int(records, "DMRS Symbol Bitmask");
	value = tmp & 0x3fff;
	_replace_result_int(records, "DMRS Symbol Bitmask", value);

	value = tmp >> 14;
	_replace_result_int(records, "SRS Res Ind", value);

	tmp = _search_result_int(records, "CB Size");
	value = tmp & 0x7fff;
	_replace_result_int(records, "CB Size", value);

	value = tmp >> 15;
	_replace_result_int(records, "Freq Hop Flag", value);

	tmp = _search_result_int(records, "Tx Slot Offset");
	value = tmp & 0x3f;
	_replace_result_int(records, "Tx Slot Offset", value);

	tmp2 = _search_result_int(records, "TPC Accum 0");
	value = ((tmp2 & 0x3f) << 2) + (tmp >> 6);
	_replace_result_int(records, "TPC Accum 0", value);

	tmp = _search_result_int(records, "TPC Accum 1");
	value = ((tmp & 0x3f) << 2) + (tmp2 >> 6);
	_replace_result_int(records, "TPC Accum 1", value);

	tmp2 = _search_result_int(records, "TPMI");
	value = ((tmp2 & 0b111) << 2) + (tmp >> 6);
	_replace_result_int(records, "TPMI", value);

	value = (tmp2 >> 3) & 0b11;
	_replace_result_int(records, "Data Scrambling Selection", value);
	(void)_map_result_field_to_name(records,
		"Data Scrambling Selection", NRMacULPhyCHAScheDataScramblingSelection_v2_11,
		ARRAY_SIZE(NRMacULPhyCHAScheDataScramblingSelection_v2_11, ValueName),
		"(MI)Unknown");

	value = (tmp2 >> 5) & 0b111;
	_replace_result_int(records, "DMRS Ports[0]", value);

	tmp = _search_result_int(records, "DMRS Ports[1]");
	value = tmp & 0x0f;
	_replace_result_int(records, "DMRS Ports[1]", value);

	tmp2 = _search_result_int(records, "RB Start Hop");
	value = ((tmp2 & 0x1f) << 4) + (tmp >> 4);
	_replace_result_int(records, "RB Start Hop", value);

	value = tmp2 >> 5;
	_replace_result_int(records, "MOD_TYPE", value);
	(void)_map_result_field_to_name(records,
		"MOD_TYPE", NRMacULPhyCHAScheMOD_TYPE_v2_11,
		ARRAY_SIZE(NRMacULPhyCHAScheMOD_TYPE_v2_11, ValueName),
		"(MI)Unknown");

	tmp = _search_result_int(records, "CDM Groups");
	value = tmp & 0b11;
	_replace_result_int(records, "CDM Groups", value);

	value = (tmp >> 2) & 0x0f;
	_replace_result_int(records, "DMRS Scrambing Selection", value);
	(void)_map_result_field_to_name(records,
		"DMRS Scrambing Selection", NRMacULPhyCHAScheMDMRSScrambingSelection_v2_11,
		ARRAY_SIZE(NRMacULPhyCHAScheMDMRSScrambingSelection_v2_11, ValueName),
		"(MI)Unknown");

	tmp = _search_result_int(records, "RBG Bitmap");
	if (tmp == 0) {
		std::string tmp_value = "NA";
		_replace_result_string(records, "RBG Bitmap", tmp_value);
	}

	result.push_back({lc_name, FieldValue{std::move(records)}, "dict"});
	return offset - start;
}
static int
_decode_nr_mac_ul_pcsr_per_pucch_v2_11(const char* b, int offset, size_t length,
	FieldList &result, std::string lc_name) {
	int start = offset;
	FieldList records;
	offset += _decode_by_fmt(NrMacUlPhyChannelSchedulePerPUCCH_Fmt_v2_11,
		ARRAY_SIZE(NrMacUlPhyChannelSchedulePerPUCCH_Fmt_v2_11, Fmt),
		b, offset, length, records);
	int tmp = _search_result_int(records, "Is Second Phychan");
	int value = tmp & 0b1;
	_replace_result_int(records, "Is Second Phychan", value);

	value = (tmp >> 1) & 0x0f;
	_replace_result_int(records, "PUCCH Format", value);
	(void)_map_result_field_to_name(records,
		"PUCCH Format", NRMacULPhyCHASchePUCCHFormat_v2_11,
		ARRAY_SIZE(NRMacULPhyCHASchePUCCHFormat_v2_11, ValueName),
		"(MI)Unknown");

	value = (tmp >> 5) & 0x0f;
	_replace_result_int(records, "UCI Request BMask", value);
	(void)_map_result_field_to_name(records,
		"UCI Request BMask", NRMacULPhyCHAScheUCIRequestBMask_v2_11,
		ARRAY_SIZE(NRMacULPhyCHAScheUCIRequestBMask_v2_11, ValueName),
		"(MI)Unknown");

	tmp = _search_result_int(records, "Start symbol");
	value = tmp & 0x0f;
	_replace_result_int(records, "Start symbol", value);

	value = (tmp >> 4) & 0x0f;
	_replace_result_int(records, "Num Symbols", value);

	tmp = _search_result_int(records, "Starting RB");
	value = tmp & 0x3ff;
	_replace_result_int(records, "Starting RB", value);

	tmp = _search_result_int(records, "Freq Hopping Flag");
	value = tmp & 0b111;
	_replace_result_int(records, "Freq Hopping Flag", value);
	(void)_map_result_field_to_name(records,
		"Freq Hopping Flag", NRMacULPhyCHAScheMFreqHoppingFlag_v2_11,
		ARRAY_SIZE(NRMacULPhyCHAScheMFreqHoppingFlag_v2_11, ValueName),
		"(MI)Unknown");

	int tmp2 = _search_result_int(records, "Second Hop RB");
	value = ((tmp2 & 0x0f) << 5) + (tmp >> 3);
	_replace_result_int(records, "Second Hop RB", value);

	if (value == 0x1ff) {
		std::string tmp_value = "NA";
		_replace_result_string(records, "Second Hop RB", tmp_value);
	}

	tmp = _search_result_int(records, "Num HARQ ACK Bits");
	value = ((tmp & 0b11) << 4) + (tmp2 >> 4);
	_replace_result_int(records, "Num HARQ ACK Bits", value);

	value = tmp >> 2;
	_replace_result_int(records, "Num SR Bits", value);

	tmp = _search_result_int(records, "Num UCI P1 Bits");
	value = tmp & 0x3ff;
	_replace_result_int(records, "Num UCI P1 Bits", value);

	tmp2 = _search_result_int(records, "Num UCI P2 Bits");
	value = ((tmp2 & 0x3ff) << 6) + (tmp >> 10);
	_replace_result_int(records, "Num UCI P2 Bits", value);

	value = (tmp2 >> 10) & 0x1f;
	_replace_result_int(records, "Spatial Relation Id", value);

	value = (tmp2 >> 15);
	_replace_result_int(records, "Antenna Switch Indictor", value);

	tmp = _search_result_int(records, "M0");
	value = tmp & 0x0f;
	_replace_result_int(records, "M0", value);

	value = (tmp >> 4) & 0b11;
	_replace_result_int(records, "Time OCC Index", value);

	tmp2 = _search_result_int(records, "I DMRS");
	value = ((tmp2 & 0b11) << 2) + (tmp >> 6);
	_replace_result_int(records, "I DMRS", value);

	value = (tmp2 >> 2) & 0b111;
	_replace_result_int(records, "DFT 0CC Length", value);

	value = (tmp2 >> 5) & 0b111;
	_replace_result_int(records, "DFT 0CC Index", value);

	const char* s_name = lc_name.c_str();
	result.push_back({s_name, FieldValue{std::move(records)}, "dict"});
	return offset - start;
}
static int
_decode_nr_mac_ul_pcsr_pucch_v2_11(const char* b, int offset, size_t length,
	FieldList &result, const char* lc_name) {
	int start = offset;
	FieldList records;
	offset += _decode_by_fmt(NrMacUlPhyChannelSchedulePUCCH_Fmt_v2_11,
		ARRAY_SIZE(NrMacUlPhyChannelSchedulePUCCH_Fmt_v2_11, Fmt),
		b, offset, length, records);

	int tmp = _search_result_int(records, "Num PUCCH");
	FieldList records_list;
	for (int i = 0; i < tmp; i++) {
		std::string name = "Per PUCCH Data[" + std::to_string(i) + "]";
		offset += _decode_nr_mac_ul_pcsr_per_pucch_v2_11(b, offset, length, records_list, lc_name);
	}
	records.push_back({"Per PUCCH Data", FieldValue{std::move(records_list)}, "list"});

	result.push_back({lc_name, FieldValue{std::move(records)}, "dict"});
	return offset - start;


}
static int
_decode_nr_mac_ul_pcsr_PRACH_v2_11(const char* b, int offset, size_t length,
	FieldList &result, const char* lc_name) {
	int start = offset;
	FieldList records;
	offset += _decode_by_fmt(NrMacUlPhyChannelSchedulePrach_Fmt_v2_11,
		ARRAY_SIZE(NrMacUlPhyChannelSchedulePrach_Fmt_v2_11, Fmt),
		b, offset, length, records);
	int tmp = _search_result_int(records, "Resource Allocation");
	int value = tmp & 0x1ff;
	_replace_result_int(records, "Resource Allocation", value);

	int tmp2 = _search_result_int(records, "ZC Root Seq");
	value = ((tmp2 & 0b11) << 7) + (tmp >> 9);
	_replace_result_int(records, "ZC Root Seq", value);

	value = tmp2 >> 2;
	_replace_result_int(records, "Preamble Format", value);

	tmp = _search_result_int(records, "Symbol Offset");
	value = tmp & 0x0f;
	_replace_result_int(records, "Symbol Offset", value);

	value = (tmp >> 4) & 0b11;
	_replace_result_int(records, "Numerology", value);
	(void)_map_result_field_to_name(records,
		"Numerology", prach_Numerology_v2_11,
		ARRAY_SIZE(prach_Numerology_v2_11, ValueName),
		"(MI)Unknown");
	tmp = _search_result_int(records, "PRACH Number");
	value = tmp & 0x1ff;
	_replace_result_int(records, "PRACH Number", value);

	tmp2 = _search_result_int(records, "ZC Cyclic Shift");
	value = ((tmp2 & 0x1ff) << 7) + (tmp >> 9);
	_replace_result_int(records, "ZC Cyclic Shift", value);

	result.push_back({lc_name, FieldValue{std::move(records)}, "dict"});
	return offset - start;


}

static int
_decode_nr_mac_ul_pcsr_carriers_v2_11(const char* b, int offset, size_t length,
	FieldList &result, std::string lc_name) {
	int start = offset;
	FieldList records;
	offset += _decode_by_fmt(NrMacUlPhyChannelScheduleCarriers_Fmt_v2_11,
		ARRAY_SIZE(NrMacUlPhyChannelScheduleCarriers_Fmt_v2_11, Fmt),
		b, offset, length, records);
	int tmp = _search_result_int(records, "Carrier ID");
	int value = tmp & 0b11;
	_replace_result_int(records, "Carrier ID", value);

	value = (tmp >> 2) & 0x0f;
	_replace_result_int(records, "RNTI Type", value);
	(void)_map_result_field_to_name(records,
		"RNTI Type", NRMacULPhyCHAScheRntiType_v2_11,
		ARRAY_SIZE(NRMacULPhyCHAScheRntiType_v2_11, ValueName),
		"(MI)Unknown");

	int tmp2 = _search_result_int(records, "Phychan Bit Mask");
	value = ((tmp2 & 0x0f) << 2) + ((tmp >> 6) & 0b11);
	int ch = value;
	_replace_result_int(records, "Phychan Bit Mask", value);
	(void)_map_result_field_to_name(records,
		"Phychan Bit Mask", NRMacULPhyCHASchePhychanBitMask_v2_11,
		ARRAY_SIZE(NRMacULPhyCHASchePhychanBitMask_v2_11, ValueName),
		"(MI)Unknown");

	value = ((tmp2 >> 4) & 0b11);
	_replace_result_int(records, "Dual Pol Status", value);
    
	if (ch & 0b010)//PUSCH 
	{
		offset += _decode_nr_mac_ul_pcsr_pusch_v2_11(b, offset, length, records, "PUSCH");
	}
    
	if (ch & 0b100)//PUCCH
	{
		offset += _decode_nr_mac_ul_pcsr_pucch_v2_11(b, offset, length, records, "Per PUCCH Data");
	}
    if(ch&0b010000)//PRACH
	{
       offset += _decode_nr_mac_ul_pcsr_PRACH_v2_11(b, offset, length, records, "PRACH");
    }
	const char* s_name = lc_name.c_str();
	result.push_back({s_name, FieldValue{std::move(records)}, "dict"});
	return offset - start;
}
static int
_decode_nr_mac_ul_pcsr_records_v2_11(const char* b, int offset, size_t length,
	FieldList &result, std::string lc_name) {
	int start = offset;
	FieldList records;
	offset += _decode_nr_mac_ul_pcsr_system_time_v2_11(b, offset, length, records, "System time");
	offset += _decode_by_fmt(NrMacUlPhyChannelScheduleRecords_Fmt_v2_11,
		ARRAY_SIZE(NrMacUlPhyChannelScheduleRecords_Fmt_v2_11, Fmt),
		b, offset, length, records);
	int tmp = _search_result_int(records, "Num Carrier");
    
	FieldList records_list;
	for (int i = 0; i < tmp; i++) {
		std::string s_name = "Carriers[" + std::to_string(i) + "]";
		offset += _decode_nr_mac_ul_pcsr_carriers_v2_11(b, offset, length, records_list, lc_name);
	}
	records.push_back({"Carriers", FieldValue{std::move(records_list)}, "list"});

	const char* s_name = lc_name.c_str();
	result.push_back({s_name, FieldValue{std::move(records)}, "dict"});
	return offset - start;



}



static int
_decode_nr_mac_ul_physical_channel_schedule_report_subpkt(const char *b, int offset, size_t length,
                       FieldList &result) {

    int start = offset;
    int major_ver = _search_result_int(result, "Major Version");
    int minor_ver = _search_result_int(result, "Minor Version");
    int n_records = _search_result_int(result, "Num Records");
    bool success = false;
	FieldList tmp_py;
    int tmp;
    switch (major_ver) {
        case 2:{

            switch (minor_ver){
                case 8:{
                    FieldList result_allrecords;

                    for(int i = 0; i < n_records; i++){
                        FieldList result_record;
                        offset += _decode_by_fmt(NrMacUlSchedRecord_v2_8,
                                                 ARRAY_SIZE(NrMacUlSchedRecord_v2_8, Fmt),
                                                 b, offset, length, result_record);

                        int n_carriers = _search_result_int(result_record, "Num Carrier");
                        FieldList result_allcarriers;
                        for(int j = 0; j < n_carriers; j++){

                            FieldList result_carrier;
                            offset += _decode_by_fmt(NrMacUlSchedCarrierRecord_v2_8,
                                                     ARRAY_SIZE(NrMacUlSchedCarrierRecord_v2_8, Fmt),
                                                     b, offset, length, result_carrier);

                            int phy_bitmask = _search_result_int(result_carrier, "PhyChan BitMask");
                           
                            int carrier_id = phy_bitmask & 0x000f; // TODO: Add RNTI type
                            bool pusch_flag = phy_bitmask & 0x0080; 
                            bool pucch_flag = phy_bitmask & 0x0100;
                            bool srs_flag = phy_bitmask & 0x0200;
                            bool tav_status = phy_bitmask & 0x2000;
                            bool ccd_status = phy_bitmask & 0x2000;
                            FieldValue py_carrier = FieldValue{int64_t(carrier_id)};
                            result_carrier.push_back({"Carrier ID", FieldValue{std::move(py_carrier)}, ""});
                            FieldValue py_pusch = FieldValue{int64_t(pusch_flag)};
                            result_carrier.push_back({"PUSCH flag", FieldValue{std::move(py_pusch)}, ""});
                            FieldValue py_pucch = FieldValue{int64_t(pucch_flag)};
                            result_carrier.push_back({"PUCCH flag", FieldValue{std::move(py_pucch)}, ""});
                            FieldValue py_srs = FieldValue{int64_t(srs_flag)};
                            result_carrier.push_back({"SRS flag", FieldValue{std::move(py_srs)}, ""});
                            FieldValue py_tav = FieldValue{int64_t(tav_status)};
                            result_carrier.push_back({"TAV status", FieldValue{std::move(py_tav)}, ""});
                            FieldValue py_ccd = FieldValue{int64_t(ccd_status)};
                            result_carrier.push_back({"CCD status", FieldValue{std::move(py_ccd)}, ""});



                            // PUSCH data
                            if(pusch_flag){

                                // TODO: Many more fileds are not yet parsed. The current implementation focuses on the most useful ones.

                                FieldList result_pusch;
                                offset += _decode_by_fmt(NrMacUlSchedPUSCHRecord_v2_8,
                                                     ARRAY_SIZE(NrMacUlSchedPUSCHRecord_v2_8, Fmt),
                                                     b, offset, length, result_pusch);

                                std::string py_pusch = _search_result_bytestream(result_pusch,"PUSCH Raw Data");
                                // printf("%s\n", py_pusch.c_str());
                                // _delete_result(result_pusch,"PUSCH data"); // Remove raw data

                                size_t pusch_len = py_pusch.size();


                                FieldValue is_second_phychan = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-4,4),0,2))};
                                result_pusch.push_back({"Is Second Phychan", FieldValue{std::move(is_second_phychan)}, ""});


                                FieldValue start_symbol = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-5,1),0,2))};
                                result_pusch.push_back({"Start Symbol", FieldValue{std::move(start_symbol)}, ""});


                                FieldValue num_symbol = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-9,4),0,2))};
                                result_pusch.push_back({"Num Symbols", FieldValue{std::move(num_symbol)}, ""});

                                FieldValue harq_id = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-13,4),0,2))};
                                result_pusch.push_back({"HARQ ID", FieldValue{std::move(harq_id)}, ""});


                                FieldValue mcs = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-18,5),0,2))};
                                result_pusch.push_back({"MCS", FieldValue{std::move(mcs)}, ""});

                                FieldValue dmrs_add_pos = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-24,6),0,2))};
                                result_pusch.push_back({"DMRS Add Pos", FieldValue{std::move(dmrs_add_pos)}, ""});

                                FieldValue RB_start = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-32,8),0,2))};
                                result_pusch.push_back({"RB Start", FieldValue{std::move(RB_start)}, ""});

                                FieldValue num_RB = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-41,9),0,2))};
                                result_pusch.push_back({"Num RBs", FieldValue{std::move(num_RB)}, ""});

                                FieldValue ra_type = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-42,1),0,2))};
                                result_pusch.push_back({"RA Type", FieldValue{std::move(ra_type)}, ""});

                                FieldValue tb_size = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-57,14),0,2))};
                                result_pusch.push_back({"TB Size(bytes)", FieldValue{std::move(tb_size)}, ""});


                                FieldValue code_rate = FieldValue{int64_t(stoi(py_pusch.substr(pusch_len-87,11),0,2))};
                                result_pusch.push_back({"Code rate", FieldValue{std::move(code_rate)}, ""});

 
                                result_carrier.push_back({"PUSCH Data (partial results)", FieldValue{std::move(result_pusch)}, "dict"});
                            }
                            

                            // PUCCH data
                            if(pucch_flag){

                                // TODO: Many more fileds are not yet parsed. The current implementation focuses on the most useful ones.

                                FieldList result_pusch;
                                offset += _decode_by_fmt(NrMacUlSchedPUCCHRecord_v2_8,
                                                     ARRAY_SIZE(NrMacUlSchedPUCCHRecord_v2_8, Fmt),
                                                     b, offset, length, result_pusch);


                                result_carrier.push_back({"PUCCH Data (coming soon)", FieldValue{std::move(result_pusch)}, "dict"});


                            }

                            if(srs_flag){

                                FieldList result_srs;
                                offset += _decode_by_fmt(NrMacUlSchedSrsRecord_v2_8,
                                                     ARRAY_SIZE(NrMacUlSchedSrsRecord_v2_8, Fmt),
                                                     b, offset, length, result_srs);


                                result_carrier.push_back({"SRS Data (coming soon)", FieldValue{std::move(result_srs)}, "dict"});

                            }


                            // if(prach_flag){

                            // }

                            char name_carrier[64];
                            sprintf(name_carrier, "Carrier [%d]", j);
                            result_allcarriers.push_back({name_carrier, FieldValue{std::move(result_carrier)}, "dict"});


                        }

                        result_record.push_back({"Carriers", FieldValue{std::move(result_allcarriers)}, "dict"});


                        char name[64];
                        sprintf(name, "Record [%d]", i);
                        result_allrecords.push_back({name, FieldValue{std::move(result_record)}, "dict"});

                    }




                    result.push_back({"Records", FieldValue{std::move(result_allrecords)}, "list"});


                    success = true;
                    break;
                }
                
                case 11: {
					tmp = _search_result_int(result, "Num Records");
					for (int i = 0; i < tmp; i++) {
						std::string name = "Records[" + std::to_string(i) + "]";
						offset += _decode_nr_mac_ul_pcsr_records_v2_11(b, offset, length, tmp_py, name);
					}
					result.push_back({"Records", FieldValue{std::move(tmp_py)}, "list"});
					success = 1;
                    break;
				}
                default:
                    break;

            }

            
        }
        default:
            break;
    }


    if(!success){

        printf("(MI)Unknown 5G_NR_MAC_UL_Physical_Channel_Schedule_Report: %d.%d\n", major_ver, minor_ver);
    }

    return offset - start;
}


