//
//  network.h
//  esp32-ota
//
//  Networking code for OTA demo application
//
//  Created by Andreas Schweizer on 02.12.2016.
//  Copyright © 2016 ClassyCode GmbH. All rights reserved.
//

#ifndef __NETWORK_H__
#define __NETWORK_H__ 1

// Call this function once at the beginning to configure this module.
void networkInit();

// Try to connect asynchronously to the defined access point.
void networkConnect(const char *ssid, const char *password);

// Check if the module is currently connected to an access point.
int networkIsConnected();

#endif // __NETWORK_H__
