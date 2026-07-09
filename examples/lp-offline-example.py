#!/usr/bin/python
# Filename: offline-analysis-example.py
import os
import sys

"""
Offline analysis by replaying logs
"""

# Import MobileInsight modules
from mobile_insight.monitor import OfflineReplayer
from mobile_insight.analyzer import MsgLogger, NrRrcAnalyzer, LteRrcAnalyzer, WcdmaRrcAnalyzer, LteNasAnalyzer, UmtsNasAnalyzer, LteMacAnalyzer, LteMeasurementAnalyzer

if __name__ == "__main__":

    # Initialize a monitor
    src = OfflineReplayer()
    src.set_input_path("/vagrant/lp_EA0_Jun30_DeregCheck.mi2log")
    # src.enable_log_all()

    src.enable_log("5G_NR_RRC_OTA_Packet")
    # src.enable_log("LTE_RRC_OTA_Packet")
    #src.enable_log("WCDMA_RRC_OTA_Packet")
    # src.enable_log("WCDMA_RRC_Serv_Cell_Info")
    src.enable_log("5G_NR_NAS_SM_Plain_OTA_Incoming_Msg")
    src.enable_log("5G_NR_NAS_SM_Plain_OTA_Outgoing_Msg")
    #src.enable_log("5G_NR_NAS_MM5G_State")
    src.enable_log("5G_NR_NAS_MM_Plain_OTA_Incoming_Msg")
    src.enable_log("5G_NR_NAS_MM_Plain_OTA_Outgoing_Msg")


    logger = MsgLogger()
    logger.set_decode_format(MsgLogger.XML)
    logger.set_dump_type(MsgLogger.FILE_ONLY)
    logger.save_decoded_msg_as("./decoded-EA0-lp-Jun30.txt")
    logger.set_source(src)

    # # Analyzers
    # nr_rrc_analyzer = NrRrcAnalyzer()
    # nr_rrc_analyzer.set_source(src)  # bind with the monitor

    # lte_rrc_analyzer = LteRrcAnalyzer()
    # lte_rrc_analyzer.set_source(src)  # bind with the monitor

    # wcdma_rrc_analyzer = WcdmaRrcAnalyzer()
    # wcdma_rrc_analyzer.set_source(src)  # bind with the monitor

    # lte_nas_analyzer = LteNasAnalyzer()
    # lte_nas_analyzer.set_source(src)

    # umts_nas_analyzer = UmtsNasAnalyzer()
    # umts_nas_analyzer.set_source(src)

    # lte_mac_analyzer = LteMacAnalyzer()
    # lte_mac_analyzer.set_source(src)

    # lte_meas_analyzer = LteMeasurementAnalyzer()
    # lte_meas_analyzer.set_source(src)

    # print lte_meas_analyzer.get_rsrp_list() 
    # print lte_meas_analyzer.get_rsrq_list()

    # Start the monitoring
    src.run()
