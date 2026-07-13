#! /usr/bin/bash

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



