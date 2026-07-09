#!/usr/bin/python
# Filename: offline_replayer.py
"""
An offline log replayer

Author: Jiayao Li,
        Yuanjie Li
Update: Yunqi Guo, 2020/06, for MI5
"""

__all__ = ["OfflineReplayer"]

import os
import sys
import time
import timeit

from .dm_collector import dm_collector_c, DMLogPacket, FormatError
from .monitor import Monitor, Event


class OfflineReplayer(Monitor):
    """
    A log replayer for offline analysis.
    """

    SUPPORTED_TYPES = set(dm_collector_c.log_packet_types)

    def __test_android(self):
        try:
            from jnius import autoclass, cast  # For Android
            # try:
            #     self.service_context = autoclass('org.kivy.android.PythonService').mService
            #     if not self.service_context:
            #         self.service_context = autoclass("org.kivy.android.PythonActivity").mActivity
            # except Exception, e:
            #     self.service_context = autoclass("org.kivy.android.PythonActivity").mActivity
            self.is_android = True
            try:
                from service import mi2app_utils
                self.service_context = autoclass(
                    'org.kivy.android.PythonService').mService
            except Exception as e:
                self.service_context = None

        except Exception as e:
            # not used, but bugs may exist on laptop
            self.is_android = False

    def __init__(self):
        Monitor.__init__(self)

        self.is_android = False
        self.service_context = None

        self.__test_android()

        if self.is_android:
            libs_path = self.__get_libs_path()

            prefs = {
                "ws_dissect_executable_path": os.path.join(
                    libs_path,
                    "android_pie_ws_dissector"),
                "libwireshark_path": libs_path}
        else:
            prefs = {}

        DMLogPacket.init(prefs)

        self._type_names = []

    def __del__(self):
        if self.is_android and self.service_context:
            print("detaching...")
            from service import mi2app_utils
            mi2app_utils.detach_thread()

    # def __get_cache_dir(self):
    #     if self.is_android:
    #         return str(self.service_context.getCacheDir().getAbsolutePath())
    #     else:
    #         return ""

    def __get_libs_path(self):
        if self.is_android and self.service_context:
            return os.path.join(
                self.service_context.getFilesDir().getAbsolutePath(), "app/data")
        else:
            return "./data"

    def available_log_types(self):
        """
        Return available log types

        :returns: a list of supported message types
        """
        return self.__class__.SUPPORTED_TYPES

    def set_sampling_rate(self, sampling_rate):
        dm_collector_c.set_sampling_rate(sampling_rate)

    def enable_log(self, type_name):
        """
        Enable the messages to be monitored. Refer to cls.SUPPORTED_TYPES for supported types.

        If this method is never called, the config file existing on the SD card will be used.

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
                self.log_info("Enable " + n)
        dm_collector_c.set_filtered(self._type_names)

    def enable_log_all(self):
        """
        Enable all supported logs
        """
        cls = self.__class__
        self.enable_log(cls.SUPPORTED_TYPES)

    def set_input_path(self, path):
        """
        Set the replay trace path

        :param path: the replay file path. If it is a directory, the OfflineReplayer will read all logs under this directory (logs in subdirectories are ignored)
        :type path: string
        """
        dm_collector_c.reset()
        self._input_path = path
        # self._input_file = open(path, "rb")

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
        """

        # fd = open('./Diag.cfg','wb')
        # dm_collector_c.generate_diag_cfg(fd, self._type_names)
        # fd.close()

        try:

            self.broadcast_info('STARTED', {})
            self.log_info('STARTED: ' + str(time.time()))
            log_list = []
            if os.path.isfile(self._input_path):
                log_list = [self._input_path]
            elif os.path.isdir(self._input_path):
                for file in os.listdir(self._input_path):
                    if file.endswith(".mi2log") or file.endswith(".qmdl"):
                        # log_list.append(self._input_path+"/"+file)
                        log_list.append(os.path.join(self._input_path, file))
            else:
                return

            log_list.sort()  # Hidden assumption: logs follow the diag_log_TIMSTAMP_XXX format

            # --- performance counters ---
            # Same sandwich as DMCollector.run(): the timed window is
            #   file read -> feed_binary -> receive_log_packet -> DMLogPacket
            # accumulated per iteration; type_id lookup / Event dispatch /
            # analyzers / printing are outside the window. cpu = accumulated
            # process CPU inside the window; wall (for pkt/s) = overall
            # elapsed since the loop started (across all files in log_list).
            PERF_INTERVAL = 10
            _perf_pkts  = 0
            _perf_reads = 0
            _perf_cpu_acc  = 0.0
            _perf_wall_acc = 0.0
            _perf_wall0 = time.perf_counter()
            # ----------------------------

            for file in log_list:
                self.log_info("Loading " + file)
                self.log_info('Loading: ' + str(time.time()))
                self._input_file = open(file, "rb")
                dm_collector_c.reset()
                while True:
                    _t_cpu0  = time.process_time()   # sandwich: open
                    _t_wall0 = time.perf_counter()

                    s = self._input_file.read(64)
                    _perf_reads += 1

                    if s:
                        dm_collector_c.feed_binary(s)

                    decoded = dm_collector_c.receive_log_packet(self._skip_decoding,
                                                                True,   # include_timestamp
                                                                )
                    packet = None
                    try:
                        if decoded and decoded[0]:
                            packet = DMLogPacket(decoded[0])
                    except FormatError as e:
                        # skip this packet
                        print(("FormatError: ", e))
                        packet = None

                    _perf_cpu_acc  += time.process_time() - _t_cpu0   # sandwich: close
                    _perf_wall_acc += time.perf_counter() - _t_wall0

                    if not s and not decoded:
                        # EOF encountered and no message can be received any more
                        break

                    if packet is None:
                        continue

                    try:
                        type_id = packet.get_type_id()

                        _perf_pkts += 1
                        if _perf_pkts % PERF_INTERVAL == 0:
                            wall = time.perf_counter() - _perf_wall0
                            print("[PERF] pkts=%d reads=%d | "
                                  "wall=%.3fs cpu=%.3fs | "
                                  "%.1f pkt/s  cpu/pkt=%.3fms" % (
                                  _perf_pkts, _perf_reads,
                                  wall, _perf_cpu_acc,
                                  _perf_pkts / wall,
                                  _perf_cpu_acc / _perf_pkts * 1000.0), flush=True)

                        if type_id in self._type_names or type_id == "Custom_Packet":
                            event = Event(timeit.default_timer(),
                                          type_id,
                                          packet)
                            self.send(event)

                    except FormatError as e:
                        # skip this packet
                        print(("FormatError: ", e))
                self._input_file.close()

        except Exception as e:
            import traceback
            sys.exit(str(traceback.format_exc()))
            # sys.exit(e)
        event = Event(timeit.default_timer(), 'Monitor.STOP', None)
        self.send(event)
        self.log_info("Offline replay is completed.")
