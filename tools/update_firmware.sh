#!/bin/bash

#
#  update_firmware.sh
#  esp32-ota
#
#  Script to upload firmware for OTA demo application
#
#  Created by Andreas Schweizer on 02.12.2016.
#  Copyright © 2016 Classy Code GmbH
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


HEXDUMP=/usr/bin/hexdump

#
# Prepare the binary input data for the OTA update.
#

# We need as input arguments the target IP address and binary file to upload.
if [ $# -ne 2 ]
then
  echo "usage: $0 <ip-address> <input.bin>"
  exit -1
fi

echo "Target IP address: $1"
echo "Binary input file: $2"

# Split the binary file in 
$HEXDUMP -v -e '4096/1 "%02x" "\n"' "$2" > /tmp/esp32_ota.hex

NCSUCCESS="OK"
LINECTR=1
NOFLINES="$(/usr/bin/wc -l < /tmp/esp32_ota.hex)"

#
# Tell the ESP32 application that we want to start an OTA update.
#

echo "Starting OTA update on target..."

NCRESULT_START="$(echo '![' | nc $1 80)"
if [[ "$NCRESULT_START" == *"$NCSUCCESS"* ]]
then
  echo "Success!"
else
  echo "Failed, aborting"
  exit -1
fi

#
# Next, we send block by block (each 4096 bytes of data, in hex representation) to the Thing.
# The Thing will store every block in the flash memory.
#

echo "Transferring binary data to target $1..."

while read HEXLINE; do

  DATA="!$HEXLINE\r\n"
  NCRESULT_DATA="$(echo $DATA | nc $1 80)"

  if [[ "$NCRESULT_DATA" == *"$NCSUCCESS"* ]]
  then
    echo "OK $LINECTR / $NOFLINES"
    LINECTR=$[$LINECTR +1]

  else
    echo "ERROR $LINECTR $NCRESULT_DATA"
    exit -1
  fi

done < /tmp/esp32_ota.hex

#
# Last but not least, tell the target that the update has been completed and trigger a re-boot.
#

echo "Finalizing OTA update..."

NCRESULT_FINISH="$(echo '!]'| nc $1 80)"
if [[ "$NCRESULT_FINISH" == *"$NCSUCCESS"* ]]
then
  echo "Success!"
else
  echo "Failed, aborting"
  exit -1
fi

echo "Triggering a Re-boot..."

NCRESULT_REBOOT="$(echo '!*'| nc $1 80)"
if [[ "$NCRESULT_REBOOT" == *"$NCSUCCESS"* ]]
then
  echo "Success!"
else
  echo "Failed"
  exit -1
fi
