/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include "popups.h"

enum BluetoothStates {
#if defined(PCBX9E)
  BLUETOOTH_INIT,
  BLUETOOTH_WAIT_TTM,
  BLUETOOTH_WAIT_BAUDRATE_CHANGE,
#endif
  BLUETOOTH_STATE_OFF,
  BLUETOOTH_STATE_FACTORY_BAUDRATE_INIT,
  BLUETOOTH_STATE_BAUDRATE_SENT,
  BLUETOOTH_STATE_BAUDRATE_INIT,
  BLUETOOTH_STATE_NAME_SENT,
  BLUETOOTH_STATE_POWER_SENT,
  BLUETOOTH_STATE_DISCMOD_REQUESTED,
  BLUETOOTH_STATE_DISCMOD_COMPLETE,
  BLUETOOTH_STATE_ROLE_SENT,
  BLUETOOTH_STATE_IDLE,
  BLUETOOTH_STATE_DISCOVER_REQUESTED,
  BLUETOOTH_STATE_DISCOVER_SENT,
  BLUETOOTH_STATE_DISCOVER_START,
  BLUETOOTH_STATE_DISCOVER_END,
  BLUETOOTH_STATE_BIND_REQUESTED,
  BLUETOOTH_STATE_CONNECT_SENT,
  BLUETOOTH_STATE_CONNECTED,
  BLUETOOTH_STATE_DISCONNECTED,
  BLUETOOTH_STATE_CLEAR_REQUESTED,
  BLUETOOTH_STATE_FLASH_FIRMWARE
};

#define LEN_BLUETOOTH_ADDR              16
#define MAX_BLUETOOTH_DISTANT_ADDR      6
#define BLUETOOTH_PACKET_SIZE           14
#define BLUETOOTH_LINE_LENGTH           32
#define BLUETOOTH_TRAINER_CHANNELS      8

#if defined(LOG_BLUETOOTH)
  #define BLUETOOTH_TRACE(...)  \
    f_printf(&g_bluetoothFile, __VA_ARGS__); \
    TRACE_NOCRLF(__VA_ARGS__);
#else
#if defined(DEBUG_BLUETOOTH)
  #define BLUETOOTH_TRACE(...)  TRACE_NOCRLF(__VA_ARGS__);
  #define BLUETOOTH_TRACE_TIMESTAMP(f_, ...)  debugPrintf((TRACE_TIME_FORMAT f_), TRACE_TIME_VALUE, ##__VA_ARGS__)
#if defined(DEBUG_BLUETOOTH_VERBOSE)
  #define BLUETOOTH_TRACE_VERBOSE(...) TRACE_NOCRLF(__VA_ARGS__);
#else
  #define BLUETOOTH_TRACE_VERBOSE(...)
#endif
#else
  #define BLUETOOTH_TRACE(...)
  #define BLUETOOTH_TIMESTAMP(f_, ...)
  #define BLUETOOTH_TRACE_VERBOSE(...)
#endif
#endif

/* Handles all the interfacing with the module
 *   picking modes, bauds, etc..
 *   The various classes get the data from that mode
 */

class Bluetooth
{
  public:
    Bluetooth() {
      for(int i=0; i < BLUETOOTH_MAX ; i++) {
       modes[i] = nullptr;
      }
    }

    void writeString(const char * str);

    void forwardTelemetry(const uint8_t * packet);
    void wakeup();
    const char * flashFirmware(const char * filename, ProgressHandler progressHandler);

    volatile uint8_t state;
    char localAddr[LEN_BLUETOOTH_ADDR+1];
    char distantAddr[LEN_BLUETOOTH_ADDR+1];

    void setMode(uint8_t mode;)
    uint8_t mode() {return currentmode;}
    bool hasMode(uint8_t mode)
    {
      if(modes[mode] != nullptr) // TODO: Implement Properly
        return true;
      return false;
    }

  protected:
    friend class BTMode;

    // Store all available modes
    BTMode *modes[BLUETOOTH_MAX];
    uint8_t currentmode = 0;
    void setMode(BTMode *md, uint8_t mode)
    {
      if(mode < BLUETOOTH_MAX)
        modes[mode] = md;
    }

    // IO Operations
    void write(const uint8_t * data, uint8_t length);
    char * readline(bool error_reset = true);
    void pushByte(uint8_t byte);
    uint8_t read(uint8_t * data, uint8_t size, uint32_t timeout=1000/*ms*/);

    // REMOVE ME...
    uint8_t bootloaderChecksum(uint8_t command, const uint8_t * data, uint8_t size);
    void bootloaderSendCommand(uint8_t command, const void *data = nullptr, uint8_t size = 0);
    void bootloaderSendCommandResponse(uint8_t response);
    const char * bootloaderWaitCommandResponse(uint32_t timeout=1000/*ms*/);
    const char * bootloaderWaitResponseData(uint8_t *data, uint8_t size);
    const char * bootloaderSetAutoBaud();
    const char * bootloaderReadStatus(uint8_t &status);
    const char * bootloaderCheckStatus();
    const char * bootloaderSendData(const uint8_t * data, uint8_t size);
    const char * bootloaderEraseFlash(uint32_t start, uint32_t size);
    const char * bootloaderStartWriteFlash(uint32_t start, uint32_t size);
    const char * bootloaderWriteFlash(const uint8_t * data, uint32_t size);
    const char * doFlashFirmware(const char * filename, ProgressHandler progressHandler);

    uint8_t buffer[BLUETOOTH_LINE_LENGTH+1];
    uint8_t bufferIndex = 0;
    tmr10ms_t wakeupTime = 0;
    uint8_t crc;
};

class BTMode
{
  public:
    BTMode(Bluetooth &b) {
      bt = &b;
      b.setMode(this,id());
      }
    virtual uint8_t id()=0;
    virtual void dataReceived(uint8_t *data, int len)=0;
    virtual void wakeup();

  protected:
    Bluetooth *bt=nullptr;
    void write(uint8_t *data, int len);
    void write(const char *str);
};

class BTJoystick : public BTMode
{
  public:
    BTJoystick(Bluetooth &b) : BTMode(b) {}
    uint8_t id() {return BLUETOOTH_JOYSTICK;}
    void dataReceived(uint8_t *data, int len) {}
};

class BTTelemetry : public BTMode
{
  public:
    BTTelemetry(Bluetooth &b) : BTMode(b) {}
    uint8_t id() {return BLUETOOTH_TELEMETRY;}
    void dataReceived(uint8_t *data, int len) {}
};

class BTAudio : public BTMode
{
  public:
    BTAudio(Bluetooth &b) : BTMode(b) {}
    uint8_t id() {return BLUETOOTH_AUDIO;}
    void dataReceived(uint8_t *data, int len) {}

};

class BTFtp : public BTMode // TODO
{
  public:
    BTFtp(Bluetooth &b) : BTMode(b) {}
    uint8_t id() {return BLUETOOTH_FTP;}
    void dataReceived(uint8_t *data, int len) {}
};

class BTTrainer : public BTMode
{
  public:
    BTTrainer(Bluetooth &b) : BTMode(b) {}
    uint8_t id() {return BLUETOOTH_TRAINER;}
    void dataReceived(uint8_t *data, int len);
    void setTrainerModeMaster(bool m) {isMaster=m;}

  protected:
    bool isMaster=true;
    void appendTrainerByte(uint8_t data);
    void processTrainerFrame(const uint8_t * buffer);
    void processTrainerByte(uint8_t data);
    void sendTrainer();
    void receiveTrainer();

};

extern Bluetooth bluetooth;
extern BTAudio(bluetooth) btaudio;
extern BTTrainer(bluetooth) bttrainer;

