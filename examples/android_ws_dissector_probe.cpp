#include "../ws_dissector_client/ws_dissector_client.h"
#include <cstdio>
#include <cstdint>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: exe_name <path to ws dissector bin> <path to libs for ws>");
        return 1;
    }

    char* android_pie_ws_dissector = nullptr;
    *android_pie_ws_dissector = *argv[1];


    char* lib_path = nullptr;
    *lib_path = *argv[2];

    WsDissector* ws_dissector_client = new WsDissector();

    if (!ws_dissector_client->start(android_pie_ws_dissector, lib_path)) {
        fprintf(stderr, "fail: cloud not start WS on Android");
        return 1;
    }

    const uint8_t probe[] = {0x7e, 0x00};   // NAS-5GS EPD byte + spare
    std::string reply = ws_dissector_client->decode("nas-5gs", probe, sizeof(probe));






}