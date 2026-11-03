#!/bin/bash

FTP_SERVER="${1:-10.0.0.225}"
LOCAL_FILE="${2:-./ps3vecX.pkg}"
REMOTE_DIR="${3:-/dev_hdd0/packages}"

if [[ ! -f "$LOCAL_FILE" ]]; then
    echo "file not found: $LOCAL_FILE"
    exit 1
fi

curl --fail --show-error \
    --user "anonymous:" \
    --ftp-pasv \
    -T "$LOCAL_FILE" \
    "ftp://${FTP_SERVER}${REMOTE_DIR}/"

if [[ $? -eq 0 ]]; then
    echo "uploaded"
else
    echo "failed"
    exit 1
fi