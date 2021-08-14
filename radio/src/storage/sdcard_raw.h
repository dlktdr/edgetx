#ifndef _SDCARD_RAW_H_
#define _SDCARD_RAW_H_

#include "FatFs/ff.h"

#if !defined(SDCARD_YAML)
#define writeModel           writeModelBin
#define loadRadioSettings    loadRadioSettingsBin
#define writeGeneralSettings writeGeneralSettingsBin
#define readModel            readModelBin
#endif

const char* loadRadioSettingsBin();
const char * writeGeneralSettingsBin();

const char* openFileBin(const char * fullpath, FIL * file, uint16_t * size, uint8_t * version);
const char* readModelBin(const char* filename, uint8_t* buffer, uint32_t size,
                         uint8_t* version);
const char * writeModelBin();

#endif
