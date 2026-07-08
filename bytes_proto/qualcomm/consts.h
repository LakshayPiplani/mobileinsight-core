/* consts.h
 * Author: Jiayao Li
 * Defines constants general to the whole module.
 */

 // Updated by: Lakshay Piplai
 // Date: Jun 23, 2026

#ifndef __DM_COLLECTOR_C_CONSTS_H__
#define __DM_COLLECTOR_C_CONSTS_H__

#include "utils.h"

#include <cstddef>   // size_t (for the is_*_packet classifiers)

// const int EXPOSE_INTERNAL_LOGS=1;  //Yuanjie: 1 means expose all logs, 0 means only public logs are exposed

// All supported type IDs
enum LogPacketType {
    CDMA_Paging_Channel_Message = 0x1007,

    _1xEV_Connection_Attempt = 0x106E,
    _1xEV_Connection_Release = 0x1071,
    _1xEV_Signaling_Control_Channel_Broadcast = 0x107C,
    _1xEV_Connected_State_Search_Info = 0x118A,
    Srch_TNG_1x_Searcher_Dump = 0x119B,
    _1xEV_Rx_Partial_MultiRLP_Packet = 0x1277,
    _1xEVDO_Multi_Carrier_Pilot_Sets = 0x12A1,
    HDR_Pilot_Sets = 0x12B6,    // TODO

    WCDMA_Search_Cell_Reselection_Rank = 0x4005,
    WCDMA_RRC_States = 0x4125,
    WCDMA_CELL_ID = 0x4127,
    WCDMA_Signaling_Messages = 0x412F,

    UMTS_NAS_GMM_State = 0x7130,
    UMTS_NAS_MM_State = 0x7131,
    UMTS_NAS_MM_REG_State = 0x7135,
    UMTS_NAS_OTA = 0x713A,

    LTE_RRC_OTA_Packet = 0xB0C0,
    LTE_RRC_MIB_Message_Log_Packet = 0xB0C1,
    LTE_RRC_Serv_Cell_Info_Log_Packet = 0xB0C2,

    LTE_NAS_ESM_Plain_OTA_Incoming_Message = 0xB0E2,
    LTE_NAS_ESM_Plain_OTA_Outgoing_Message = 0xB0E3,
    LTE_NAS_EMM_Plain_OTA_Incoming_Message = 0xB0EC,
    LTE_NAS_EMM_Plain_OTA_Outgoing_Message = 0xB0ED,
    LTE_NAS_EMM_State = 0xB0EE,
    LTE_NAS_ESM_State = 0xB0E5,

    LTE_PDCP_DL_Config = 0xB0A0,
    LTE_PDCP_UL_Config = 0xB0B0,
    LTE_PDCP_UL_Data_PDU = 0xB0B1,
    LTE_PDCP_DL_Ctrl_PDU = 0xB0A2,
    LTE_PDCP_UL_Ctrl_PDU = 0xB0B2,
    LTE_PDCP_DL_Cipher_Data_PDU = 0xB0A3,
    LTE_PDCP_UL_Cipher_Data_PDU = 0xB0B3,
    LTE_PDCP_DL_Stats = 0xB0A4,
    LTE_PDCP_UL_Stats = 0xB0B4,
    LTE_PDCP_DL_SRB_Integrity_Data_PDU = 0xB0A5,
    LTE_PDCP_UL_SRB_Integrity_Data_PDU = 0xB0B5,

    LTE_MAC_Configuration = 0xB060,
    LTE_MAC_Rach_Trigger = 0xB061,
    LTE_MAC_Rach_Attempt = 0xB062,
    LTE_MAC_DL_Transport_Block = 0xB063,
    LTE_MAC_UL_Transport_Block = 0xB064,
    LTE_MAC_UL_Buffer_Status_Internal = 0xB066,
    LTE_MAC_UL_Tx_Statistics = 0xB067,

    LTE_RLC_UL_Config_Log_Packet = 0xB091,
    LTE_RLC_DL_Config_Log_Packet = 0xB081,
    LTE_RLC_UL_AM_All_PDU = 0xB092,
    LTE_RLC_DL_AM_All_PDU = 0xB082,
    LTE_RLC_UL_Stats = 0xB097,
    LTE_RLC_DL_Stats = 0xB087,

    LTE_PUCCH_Power_Control = 0xB16F,
    LTE_PUSCH_Power_Control = 0xB16E,
    LTE_PDCCH_PHICH_Indication_Report = 0xB16B,
    LTE_PDSCH_Stat_Indication = 0xB173,

    LTE_PHY_Connected_Mode_LTE_Intra_Freq_Meas_Results = 0xB179,
    LTE_PHY_CDMA_MEAS = 0xB185,
    LTE_PHY_IRAT_Measurement_Request = 0xB187,
    LTE_PHY_IRAT_MDB = 0xB188,
    LTE_PHY_RLM_Report = 0xB18A,
    LTE_PHY_System_Scan_Results = 0xB18E,
    LTE_PHY_Idle_Neighbor_Cell_Meas = 0xB192,
    LTE_PHY_Serving_Cell_Measurement_Result = 0xB193,
    LTE_PHY_Connected_Mode_Neighbor_Meas_Req_Resp = 0xB195,
    LTE_PHY_CDRX_Events_Info = 0xB198,
    LTE_PHY_Inter_Freq_Log = 0xB19E,
    LTE_PHY_BPLMN_Cell_Request = 0xB1A1,
    LTE_PHY_BPLMN_Cell_Confirm = 0xB1A2,

    LTE_PHY_Serving_Cell_COM_Loop = 0xB121,
    LTE_PHY_PDSCH_Demapper_Configuration = 0xB126,
    LTE_PHY_PDCCH_Decoding_Result = 0xB130,
    LTE_PHY_PDSCH_Decoding_Result = 0xB132,
    LTE_PHY_PUSCH_Tx_Report = 0xB139,
    LTE_PHY_PUCCH_Tx_Report = 0xB13C,
    LTE_PHY_PUCCH_CSF = 0xB14D,
    LTE_PHY_PUSCH_CSF = 0xB14E,

    GSM_Surround_Cell_BA_List = 0x5071,
    GSM_RR_Signaling_Message = 0x512F,
    GSM_RR_Cell_Reselection_Parameters = 0x5130,
    GSM_RR_Cell_Information = 0x5134,
    GSM_RR_Cell_Reselection_Meas = 0x51FC,

    GSM_DSDS_RR_Signaling_Message = 0x5B2F,
    GSM_DSDS_RR_Cell_Reselection_Parameters = 0x5B30,
    GSM_DSDS_RR_Cell_Information = 0x5B34,

    Modem_debug_message = 0x1FEB,

    LTE_NB1_Random_Access_Request_Report = 0xB240,
    LTE_NB1_Random_Access_Response_Report = 0xB241,
    LTE_NB1_UE_Identification_Message_Report= 0xB242,
    LTE_NB1_Contention_Resolution_Message_Report = 0xB243,
    
    LTE_NB1_ML1_GM_DCI_Info = 0xB244,
    LTE_NB1_ML1_GM_TX_Report = 0xB245,
    LTE_NB1_ML1_GM_PDSCH_STAT_Ind = 0xB246,
    LTE_NB1_ML1_CDRX_Events_Info = 0xB248,
    LTE_NB1_ML1_Sum_Sys_Info = 0xB24B,
    LTE_NB1_ML1_Cell_Resel = 0xB24C,
    LTE_NB1_ML1_Serv_Cell_Meas_Response = 0xB24D,
    LTE_NB1_NBR_Cell_Meas = 0xB24E,
    LTE_NB1_ML1_Search_PBCH_Decode = 0xB250,
    LTE_NB1_ML1_DLM_Decode_Page = 0xB251,
    
    
    //GNSS related
    GNSS_GPS_Measurement_Report= 0x1477,
    GNSS_Glonass_Measurement_Report=0x1480,
    GNSS_BDS_Measurement_Report=0x1756,
    GNSS_GAL_Measurement_Report=0x1886,

    // 5G SM
    NR_NAS_SM5G_Plain_OTA_Incoming_Msg = 0xB800,
    NR_NAS_SM5G_Plain_OTA_Outgoing_Msg = 0xB801,

    // 5G MM
    NR_NAS_MM5G_Plain_OTA_Incoming_Msg = 0xB80A,
    NR_NAS_MM5G_Plain_OTA_Outgoing_Msg = 0xB80B,
    NR_NAS_MM5G_Plain_OTA_Container_Msg = 0xB814,
    NR_NAS_MM5G_State= 0xB80C,

    // 5G RRC
    NR_RRC_OTA_Packet = 0xB821,

    // 5G PDCP
    NR_PDCP_UL_Control_Pdu = 0xB861, 

    // 5G L2
    NR_L2_UL_TB = 0xB872,
    NR_L2_UL_BSR = 0xB873,

    // 5G RLC
    NR_RLC_DL_Stats = 0xB84D,

    // 5G MAC
    NR_MAC_UL_Physical_Channel_Schedule_Report = 0xB883,
    NR_MAC_PDSCH_Stats = 0xB888,
    NR_MAC_RACH_Trigger= 0xB889,
    NR_MAC_UL_TB_Stats=0xB881, 
    // 5G LL1/ML1
    NR_LL1_FW_Serving_FTL = 0xB8DD,
    NR_ML1_Searcher_Measurement_Database_Update_Ext = 0xB97F,                                       
    NR_ML1_Serving_Cell_Beam_Management = 0xB975,

};

// Mapping from type IDs to human-readable names.
// Defined in consts.cpp — one copy in the binary regardless of how many
// translation units include this header.
extern const ValueName LogPacketTypeID_To_Name[];
extern const int        LogPacketTypeID_To_Name_n;

// Classify an unescaped DIAG frame by its leading byte(s).
// Shared by the decoder (frame dispatch) and the export manager (whitelist
// filtering); moved here from decoder/qualcomm_decoder/log_packet.{h,cpp}.
bool is_log_packet    (const char *b, size_t length);
bool is_debug_packet  (const char *b, size_t length);
bool is_custom_packet (const char *b, size_t length);

#endif	// __DM_COLLECTOR_C_CONSTS_H__
