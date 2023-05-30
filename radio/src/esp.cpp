#include "esp.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS/include/FreeRTOS.h"
#include "FreeRTOS/include/task.h"

ESPModule espmodule;
ESPRoot esproot(espmodule);
ESPAudio espaudio(espmodule);
ESPTrainer esptrainer(espmodule);
ESPJoystick espjoystick(espmodule);
ESPTelemetry esptelemetry(espmodule);

// Global ESP Settings
espsettings espSettings;
const espsettingslink espSettingsIndex[] = {
    SETTING_LINK_ARR("name", espSettings.name),  // Settings must be 4 characters!
    SETTING_LINK_ARR("wmac", espSettings.wifimac),
    SETTING_LINK_ARR("btma", espSettings.blemac),
    SETTING_LINK_ARR("ssid", espSettings.ssid),
    SETTING_LINK_ARR("ip  ", espSettings.ip),
    SETTING_LINK_ARR("subn", espSettings.subnet),
    SETTING_LINK_ARR("stip", espSettings.staticip),
    SETTING_LINK_VAR("dhcp", espSettings.dhcpMode),
    SETTING_LINK_VAR("wimd", espSettings.wifiStationMode)};

//-----------------------------------------------------------------------------
// AUX Serial Implementation

// void (*espReceiveCallBack)(uint8_t *buf, uint32_t len) = nullptr;
void (*espSendCb)(void *, uint8_t) = nullptr;
void *espSendCtx = nullptr;
const etx_serial_driver_t *espSerialDriver = nullptr;
void *espSerialDriverCtx = nullptr;

void espSetSendCb(void *ctx, void (*cb)(void *, uint8_t))
{
  espSendCtx = ctx;
  espSendCb = cb;
}

void espSetSerialDriver(void *ctx, const etx_serial_driver_t *drv)
{
  espSerialDriverCtx = ctx;
  espSerialDriver = drv;

  if (drv) {
    espSetSendCb(ctx, drv->sendByte);
  } else {
    espSetSendCb(nullptr, nullptr);
  }
}

void InitalizeMessageBuffer() {}

//-----------------------------------------------------------------------------

ESPModule::ESPModule()
{
  for (int i = 0; i < ESP_MAX; i++) {
    modes[i] = nullptr;
  }
  InitalizeMessageBuffer();
}

void ESPModule::wakeup()
{
  // Do nothing if serial driver not iniatlized
  if (espSerialDriverCtx == nullptr) return;

  // TODO Rework this and GUI to allow multiple modes active if compatible.
  if (currentmode != g_eeGeneral.espMode) {
    if (currentmode != 0) {
      stopMode(currentmode);
      currentmode = 0;
    }

    // User switched modes
    switch (g_eeGeneral.espMode) {
      case ESP_ROOT:
        break;
      case ESP_TELEMETRY:
        break;
      case ESP_TRAINER:
        startMode(ESP_TRAINER);
        currentmode = ESP_TRAINER;
        break;
      case ESP_JOYSTICK:
        startMode(ESP_JOYSTICK);
        currentmode = ESP_JOYSTICK;
        break;
      case ESP_AUDIO:
        startMode(ESP_AUDIO);
        currentmode = ESP_AUDIO;
        break;
      case ESP_FTP:
        break;
      case ESP_IMU:
        break;
    }

    g_eeGeneral.espMode = currentmode;
  }

  // Parse Receieved Packets
  auto _getByte = espSerialDriver->getByte;
  if (!_getByte) return;
  uint8_t inb;
  while (_getByte(espSerialDriverCtx, &inb)) {
    if (inb == 0 && bufferpos != 0) {  // Find the null
      int lenout = COBS::decode(buffer, bufferpos, (uint8_t *)&packet);
      uint16_t packetcrc = packet.crcl | (packet.crch << 8);
      packet.crcl = 0xBB;
      packet.crch = 0xAA;
      uint16_t calccrc = crc16(0, (uint8_t *)&packet, lenout, 0);
      packet.len = lenout - PACKET_OVERHEAD;
      if (packetcrc == calccrc) {
        processPacket(packet);
      } else {
        TRACE("CRC Fault");
      }
      bufferpos = 0;
    } else {
      buffer[bufferpos++] = inb;
      if (bufferpos == sizeof(buffer)) {
        TRACE("Buffer Overflow");
        bufferpos = 0;
      }
    }
  }

  // Call all modules wakeup
  for (int i = 0; i < ESP_MAX; i++) {
    if (modes[i] != nullptr) {
      if (isModeStarted(i)) modes[i]->wakeup();
    }
  }
}

void ESPModule::write(const uint8_t *data, uint8_t length)
{
  if (!espSendCb) return;
  for (int i = 0; i < length; i++) {
    espSendCb(espSendCtx, data[i]);
  }
}

void ESPModule::processPacket(const packet_s &packet)
{
  int mode = packet.type & ESP_PACKET_TYPE_MSK;

  // Root Command, Ack/Nak Packet
  if (ESP_PACKET_ISACK(packet.type)) {
    // Last command sent was received, Module is connected, allow more commands
    // to be sent. Reset the timer

    // TODO
    return;
  }

  if (mode < ESP_MAX && modes[mode] != nullptr) {
    if (ESP_PACKET_ISCMD(packet.type)) {
      modes[mode]->cmdReceived(packet.data[0], packet.data + 1, packet.len - 1);
    } else {
      modes[mode]->dataReceived(packet.data, packet.len);
    }
  }
}

void ESPModule::startMode(espmode mode)
{
  // TEMP
  modesStarted |= 1 << mode;

  if (modes[ESP_ROOT] == nullptr) return;

  modes[ESP_ROOT]->writeCommand(ESP_ROOTCMD_START_MODE, (uint8_t *)&mode, 1);
}

void ESPModule::stopMode(espmode mode)
{
  // TEMP
  modesStarted &= ~(1 << mode);

  if (modes[ESP_ROOT] == nullptr) return;

  modes[ESP_ROOT]->writeCommand(ESP_ROOTCMD_STOP_MODE, (uint8_t *)&mode, 1);
}

// Full module reboot
void ESPModule::reset()
{
  if (modes[ESP_ROOT] == nullptr) return;
  modes[ESP_ROOT]->writeCommand(ESP_ROOTCMD_RESTART);
}

//-----------------------------------------------------------------------------

void ESPMode::writeCommand(uint8_t command, const uint8_t *dat, int len)
{
  if (len > 253) {
    TRACE("PACKET TOO LONG!");
    return;
  }
  uint8_t *buffer = (uint8_t *)malloc(len + 1);
  buffer[0] = command;  // First byte of a command is the command, data follows
  memcpy(buffer + 1, dat, len);
  write(buffer, len + 1, true);
  free(buffer);
}

void ESPMode::write(const uint8_t *dat, int len, bool iscmd)
{
  if (!esp->isModeStarted(id()) || !esp->hasMode(id()) ||
      len > ESP_MAX_PACKET_DATA) {
    return;
  }

  uint8_t encodedbuffer[sizeof(packet_s) + 1];

  packet.type = id();
  packet.type |= (iscmd << ESP_PACKET_CMD_BIT);
  packet.crcl = 0xBB;
  packet.crch = 0xAA;
  memcpy(packet.data, dat, len);
  uint16_t crc = crc16(0, (uint8_t *)&packet, len + PACKET_OVERHEAD);
  packet.crcl = crc & 0xFF;
  packet.crch = (crc & 0xFF00) >> 8;
  int wl =
      COBS::encode((uint8_t *)&packet, len + PACKET_OVERHEAD, encodedbuffer);
  encodedbuffer[wl] = '\0';
  // Write the packet
  esp->write(encodedbuffer, wl + 1);
}

//-----------------------------------------------------------------------------

void ESPRoot::cmdReceived(uint8_t command, const uint8_t *data, int len)
{
  TRACE("ESP: RX root command,");

  switch (command) {
    // On Receive this means the mode has started
    case ESP_ROOTCMD_START_MODE:
      if(len == 1)  {
        TRACE("    Mode Started %s", STR_ESP_MODES[*data - 1]);
      } else {
        TRACE("    Invalid data len on Start Mode");
      }
      break;

    // On Receive this means the mode has stopped
    case ESP_ROOTCMD_STOP_MODE:
      if(len == 1)  {
        TRACE("    Mode Stopped %s", STR_ESP_MODES[*data -1]);
      } else {
        TRACE("    Invalid data len on Stop Mode");
      }
      break;

    case ESP_ROOTCMD_ACTIVE_MODES:
      if(len == 4) { // 32 bits (32 max modes)
        uint32_t* udata32 = (uint32_t*)data;
        TRACE_NOCRLF("    Modes Running " );
        for(int i=ESP_TELEMETRY; i < ESP_MAX; i++) { // Skip Root
          if(*udata32 & 1 << i)
            TRACE_NOCRLF("%s ", STR_ESP_MODES[i - 1]);
        }
        TRACE_NOCRLF("\r\n");
      }
      break;

    // An Event Was Received ESP_EVT_xxxx
    case ESP_ROOTCMD_CON_EVENT:
      TRACE("    Connection Event");
      espevent evt;
      evt.event = data[0];
      memcpy(evt.data, data + 1, len < 51 ? len - 1 : 50);
      if (ESPevent != nullptr) {
        ESPevent(&evt, userdata);  // Updates GUI..
      }
      break;

    // Does nothing
    case ESP_ROOTCMD_CON_MGMNT:
      TRACE("    Connection Command.. It shouldn't do this");
      break;

    // A setting was returned
    case ESP_ROOTCMD_SET_VALUE: {
      TRACE("    Setting Value");
      // First 4 Characters are the Variable, Remainder is the Data
      if (len > 5) {
        char variable[5];
        memcpy(variable, data, 4);
        variable[4] = '\0';
        for (unsigned int i = 0; i < SETTINGS_COUNT; i++) {
          if (!strcmp(variable, espSettingsIndex[i].variable)) {
            // Found the variable, make sure it's the same size
            if (len - SETTING_LEN == espSettingsIndex[i].len) {
              memcpy(espSettingsIndex[i].ptr, data + 4, len - 4);
            }
            break;
          }
        }
      }
      break;
    }

    // Does nothing, all values stored on the ESP
    case ESP_ROOTCMD_GET_VALUE:
      TRACE("    Requesting value.. It shouldn't Do This");
      break;

    default:
      TRACE("    Unknown root command");
      break;
  }
}

void ESPRoot::setConnMgrValue(uint8_t value, char *data, int len)
{
  uint8_t *buffer = (uint8_t *)malloc(len + 1);
  if (!buffer) return;
  buffer[0] = value;
  memcpy(buffer + 1, data, len);
  writeCommand(ESP_ROOTCMD_CON_MGMNT, buffer, len + 1);
  free(buffer);
}

// Request all Settings
void ESPRoot::getSettings()
{
  for (unsigned int i = 0; i < SETTINGS_COUNT; i++) {
    writeCommand(ESP_ROOTCMD_GET_VALUE, (uint8_t *)espSettingsIndex[i].variable,
                 SETTING_LEN);
  }
}

// Send all settings
void ESPRoot::sendSettings()
{
  uint8_t buffer[50];
  for (unsigned int i = 0; i < SETTINGS_COUNT; i++) {
    memcpy(buffer, espSettingsIndex[i].variable, SETTING_LEN);
    memcpy(buffer + 4, espSettingsIndex[i].ptr, espSettingsIndex[i].len);
    writeCommand(ESP_ROOTCMD_SET_VALUE, buffer,
                 espSettingsIndex[i].len + SETTING_LEN);
  }
}