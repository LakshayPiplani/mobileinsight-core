#! /usr/bin/sh

echo "Cloning pre-built wireshark executable and supporting binaries"

hash_id="eda41f72b56341e3ab44c3c5847eb9661cb2644f"
temp_dir="./temp/"

rm -rf ${temp_dir}
mkdir -p ${temp_dir}

git clone -b ws-3.4 --depth 1 https://github.com/mobile-insight/mobileinsight-libs.git ${temp_dir}

retrieved_hash_id=$(git -C ${temp_dir} rev-parse HEAD)

echo "retrieved hash id is ${retrieved_hash_id} and required hash id is ${hash_id}"

if [[ ${hash_id} == ${retrieved_hash_id} ]]; then
    echo "Hashes matched. Good binaries"
    exit 0
else 
    echo "Hashes do not match. Bad binaries"
    exit 1
fi