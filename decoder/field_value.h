/* field_value.h
 * The decoded-value tree shared by every decoder (Qualcomm, future MTK).
 * Replaces the Python result list of (name, value, type_hint) 3-tuples.
 *
 * type_hint semantics (same encoding as the original Python lists):
 *   ""                — scalar leaf (int64_t, double, or std::string)
 *   "list"            — value is a FieldList of homogeneous "dict" entries
 *   "dict"            — value is a FieldList of mixed fields for one record
 *   "raw_msg/PROTO"   — value is raw bytes; PROTO names the dissector
 */

#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

struct FieldEntry;
using FieldList = std::vector<FieldEntry>;

struct FieldValue {
    using V = std::variant<
        int64_t,               // all UINT fields (1/2/4/8 bytes)
        double,                // RSRP, RSRQ, computed floats
        std::string,           // BYTE_STREAM hex, PLMN, datetime as ISO-8601
        std::vector<uint8_t>,  // raw_msg bytes (opaque dissector payload)
        FieldList              // nested list or dict (recursion via vector's
                               // heap storage; the variant itself is fixed-size)
    >;
    V data;
};

struct FieldEntry {
    std::string name;
    FieldValue  value;
    std::string type_hint; // "", "list", "dict", or "raw_msg/PROTO"
};
