/* ws_dissector_client.cpp — see ws_dissector_client.h */

#include "ws_dissector_client.h"

#include <arpa/inet.h>   // htonl
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <unordered_map>

// Longest payload the dissector will accept (matches the Python wrapper).
static const size_t MAX_PAYLOAD = 3000;

// The sentinel line ws_dissector prints after each message's PDML.
static const char SENTINEL[] = "===___===";

// Name -> AWW protocol number. Mirrors WSDissector.SUPPORTED_TYPES in
// dm_endec/ws_dissector.py; keep in sync with packet-aww.cpp.
static const std::unordered_map<std::string, int>& type_table() {
    static const std::unordered_map<std::string, int> t = {
        // WCDMA RRC
        {"RRC_UL_CCCH", 100}, {"RRC_UL_DCCH", 101}, {"RRC_DL_CCCH", 102},
        {"RRC_DL_DCCH", 103}, {"RRC_DL_BCCH_BCH", 104}, {"RRC_DL_PCCH", 106},
        // WCDMA RRC SysInfo
        {"RRC_MIB", 150}, {"RRC_SIB1", 151}, {"RRC_SIB2", 152},
        {"RRC_SIB3", 153}, {"RRC_SIB5", 155}, {"RRC_SIB7", 157},
        {"RRC_SIB11", 161}, {"RRC_SIB12", 162}, {"RRC_SIB19", 169},
        {"RRC_SB1", 181},
        {"NAS", 190},
        // LTE
        {"LTE-RRC_PCCH", 200}, {"LTE-RRC_DL_DCCH", 201}, {"LTE-RRC_UL_DCCH", 202},
        {"LTE-RRC_BCCH_DL_SCH", 203}, {"LTE-RRC_DL_CCCH", 204},
        {"LTE-RRC_UL_CCCH", 205}, {"LTE-RRC_DL_DCCH_NB", 206},
        {"LTE-RRC_UL_DCCH_NB", 207}, {"LTE-RRC_BCCH_DL_SCH_NB", 208},
        {"LTE-RRC_DL_CCCH_NB", 209}, {"LTE-RRC_UL_CCCH_NB", 210},
        {"LTE-NAS_EPS_PLAIN", 250},
        {"LTE-PDCP_DL_SRB", 300}, {"LTE-PDCP_UL_SRB", 301},
        // 5G NR
        {"nr-rrc.ue_radio_paging_info", 400}, {"nr-rrc.ue_radio_access_cap_info", 401},
        {"nr-rrc.bcch.bch", 402}, {"nr-rrc.bcch.dl.sch", 403},
        {"nr-rrc.dl.ccch", 404}, {"nr-rrc.dl.dcch", 405}, {"nr-rrc.pcch", 406},
        {"nr-rrc.ul.ccch", 407}, {"nr-rrc.ul.ccch1", 408}, {"nr-rrc.ul.dcch", 409},
        {"nr-rrc.rrc_reconf", 410}, {"nr-rrc.ue_mrdc_cap", 411},
        {"nr-rrc.ue_nr_cap", 412}, {"nr-rrc.sbcch.sl.bch", 413},
        {"nr-rrc.scch", 414}, {"nr-rrc.radio_bearer_conf", 415},
        {"nas-5gs", 416},
    };
    return t;
}

int WsDissector::aww_type(const std::string& proto_name) {
    auto it = type_table().find(proto_name);
    return it == type_table().end() ? -1 : it->second;
}

static bool write_all(int fd, const void* buf, size_t n) {
    const char* p = static_cast<const char*>(buf);
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;   // EPIPE etc. => child gone
        }
        p += w;
        n -= (size_t) w;
    }
    return true;
}

bool WsDissector::start(const std::string& executable_path,
                        const std::string& lib_path) {
    if (is_running()) return true;

    int in_pipe[2];   // parent writes in_pipe[1] -> child stdin in_pipe[0]
    int out_pipe[2];  // child stdout out_pipe[1] -> parent reads out_pipe[0]
    if (pipe(in_pipe) != 0) return false;
    if (pipe(out_pipe) != 0) {
        close(in_pipe[0]); close(in_pipe[1]);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        return false;
    }

    if (pid == 0) {
        // ---- child ----
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);

        if (!lib_path.empty()) {
            const char* cur = getenv("LD_LIBRARY_PATH");
            std::string v = lib_path + (cur ? std::string(":") + cur : "");
            setenv("LD_LIBRARY_PATH", v.c_str(), 1);
        }
        execl(executable_path.c_str(), executable_path.c_str(),
              (char*) NULL);
        // exec failed
        _exit(127);
    }

    // ---- parent ----
    close(in_pipe[0]);
    close(out_pipe[1]);
    to_child_   = in_pipe[1];
    from_child_ = out_pipe[0];
    pid_        = pid;

    // A dead child must surface as a write() error, not a fatal SIGPIPE.
    signal(SIGPIPE, SIG_IGN);
    return true;
}

std::string WsDissector::decode(const std::string& proto_name,
                                const uint8_t* data, size_t len) {
    if (!is_running()) return "";
    int type = aww_type(proto_name);
    if (type < 0) return "";           // unsupported protocol
    if (len > MAX_PAYLOAD) return "";  // too large; match Python guard

    uint32_t hdr[2];
    hdr[0] = htonl((uint32_t) type);
    hdr[1] = htonl((uint32_t) len);
    if (!write_all(to_child_, hdr, sizeof(hdr))) return "";
    if (len && !write_all(to_child_, data, len)) return "";

    // Read until the sentinel appears in the accumulated stdout.
    size_t pos;
    while ((pos = readbuf_.find(SENTINEL)) == std::string::npos) {
        char tmp[4096];
        ssize_t n = read(from_child_, tmp, sizeof(tmp));
        if (n <= 0) return "";   // EOF / error => child died
        readbuf_.append(tmp, (size_t) n);
    }

    std::string result = readbuf_.substr(0, pos);
    // Drop everything through the end of the sentinel line.
    size_t nl = readbuf_.find('\n', pos);
    readbuf_.erase(0, nl == std::string::npos ? pos + sizeof(SENTINEL) - 1
                                              : nl + 1);
    return result;
}

void WsDissector::stop() {
    if (to_child_ >= 0)   { close(to_child_);   to_child_ = -1; }
    if (from_child_ >= 0) { close(from_child_); from_child_ = -1; }
    if (pid_ > 0) {
        int status;
        while (waitpid(pid_, &status, 0) < 0 && errno == EINTR) { }
        pid_ = -1;
    }
    readbuf_.clear();
}

WsDissector::~WsDissector() {
    stop();
}
