#!/usr/bin/env bash
set -euo pipefail

echo "Cloning pre-built wireshark executable and supporting binaries"

hash_id="eda41f72b56341e3ab44c3c5847eb9661cb2644f"
temp_dir="./temp/"
destination_dir="./android_prebuilt"

rm -rf ${temp_dir}
mkdir -p ${temp_dir}

# if [[ -d ${destination_dir}]]; then
#     echo "Pre-built bins have already been downloaded. Early-stopping here."
#     exit 0
# fi
rm -rf ${destination_dir}
mkdir -p ${destination_dir}

git clone -b ws-3.4 --depth 1 https://github.com/mobile-insight/mobileinsight-libs.git ${temp_dir}

retrieved_hash_id=$(git -C ${temp_dir} rev-parse HEAD)

echo "retrieved hash id is ${retrieved_hash_id} and required hash id is ${hash_id}"

if [[ ${hash_id} == ${retrieved_hash_id} ]]; then
    echo "Hashes matched. Good binaries"
else 
    echo "Hashes do not match. Bad binaries"
    exit 1
fi

mkdir -p ${temp_dir}/extracted
tar -xzvf ${temp_dir}/lib.tar.gz -C ${temp_dir}/extracted

mkdir -p ${destination_dir}/lib
cp ${temp_dir}/extracted/lib/*.so ${destination_dir}/lib

mkdir -p ${destination_dir}/bin
cp ${temp_dir}/bin/android_pie_ws_dissector ${destination_dir}/bin

rm -rf ${temp_dir}

echo ""
echo "=== self-check: file types (expect ELF 32-bit LSB ..., ARM, EABI5 for all 9) ==="
file ${destination_dir}/lib/*.so ${destination_dir}/bin/*

echo ""
echo "=== self-check: NEEDED entries for android_pie_ws_dissector ==="
echo "(expect exactly: libwireshark.so, libwiretap.so, libwsutil.so, libglib-2.0.so, libm.so, libc.so, libdl.so)"
readelf -d ${destination_dir}/bin/android_pie_ws_dissector | grep NEEDED

echo ""
echo "Done. Artifacts in ${destination_dir}/ (gitignored, not committed)."

