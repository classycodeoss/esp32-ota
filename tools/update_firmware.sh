#!/bin/bash

/usr/bin/hexdump -v -e '4096/1 "%02x" "\n"' logocontrol.bin > /tmp/logocontrol.hex

NCSUCCESS="OK"
LINECTR=1
NOFLINES="$(/usr/bin/wc -l < /tmp/logocontrol.hex)"

#
# Tell the ESP32 application that we want to start an OTA update.
#

echo "Starting OTA update on target..."

NCRESULT_START="$(echo '!['| nc 192.168.33.52 80)"
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

echo "Transferring binary data to target..."

while read HEXLINE; do

  DATA="!$HEXLINE\r\n"
  NCRESULT_DATA="$(echo $DATA | nc 192.168.33.52 80)"

  if [[ "$NCRESULT_DATA" == *"$NCSUCCESS"* ]]
  then
    echo "OK $LINECTR / $NOFLINES"
    LINECTR=$[$LINECTR +1]

  else
    echo "ERROR $LINECTR $NCRESULT_DATA"
    exit -1
  fi

done < /tmp/logocontrol.hex

#
# Last but not least, tell the target that the update has been completed and trigger a re-boot.
#

echo "Finalizing OTA update..."

NCRESULT_FINISH="$(echo '!]'| nc 192.168.33.52 80)"
if [[ "$NCRESULT_FINISH" == *"$NCSUCCESS"* ]]
then
  echo "Success!"
else
  echo "Failed, aborting"
  exit -1
fi

echo "Triggering a Re-boot..."

NCRESULT_REBOOT="$(echo '!*'| nc 192.168.33.52 80)"
if [[ "$NCRESULT_REBOOT" == *"$NCSUCCESS"* ]]
then
  echo "Success!"
else
  echo "Failed"
  exit -1
fi
