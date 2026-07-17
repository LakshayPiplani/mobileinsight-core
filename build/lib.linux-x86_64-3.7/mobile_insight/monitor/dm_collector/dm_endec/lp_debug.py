# Debugging Script
# Find the message value, both raw bytes and decoded from WS for NAS MM messages


def dump_message(fd: object, type_id:str, raw_msg: str, decoded_msg: str):
    fd.write(f"Type id is {type_id}\n")
    fd.write(f"Raw message is {raw_msg}\n")
    fd.write(f"Decoded message is {decoded_msg}\n\n")
    fd.flush()


