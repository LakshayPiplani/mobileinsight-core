#!/usr/bin/python
# Filename: dm_collector.py
"""
dm_collector.py
A monitor for 3G/4G mobile network protocols (RRC/EMM/ESM).

Author: Jiayao Li
"""

__all__ = ["DMCollector"]

from ..monitor import *

import binascii
import os
import optparse
import sys
import time
import timeit

from .dm_endec import *
# import dm_collector_c
from . import dm_collector_c

class DMCollector(Monitor):
    """
    A monitor for 3G/4G mobile network protocols (RRC/EMM/ESM).
    It currently supports mobile devices with Qualcomm chipsets.
    """

    #: a list containing the currently supported message types.
    SUPPORTED_TYPES = set(dm_collector_c.log_packet_types)

    def __init__(self, prefs={}):
        """
        Configure this class with user preferences.
        This method should be called before any actual decoding.

        :param prefs: configurations for message decoder. Empty by default.
        :type prefs: dictionary
        """
        Monitor.__init__(self)

        self.phy_baudrate = 9600
        self.phy_ser_name = None
        self._prefs = prefs
        self._type_names = []
        # Initialize Wireshark dissector
        DMLogPacket.init(self._prefs)

    def available_log_types(self):
        """
        Return available log types

        :returns: a list of supported message types
        """
        return self.__class__.SUPPORTED_TYPES

    def set_serial_port(self, phy_ser_name):
        """
        Configure the serial port that dumps messages

        :param phy_ser_name: the serial port name (path)
        :type phy_ser_name: string
        """
        self.phy_ser_name = phy_ser_name

    def set_baudrate(self, rate):
        """
        Configure the baudrate of the serial port

        :param rate: the baudrate of the port
        :type rate: int
        """
        self.phy_baudrate = rate

    def enable_log(self, type_name):
        """
        Enable the messages to be monitored.
        Currently DMCollector supports the following logs:

        :param type_name: the message type(s) to be monitored
        :type type_name: string or list

        :except ValueError: unsupported message type encountered
        """
        cls = self.__class__
        if isinstance(type_name, str):
            type_name = [type_name]
        for n in type_name:
            if n not in cls.SUPPORTED_TYPES:
                self.log_warning("Unsupported log message type: %s" % n)
            if n not in self._type_names:
                self._type_names.append(n)
                self.log_info("Enable collection: " + n)
        dm_collector_c.set_filtered(self._type_names)

    def enable_log_all(self):
        """
        Enable all supported logs
        """
        cls = self.__class__
        self.enable_log(cls.SUPPORTED_TYPES)

    def save_log_as(self, path):
        """
        Save the log as a mi2log file (for offline analysis)

        :param path: the file name to be saved
        :type path: string
        :param log_types: a filter of message types to be saved
        :type log_types: list of string
        """
        dm_collector_c.set_filtered_export(path, self._type_names)

    def run(self):
        """
        Start monitoring the mobile network. This is usually the entrance of monitoring and analysis.

        This function does NOT return or raise any exception.
        """
        assert self.phy_ser_name

        print(("PHY COM: %s" % self.phy_ser_name))
        print(("PHY BAUD RATE: %d" % self.phy_baudrate))

        try:
            # Open the serial port via the C++ layer (no more pyserial here).
            # dm_collector_c.open_serial() configures baud rate, raw mode, and
            # RTS/CTS flow control using POSIX termios.
            self.log_debug("Opening serial port %s at %d baud" % (self.phy_ser_name, self.phy_baudrate))
            dm_collector_c.open_serial(self.phy_ser_name, self.phy_baudrate)

            # Disable logs
            self.log_debug("Disable logs")
            dm_collector_c.disable_logs()

            # Enable logs
            self.log_debug("Enable logs")
            dm_collector_c.enable_logs(self._type_names)

            # --- performance counters ---
            PERF_INTERVAL = 10
            _perf_pkts  = 0
            _perf_cpu0  = time.process_time()
            _perf_wall0 = time.perf_counter()
            # ----------------------------

            # _on_packet is called from C++ once per decoded packet.
            # The entire read/feed/decode loop now runs in C++ (run_loop).
            def _on_packet(decoded_tuple):
                nonlocal _perf_pkts
                try:
                    if not decoded_tuple[0]:
                        return
                    packet = DMLogPacket(decoded_tuple[0])
                    type_id = packet.get_type_id()
                    event = Event(timeit.default_timer(), type_id, packet)
                    self.send(event)

                    _perf_pkts += 1
                    if _perf_pkts % PERF_INTERVAL == 0:
                        cpu  = time.process_time() - _perf_cpu0
                        wall = time.perf_counter()  - _perf_wall0
                        print("[PERF] pkts=%d | "
                              "wall=%.3fs cpu=%.3fs | "
                              "%.1f pkt/s  cpu/pkt=%.3fms" % (
                              _perf_pkts, wall, cpu,
                              _perf_pkts / wall,
                              cpu / _perf_pkts * 1000.0), flush=True)
                except FormatError as e:
                    print(("FormatError: ", e))

            # Blocks here until serial error or KeyboardInterrupt.
            # C++ owns the read/feed/decode loop; Python is only called back
            # once per fully decoded packet instead of 3 times per 64-byte read.
            dm_collector_c.run_loop(self._skip_decoding, _on_packet)

        except (KeyboardInterrupt, RuntimeError) as e:
            print(("\n\n%s Detected: Disabling all logs" % type(e).__name__))
            dm_collector_c.disable_logs()
            dm_collector_c.close_serial()
            sys.exit(e)
        except Exception as e:
            sys.exit(e)
