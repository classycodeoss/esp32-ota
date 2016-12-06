//
//  ota_update.h
//  esp32-ota
//
//  Updating the firmware over the air.
//
//  Created by Andreas Schweizer on 25.11.2016.
//  Copyright © 2016 ClassyCode GmbH. All rights reserved.
//

#ifndef __OTA_UPDATE_H__
#define __OTA_UPDATE_H__ 1

typedef enum {
    OTA_OK = 0,
    OTA_ERR_PARTITION_NOT_FOUND = 1,
    OTA_ERR_PARTITION_NOT_ACTIVATED = 2,
    OTA_ERR_BEGIN_FAILED = 3,
    OTA_ERR_WRITE_FAILED = 4,
    OTA_ERR_END_FAILED = 5,
} TOtaResult;

// Call this function once at the beginning to configure this module.
void otaUpdateInit();

// Call to check if an OTA update is ongoing.
int otaUpdateInProgress();

// Start an OTA update.
TOtaResult otaUpdateBegin();

// Call this function for every line with up to 4 kBytes of hex data.
TOtaResult otaUpdateWriteHexData(const char *hexData);

// Finish an OTA update.
TOtaResult otaUpdateEnd();

void otaDumpInformation();

#endif // __OTA_UPDATE_H__
