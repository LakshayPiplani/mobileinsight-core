#include "../ws_dissector_client/ws_dissector_client.h"
#include <cstdio>
#include <cstdint>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: exe_name <path to ws dissector bin> <path to libs for ws>\n");
        return 1;
    }

    char* android_pie_ws_dissector = argv[1];


    char* lib_path = argv[2];

    WsDissector ws;

    if (!ws.start(android_pie_ws_dissector, lib_path)) {
        fprintf(stderr, "FAIL: could not start ws_dissector\n");
        return 1;
    }

    const uint8_t probe[] = {0x7e, 0x00};   // NAS-5GS EPD byte + spare
    std::string reply = ws.decode("nas-5gs", probe, sizeof(probe));

    if (reply.empty()) {
        fprintf(stderr, "FAIL: empty response to probe\n");
        return 1;
    }

    printf("%s\n", reply.c_str());
    printf("PASS\n");
    return 0;

}