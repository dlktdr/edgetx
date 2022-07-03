#pragma once

#include "opentx.h"
#include "esp.h"
#include "audio.h"
#include "fifo.h"

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

#include "dataconstants.h"
#include <string.h>

#define ESP_BAUDRATE                   576000

#if defined(DEBUG_ESP)
  #define ESP_TRACE(...)  TRACE_NOCRLF(__VA_ARGS__);
#else
  #define ESP_TRACE(...)
#endif

enum ESPModes {
  ESP_ROOT,
  ESP_TELEMETRY,
  ESP_TRAINER,
  ESP_JOYSTICK,
  ESP_AUDIO,
  ESP_FTP,
  ESP_IMU,
  ESP_MAX
};

/* Handles all the interfacing with the module
 *   picking modes, bauds, etc..
 *   The various classes get the data from that mode. A module
 *   could have more than one mode active. Will have to make the
 *   gui select by checkbox?
 *   e.g. IMU + Trainer - Ok
 */
class ESPMode;

#include "definitions.h"

// Packet Format
typedef struct PACKED {
  uint8_t type;
  uint8_t crcl;      // CRC16:low byte
  uint8_t crch;      // CRC16:high byte
  uint8_t data[257]; // User Data
  uint8_t len;       // Data length, not transmitted
} packet_s;

#define PACKET_OVERHEAD 3

/* Packet Type Format
 * Bits 0:4 - Type(32 Possible ESPModes, Mode 0=Base Module)
 *        5 - PartialPacket=1
 *        6 - Command=1/Data=0
 *        7 - Require Ack=1
 * If Command Bit(6) = 1
 *   data[0] = Command
 *   data[1:255] = UserData
 * If command bit(6) = 0
 *   data[0:255] = DataStream
 */

#define ESP_BASE 0
#define ESP_PACKET_TYPE_MSK 0x0F
#define ESP_PACKET_CMD_BIT 6
#define ESP_PACKET_ACK_BIT 7
#define ESP_PACKET_ISCMD(t) (t&(1<<ESP_PACKET_CMD_BIT))
#define ESP_PACKET_ISACKREQ(t) (t&(1<<ESP_PACKET_ACK_BIT))

typedef uint8_t espmode;

void espSetSerialDriver(void* ctx, const etx_serial_driver_t* drv);

class ESPModule
{
  friend class ESPMode;

  public:
    ESPModule();

    void wakeup();
    void reset();
    void startMode(espmode mode);
    void stopMode(espmode mode) {modesStarted &= ~(1<<mode);}
    bool hasMode(espmode mode)
    {
    if(modes[mode] != nullptr) // TODO: Implement Properly
        return true;
      return false;
    }
    bool isModeStarted(espmode mode) {return (modesStarted & (1<<mode));}
    void setModeClass(ESPMode *md, espmode mode)
    {
      if(mode < ESP_MAX)
        modes[mode] = md;
    }

    void dataRx(const uint8_t *data, uint32_t len);

  protected:
    volatile bool packetFound=false;

    Fifo<uint8_t, 1024> rxFifo;

    // Store all available modes
    ESPMode *modes[ESP_MAX];
    int32_t modesStarted=1; // ROOT is always started

    // IO Operations
    void write(const uint8_t * data, uint8_t length);
    char * readline(bool error_reset = true);
    void pushByte(uint8_t byte);
    uint8_t read(uint8_t * data, uint8_t size, uint32_t timeout=1000/*ms*/);
};

// Class that can be used with a mode class to enable discovery and connecting
typedef struct {
  char address[12];
  char name[12];
  uint8_t rssi;
} ESPDevice;

typedef struct {
  uint8_t level; // Info,Warn,Error,Debug
  char message[50];
} ESPMessage;

typedef struct {
  uint8_t event; // Event ID
  uint8_t data[50];
} ESPEvent;

enum ESPRootCmds {
  ESP_ROOTCMD_START_MODE,
  ESP_ROOTCMD_STOP_MODE,
  ESP_ROOTCMD_RESTART,
  ESP_ROOTCMD_COUNT,
};

enum ESPEvents {
  ESP_EVT_DISCOVER_STARTED,
  ESP_EVT_DISCOVER_COMPLETE,
  ESP_EVT_DEVICE_FOUND,
  ESP_EVT_CONNECTED,
  ESP_EVT_DISCONNECTED,
  ESP_EVT_PIN_REQUEST,
  ESP_EVT_IP_OBTAINED
};

enum ESPCommands {
  ESP_CMD_RESTART
};

enum ESPConnectionCommands {
  ESP_EVT_DISCOVER,
  ESP_EVT_CONNECT,
  ESP_EVT_DISCONNECT
};

// Types of connection handlers, every mode will need it's own specific one
//   BLE Central
//   BLE Periferial
//   ESP/EDR Pairing
//   WIFI AP
//   WIFI STA

class ESPConnectionHandler {
  public:
    void startScan() {};
    void stopScan() {};

    void setDeviceFoundCB(void(*c)(ESPDevice cb)) {
      if(c != nullptr)
        deviceDiscoveredCB = c;
    };
    void setConnectedCB(void(*c)(ESPDevice cb)) {
      if(c != nullptr)
        deviceDiscoveredCB = c;
    };
  protected:
    void (*ESPevent)(ESPEvent evt) = nullptr;
    void (*connectedCB)(ESPDevice dev) = nullptr;
    void (*disconnectedCB)() = nullptr;
    void (*deviceDiscoveredCB)(ESPDevice dev) = nullptr;
    void (*discoverCompleteCB)() = nullptr;
    void (*messageCB)(ESPMessage message) = nullptr;
};

class ESPMode
{
  friend class ESPModule;

  public:
    ESPMode(ESPModule &b, uint8_t id) {esp = &b;esp->setModeClass(this,id);}
    virtual uint8_t id()=0;
    virtual void dataReceived(uint8_t *data, int len)=0;
    virtual void cmdReceived(uint8_t command, uint8_t *data, int len)=0;
    virtual void wakeup() {};

  protected:
    ESPModule *esp=nullptr;
    packet_s packet;
    void write(const uint8_t *dat, int len, bool iscmd=false);
    void writeCommand(uint8_t command, const uint8_t *dat=nullptr, int len=0);
};

// ESP Root Commands
class ESPRoot : public ESPMode
{
  public:
    ESPRoot(ESPModule &b) : ESPMode(b,id()) {}
    uint8_t id() {return ESP_ROOT;}
    void dataReceived(uint8_t *data, int len) {}
    void cmdReceived(uint8_t command, uint8_t *data, int len);

    // Send the radios channels
    void send(uint16_t chans[16]) {}
};

// BLE Joystick
class ESPJoystick : public ESPMode, public ESPConnectionHandler
{
  public:
    ESPJoystick(ESPModule &b) : ESPMode(b,id()) {}
    uint8_t id() {return ESP_JOYSTICK;}
    void dataReceived(uint8_t *data, int len) {}
    void cmdReceived(uint8_t command, uint8_t *data, int len) {}

    // Send the radios channels
    void send(uint16_t chans[16]) {}
};

// Telemetry Output
#define BLUETOOTH_LINE_LENGTH           32

class ESPTelemetry : public ESPMode, public ESPConnectionHandler
{
  public:
    ESPTelemetry(ESPModule &b) : ESPMode(b,id()) {}
    uint8_t id() {return ESP_TELEMETRY;}
    void sendSportTelemetry(const uint8_t * packet);
    void sendRawTelemetry(const uint8_t * dat, int len);
    void wakeup() override;
    void dataReceived(uint8_t *data, int len);
    void cmdReceived(uint8_t command, uint8_t *data, int len) {}

  protected:
    void pushByte(uint8_t byte);
    uint8_t buffer[BLUETOOTH_LINE_LENGTH+1];
    uint8_t bufferIndex = 0;
    uint8_t crc=0;
};

// A2DP - Audio Source
class ESPAudio : public ESPMode, public ESPConnectionHandler
{
  public:
    ESPAudio(ESPModule &b) : ESPMode(b,id()) {}
    uint8_t id() {return ESP_AUDIO;}
    void wakeup() override;
    void dataReceived(uint8_t *data, int len) {}
    void cmdReceived(uint8_t command, uint8_t *data, int len) {}
    void sendAudio(const AudioBuffer *aud);
};

// WIFI File Transfer
class ESPFtp : public ESPMode, public ESPConnectionHandler
{
  public:
    ESPFtp(ESPModule &b) : ESPMode(b,id()) {}
    uint8_t id() {return ESP_FTP;}
    void dataReceived(uint8_t *data, int len) {}
    void cmdReceived(uint8_t command, uint8_t *data, int len) {}
};

// Reads IMU data from a imu sensor, not a connectable mode
class ESPImu : public ESPMode
{
  public:
    ESPImu(ESPModule &b) : ESPMode(b,id()) {}
    void dataReceived(uint8_t *data, int len) {}
    void cmdReceived(uint8_t command, uint8_t *data, int len) {}

    uint8_t id() {return ESP_IMU;}
};

// Trainer Master/Slave
class ESPTrainer : public ESPMode, public ESPConnectionHandler
{
  public:
    ESPTrainer(ESPModule &b) : ESPMode(b,id()) {}
    uint8_t id() {return ESP_TRAINER;}
    void wakeup() override;
    void dataReceived(uint8_t *data, int len);
    void cmdReceived(uint8_t command, uint8_t *data, int len) {}
    void setTrainerMaster() {isMaster=true;}
    void setTrainerSlave() {isMaster=false;}

  protected:
    bool isMaster=true;
    void sendTrainer();
};

extern ESPModule espmodule;
extern ESPAudio espaudio;
extern ESPTrainer esptrainer;
extern ESPJoystick espjoystick;
extern ESPTelemetry esptelemetry;


// Copyright (c) 2011 Christopher Baker <https://christopherbaker.net>
// Copyright (c) 2011 Jacques Fortier <https://github.com/jacquesf/COBS-Consistent-Overhead-Byte-Stuffing>
//
// SPDX-License-Identifier: MIT
//

/// \brief A Consistent Overhead Byte Stuffing (COBS) Encoder.
///
/// Consistent Overhead Byte Stuffing (COBS) is an encoding that removes all 0
/// bytes from arbitrary binary data. The encoded data consists only of bytes
/// with values from 0x01 to 0xFF. This is useful for preparing data for
/// transmission over a serial link (RS-232 or RS-485 for example), as the 0
/// byte can be used to unambiguously indicate packet boundaries. COBS also has
/// the advantage of adding very little overhead (at least 1 byte, plus up to an
/// additional byte per 254 bytes of data). For messages smaller than 254 bytes,
/// the overhead is constant.
///
/// \sa http://conferences.sigcomm.org/sigcomm/1997/papers/p062.pdf
/// \sa http://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing
/// \sa https://github.com/jacquesf/COBS-Consistent-Overhead-Byte-Stuffing
/// \sa http://www.jacquesf.com/2011/03/consistent-overhead-byte-stuffing
class COBS
{
public:
  static int encode(const uint8_t* buffer,
                    int size,
                    uint8_t* encodedBuffer)
  {
    int read_index  = 0;
    int write_index = 1;
    int code_index  = 0;
    uint8_t code    = 1;

    while (read_index < size) {
      if (buffer[read_index] == 0) {
        encodedBuffer[code_index] = code;
        code = 1;
        code_index = write_index++;
        read_index++;
      } else {
        encodedBuffer[write_index++] = buffer[read_index++];
        code++;
        if (code == 0xFF) {
            encodedBuffer[code_index] = code;
            code = 1;
            code_index = write_index++;
        }
      }
    }
    encodedBuffer[code_index] = code;
    return write_index;
  }

  static int decode(const uint8_t* encodedBuffer,
                    int size,
                    uint8_t* decodedBuffer)  {
    if (size == 0)
        return 0;
    int read_index  = 0;
    int write_index = 0;
    uint8_t code       = 0;
    uint8_t i          = 0;
    while (read_index < size) {
      code = encodedBuffer[read_index];
      if (read_index + code > size && code != 1) {
        return 0;
      }
      read_index++;
      for (i = 1; i < code; i++) {
        decodedBuffer[write_index++] = encodedBuffer[read_index++];
      }
      if (code != 0xFF && read_index != size) {
        decodedBuffer[write_index++] = '\0';
      }
    }

    return write_index;
  }
};

// Channel Format
typedef struct  {
  int16_t ch[32];
  uint32_t channelmask=0; // Valid Channels
} channeldata;