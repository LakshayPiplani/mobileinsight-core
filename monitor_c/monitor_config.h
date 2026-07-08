/**
 * @file monitor_config.h
 * @author Lakshay Piplani (CSE, Penn State)
 * @date 2026-07-05
 * @brief Captures user-specified preferences
*/

#pragma once
#include <string>
#include <vector>

struct MonitorConfig {
    std::string port_path;               // from set_port()
    int         baud_rate    = 0;        // from set_port()
    std::vector<std::string> type_names; // from enable_log()
    bool        skip_decoding  = false;
    std::string log_output_path;
    double      sampling_rate  = 1.0;
    bool        verbose        = false;  // opt-in debug tracing (setup, I/O, frame classification)
};

