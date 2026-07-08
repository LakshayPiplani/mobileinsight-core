/* log_packet_helper.h
 * Pure-C++ helpers for decoding binary log packets into a FieldList tree.
 * Replaces the former CPython (PyObject*) implementation.
 */

#ifndef __DM_COLLECTOR_C_LOG_PACKET_HELPER_H__
#define __DM_COLLECTOR_C_LOG_PACKET_HELPER_H__

#include "consts.h"
#include "log_packet.h"
#include "../field_value.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <sstream>
#include <fstream>

#ifdef __ANDROID__
#include <android/log.h>
#define printf(fmt,args...) __android_log_print(ANDROID_LOG_INFO, "python", fmt, ##args);
#endif

#define SSTR(x) std::to_string(x)

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

#define BYTE_TO_BINARY_LITTLE_ENDIAN(byte)  \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0'), \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0')

// Read the integer alternative of a FieldValue, accepting a stored double.
static int64_t _field_value_as_int(const FieldValue &v)
__attribute__ ((unused));

static int64_t
_field_value_as_int(const FieldValue &v) {
    if (std::holds_alternative<int64_t>(v.data))
        return std::get<int64_t>(v.data);
    if (std::holds_alternative<double>(v.data))
        return (int64_t) std::get<double>(v.data);
    assert(false);  // field does not hold a number
    return 0;
}

// Find a field by its name in a result list.
// Return: i or -1
static int
_find_result_index(const FieldList &result, const char *target) {
    for (size_t i = 0; i < result.size(); i++) {
        if (result[i].name == target)
            return (int) i;
    }
    return -1;
}

// Find a field by its name in a result list.
// Return: pointer to the field's value, or NULL
static const FieldValue *
_search_result(const FieldList &result, const char *target) {
    int i = _find_result_index(result, target);
    return (i >= 0) ? &result[i].value : NULL;
}

// Find an integer field by its name in a result list, and return a C int with
// the same value (overflow is ignored).
// Return: int
static int _search_result_int(
        const FieldList &result,
        const char *target)
__attribute__ ((unused));

static int
_search_result_int(const FieldList &result, const char *target) {
    const FieldValue *item = _search_result(result, target);
    assert(item != NULL);
    return (int) _field_value_as_int(*item);
}

// This function should be called when the value is 8 bytes long.
// Return: unsigned long int
static unsigned long int _search_result_ulongint(
        const FieldList &result,
        const char *target)
__attribute__ ((unused));

static unsigned long int
_search_result_ulongint(const FieldList &result, const char *target) {
    const FieldValue *item = _search_result(result, target);
    assert(item != NULL);
    return (unsigned long int) _field_value_as_int(*item);
}

// This function should be called when the value is 4 bytes long.
// Return: unsigned int
static unsigned int _search_result_uint(
        const FieldList &result,
        const char *target)
__attribute__ ((unused));

static unsigned int
_search_result_uint(const FieldList &result, const char *target) {
    const FieldValue *item = _search_result(result, target);
    assert(item != NULL);
    return (unsigned int) _field_value_as_int(*item);
}

static std::string _search_result_bytestream(
        const FieldList &result,
        const char *target)
__attribute__ ((unused));

static std::string
_search_result_bytestream(const FieldList &result, const char *target) {
    const FieldValue *item = _search_result(result, target);
    assert(item != NULL && std::holds_alternative<std::string>(item->data));
    return std::get<std::string>(item->data);
}

// Find a field in a result list and replace its value (type_hint reset to "").
// Return: true if the field was found
static bool
_replace_result(FieldList &result, const char *target, FieldValue new_value) {
    int i = _find_result_index(result, target);
    if (i >= 0) {
        result[i].value = std::move(new_value);
        result[i].type_hint = "";
        return true;
    }
    return false;
}

static void
_delete_result(FieldList &result, const char *target) {
    int i = _find_result_index(result, target);
    if (i >= 0) {
        result.erase(result.begin() + i);
    }
}

// Find a field in a result list and replace it with an integer.
static void _replace_result_int(
        FieldList &result,
        const char *target,
        long long new_int)
__attribute__ ((unused));

static void
_replace_result_int(FieldList &result, const char *target, long long new_int) {
    _replace_result(result, target, FieldValue{int64_t(new_int)});
}

// Find a field in a result list and replace it with a double.
static void _replace_result_double(
        FieldList &result,
        const char *target,
        double new_val)
__attribute__ ((unused));

static void
_replace_result_double(FieldList &result, const char *target, double new_val) {
    _replace_result(result, target, FieldValue{new_val});
}

static void _replace_result_string(
        FieldList &result,
        const char *target,
        const std::string &new_string)
__attribute__ ((unused));

static void
_replace_result_string(FieldList &result, const char *target,
                       const std::string &new_string) {
    _replace_result(result, target, FieldValue{new_string});
}

// Search a field that has a value of integer type, and map this integer to
// a string, which replace the original integer.
// If there is no correponding string, or if the mapping is NULL, map this
// integer to *not_found*.
// Return: old number
static int _map_result_field_to_name(
        FieldList &result,
        const char *target,
        const ValueName mapping[],
        int n,
        const char *not_found)
__attribute__ ((unused));

static int
_map_result_field_to_name(FieldList &result, const char *target,
                          const ValueName mapping[], int n,
                          const char *not_found) {
    int i = _find_result_index(result, target);
    if (i >= 0) {
        int val = (int) _field_value_as_int(result[i].value);

        const char *name = search_name(mapping, n, val);
        if (name == NULL)  // not found
            name = not_found;
        result[i].value = FieldValue{std::string(name)};
        result[i].type_hint = "";
        return val;
    } else {
        return -1;
    }
}

// Convert a QCDM timestamp (raw ticks / 52428800.0 seconds since
// 1980-01-06 00:00:00) to an ISO-8601 string, e.g.
// "1980-01-06T00:04:23.125000". 315964800 is the POSIX time of the epoch.
static std::string _qcdm_timestamp_to_iso(
        long long seconds,
        int useconds)
__attribute__ ((unused));

static std::string
_qcdm_timestamp_to_iso(long long seconds, int useconds) {
    time_t t = (time_t) (315964800LL + seconds);
    struct tm tm_utc;
    gmtime_r(&t, &tm_utc);
    char buf[40];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%06d",
             tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
             tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec, useconds);
    return std::string(buf);
}

static unsigned int _decode_by_bit(
        int start,
        int bitlength,
        const char *b)
__attribute__ ((unused));

static unsigned int
_decode_by_bit(int start, int bitlength, const char *b) {
    unsigned int rt = 0;
    int temp = 0;
    assert(bitlength <= 32);
    const char *p = b + start / 8;
    int usedByte = (start + bitlength + 8 - 1) / 8 - start / 8;
    unsigned char buffer[5] = {0};
    memcpy(buffer, p, sizeof(char) * usedByte);

    int left_remaining = 8 - start % 8;
    int right_drop = 8 - (start + bitlength) % 8;
    if (right_drop == 8) {
        right_drop = 0;
    }

    if (usedByte > 1) {
        temp = (int) buffer[0] & ((int) std::pow(2, left_remaining) - 1);
        rt += temp * std::pow(2, bitlength - left_remaining);
        for (int i = 1; i < usedByte - 1; i++) {
            temp = (int) buffer[i];
            rt += temp * pow(2, bitlength - left_remaining - 8 * i);
        }
        temp = (int) buffer[usedByte - 1] >> right_drop;
        rt += temp;
    } else {
        temp = ((int) buffer[0] >> right_drop);
        temp = temp & ((int) std::pow(2, bitlength) - 1);
        rt = temp;
    }
    return rt;
}

// Decode a binary string according to an array of field description (fmt[]).
// Decoded fields are appended to result
static int _decode_by_fmt(
        const Fmt fmt[],
        int n_fmt,
        const char *b,
        int offset,
        int length,
        FieldList &result)
__attribute__ ((unused));

static int
_decode_by_fmt(const Fmt fmt[], int n_fmt,
               const char *b, int offset, int length,
               FieldList &result) {
    (void) length;
    int n_consumed = 0;

    for (int i = 0; i < n_fmt; i++) {
        const char *p = b + offset + n_consumed;
        switch (fmt[i].type) {
            case UINT: {
                unsigned int ii = 0;
                unsigned long long iiii = -1LL;
                switch (fmt[i].len) {
                    case 0:
                        // Placeholder field (e.g. "Subframe Number", "FTL SNR"):
                        // consumes no bytes; overwritten later via _replace_result.
                        ii = 0;
                        break;
                    case 1:
                        ii = *((unsigned char *) p);
                        break;
                    case 2:
                        ii = *((unsigned short *) p);
                        break;
                    case 3:
                        // 24-bit little-endian (e.g. NrRrcOtaPacketFmt "Unknown")
                        ii = (unsigned int) (unsigned char) p[0]
                           | ((unsigned int) (unsigned char) p[1] << 8)
                           | ((unsigned int) (unsigned char) p[2] << 16);
                        break;
                    case 4:
                        ii = *((unsigned int *) p);
                        break;
                    case 6:
                        // 48-bit little-endian (e.g. "Maching ID")
                        iiii = 0;
                        memcpy(&iiii, p, 6);
                        break;
                    case 8:
                        memcpy(&iiii, p, sizeof(unsigned long long));
                        break;
                    default:
                        if (fmt[i].len > 8) {
                            // Wider than 64 bits (e.g. 16-byte ciphering keys):
                            // keep the low 64 bits; all bytes are still consumed.
                            memcpy(&iiii, p, sizeof(unsigned long long));
                        } else {
                            assert(false);
                        }
                        break;
                }
                if (fmt[i].len <= 4) {
                    result.push_back({fmt[i].field_name,
                                      FieldValue{int64_t(ii)}, ""});
                } else {
                    result.push_back({fmt[i].field_name,
                                      FieldValue{int64_t(iiii)}, ""});
                }
                n_consumed += fmt[i].len;
                break;
            }
            case UINT_BIG_ENDIAN: {
                unsigned int ii = 0;
                unsigned long long iiii = -1LL;
                char p_reverse[8];
                for (int j = 0; j < fmt[i].len; j++) {
                    p_reverse[j] = p[fmt[i].len - 1 - j];
                }
                switch (fmt[i].len) {
                    case 1:
                        ii = *((unsigned char *) p_reverse);
                        break;
                    case 2:
                        ii = *((unsigned short *) p_reverse);
                        break;
                    case 3:
                        // 24-bit big-endian (bytes already reversed above)
                        ii = (unsigned int) (unsigned char) p_reverse[0]
                           | ((unsigned int) (unsigned char) p_reverse[1] << 8)
                           | ((unsigned int) (unsigned char) p_reverse[2] << 16);
                        break;
                    case 4:
                        ii = *((unsigned int *) p_reverse);
                        break;
                    case 8:
                        memcpy(&iiii, p, sizeof(unsigned long long));
                        break;
                    default:
                        assert(false);
                        break;
                }
                if (fmt[i].len <= 4) {
                    result.push_back({fmt[i].field_name,
                                      FieldValue{int64_t(ii)}, ""});
                } else {
                    result.push_back({fmt[i].field_name,
                                      FieldValue{int64_t(iiii)}, ""});
                }
                n_consumed += fmt[i].len;
                break;
            }

            case BYTE_STREAM: {
                assert(fmt[i].len > 0);
                char hex[10] = {};
                std::string ascii_data = "0x";
                for (int k = 0; k < fmt[i].len; k++) {
                    sprintf(hex, "%02x", p[k] & 0xFF);
                    ascii_data += hex;
                }
                result.push_back({fmt[i].field_name,
                                  FieldValue{ascii_data}, ""});
                n_consumed += fmt[i].len;
                break;
            }

            case BYTE_STREAM_LITTLE_ENDIAN: {
                assert(fmt[i].len > 0);
                char hex[10] = {};
                std::string ascii_data = "0x";
                for (int k = fmt[i].len - 1; k >= 0; k--) {
                    sprintf(hex, "%02x", p[k] & 0xFF);
                    ascii_data += hex;
                }
                result.push_back({fmt[i].field_name,
                                  FieldValue{ascii_data}, ""});
                n_consumed += fmt[i].len;
                break;
            }

            case BIT_STREAM: {
                assert(fmt[i].len > 0);
                char hex[10] = {};
                std::string ascii_data = "";
                for (int k = 0; k < fmt[i].len; k++) {
                    sprintf(hex, BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(p[k] & 0xFF));
                    ascii_data += hex;
                }
                result.push_back({fmt[i].field_name,
                                  FieldValue{ascii_data}, ""});
                n_consumed += fmt[i].len;
                break;
            }

            case BIT_STREAM_LITTLE_ENDIAN: {
                assert(fmt[i].len > 0);
                char hex[10] = {};
                std::string ascii_data = "";
                for (int k = fmt[i].len - 1; k >= 0; k--) {
                    sprintf(hex, BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(p[k] & 0xFF));
                    ascii_data += hex;
                }
                result.push_back({fmt[i].field_name,
                                  FieldValue{ascii_data}, ""});
                n_consumed += fmt[i].len;
                break;
            }
            case PLMN_MK1: {
                assert(fmt[i].len == 6);
                const char *plmn = p;
                char s[32];
                snprintf(s, sizeof(s), "%d%d%d-%d%d%d",
                         plmn[0], plmn[1], plmn[2],
                         plmn[3], plmn[4], plmn[5]);
                result.push_back({fmt[i].field_name,
                                  FieldValue{std::string(s)}, ""});
                n_consumed += fmt[i].len;
                break;
            }

            case PLMN_MK2: {
                /*
                 * Yunqi: Rewrite concatenate method for plmn
                 */
                assert(fmt[i].len == 3);
                const char *plmn = p;
                int last_digit = (plmn[1] >> 4) & 0x0F;
                char s[32];
                // MNC can have two or three digits
                if (last_digit < 10) {
                    // last digit exists
                    snprintf(s, sizeof(s), "%d%d%d-%d%d%d",
                             plmn[0] & 0x0F,
                             (plmn[0] >> 4) & 0x0F,
                             plmn[1] & 0x0F,
                             plmn[2] & 0x0F,
                             (plmn[2] >> 4) & 0x0F,
                             last_digit);
                } else {
                    snprintf(s, sizeof(s), "%d%d%d-%d%d",
                             plmn[0] & 0x0F,
                             (plmn[0] >> 4) & 0x0F,
                             plmn[1] & 0x0F,
                             plmn[2] & 0x0F,
                             (plmn[2] >> 4) & 0x0F);
                }
                result.push_back({fmt[i].field_name,
                                  FieldValue{std::string(s)}, ""});
                n_consumed += fmt[i].len;
                break;
            }

            case QCDM_TIMESTAMP: {
                const double PER_SECOND = 52428800.0;
                const double PER_USECOND = 52428800.0 / 1.0e6;
                assert(fmt[i].len == 8);
                unsigned long long iiii;
                memcpy(&iiii, p, sizeof(unsigned long long));
                int seconds = int(double(iiii) / PER_SECOND);
                int useconds = (double(iiii) / PER_USECOND) - double(seconds) * 1.0e6;
                result.push_back({fmt[i].field_name,
                                  FieldValue{_qcdm_timestamp_to_iso(seconds, useconds)},
                                  ""});
                n_consumed += fmt[i].len;
                break;
            }

            case BANDWIDTH: {
                assert(fmt[i].len == 1);
                unsigned int ii = *((unsigned char *) p);
                char s[16];
                snprintf(s, sizeof(s), "%d MHz", ii / 5);
                result.push_back({fmt[i].field_name,
                                  FieldValue{std::string(s)}, ""});
                n_consumed += fmt[i].len;
                break;
            }

            case RSRP: {
                // (0.0625 * x - 180) dBm
                assert(fmt[i].len == 2);
                short val = *((short *) p);
                result.push_back({fmt[i].field_name,
                                  FieldValue{double(val * 0.0625 - 180)}, ""});
                n_consumed += fmt[i].len;
                break;
            }

            case RSRQ: {
                // (0.0625 * x - 30) dB
                assert(fmt[i].len == 2);
                short val = *((short *) p);
                result.push_back({fmt[i].field_name,
                                  FieldValue{double(val * 0.0625 - 30)}, ""});
                n_consumed += fmt[i].len;
                break;
            }

            case WCDMA_MEAS: {   // (x-256) dBm
                assert(fmt[i].len == 1);
                unsigned int ii = *((unsigned char *) p);
                result.push_back({fmt[i].field_name,
                                  FieldValue{int64_t((int) ii - 256)}, ""});
                n_consumed += fmt[i].len;
                break;
            }

            case SKIP:
                n_consumed += fmt[i].len;
                break;

            case PLACEHOLDER: {
                assert(fmt[i].len == 0);
                result.push_back({fmt[i].field_name,
                                  FieldValue{int64_t(0)}, ""});
                break;
            }

            default:
                assert(false);
                break;
        }
    }
    return n_consumed;
}

// Debug printer for a FieldList tree (replacement for the old PyObject repr).
static void reprint(const FieldList &result, int indent)
__attribute__ ((unused));

static void
reprint(const FieldList &result, int indent = 0) {
    for (const FieldEntry &e : result) {
        printf("%*s%s (%s): ", indent, "", e.name.c_str(), e.type_hint.c_str());
        if (std::holds_alternative<int64_t>(e.value.data)) {
            printf("%lld\n", (long long) std::get<int64_t>(e.value.data));
        } else if (std::holds_alternative<double>(e.value.data)) {
            printf("%f\n", std::get<double>(e.value.data));
        } else if (std::holds_alternative<std::string>(e.value.data)) {
            printf("%s\n", std::get<std::string>(e.value.data).c_str());
        } else if (std::holds_alternative<std::vector<uint8_t>>(e.value.data)) {
            printf("<%zu raw bytes>\n", std::get<std::vector<uint8_t>>(e.value.data).size());
        } else {
            printf("\n");
            reprint(std::get<FieldList>(e.value.data), indent + 2);
        }
    }
}

static void
_convert_nr_rsrp(FieldList &obj, const char *rsrp_field) {
    int utemp = _search_result_uint(obj, rsrp_field);
    float rsrp = utemp * 0.0078 - 0.0003;           // TODO: Based on polyfit. To be more accurate
    _replace_result(obj, rsrp_field, FieldValue{double(rsrp)});
}

static void
_convert_nr_rsrq(FieldList &obj, const char *rsrq_field) {
    int utemp = _search_result_uint(obj, rsrq_field);
    float rsrq = utemp * 0.0078 - 0.0003;           // TODO: Based on polyfit. To be more accurate
    _replace_result(obj, rsrq_field, FieldValue{double(rsrq)});
}

#endif // __DM_COLLECTOR_C_LOG_PACKET_HELPER_H__
