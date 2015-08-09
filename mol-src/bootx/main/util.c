/* 
 *   MOL adaption of BootX
 *
 *   (C) 2001, 2002 Samuel Rydh, <samuel@ibrium.se>
 */
/*
 * Copyright (c) 2000, 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "sl.h"

unsigned long 
Alder32(unsigned char *buffer, long length)
{
	long          cnt;
	unsigned long result, lowHalf, highHalf;
  
	lowHalf = 1;
	highHalf = 0;
	
	for (cnt = 0; cnt < length; cnt++) {
		if ((cnt % 5000) == 0) {
			lowHalf  %= 65521L;
			highHalf %= 65521L;
		}
    
		lowHalf += buffer[cnt];
		highHalf += lowHalf;
	}

	lowHalf  %= 65521L;
	highHalf %= 65521L;
  
	result = (highHalf << 16) | lowHalf;
  
	return result;
}

long
ConvertFileSpec(char *fileSpec, char *devSpec, char **filePath)
{
	long cnt;
  
	// Find the first ':' in the fileSpec.
	cnt = 0;
	while ((fileSpec[cnt] != '\0') && (fileSpec[cnt] != ':')) cnt++;
	if (fileSpec[cnt] == '\0') return -1;
  
	// Find the next ',' in the fileSpec.
	while ((fileSpec[cnt] != '\0') && (fileSpec[cnt] != ',')) cnt++;
  
	// Copy the string to devSpec.
	strncpy(devSpec, fileSpec, cnt);
	devSpec[cnt] = '\0';
  
	// If there is a filePath start it after the ',', otherwise NULL.
	if (filePath != NULL) {
		if (fileSpec[cnt] != '\0') {
			*filePath = fileSpec + cnt + 1;
		} else {
			*filePath = NULL;
		}
	}
	return 0;
}

long GetDeviceType(char *devSpec)
{
	CICell ph;
	long   size;
	char   deviceType[32];
  
	ph = FindDevice(devSpec);
	if (ph == -1) return -1;
  
	size = GetProp(ph, "device_type", deviceType, 31);
	if (size != -1) deviceType[size] = '\0';
	else deviceType[0] = '\0';
  
	if (strcmp(deviceType, "network") == 0) return kNetworkDeviceType;
	if (strcmp(deviceType, "block") == 0) return kBlockDeviceType;
  
	return kUnknownDeviceType;
}
