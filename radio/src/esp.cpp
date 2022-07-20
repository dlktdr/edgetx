
#include "esp.h"

#include <stdlib.h>
#include <string.h>

ESPModule espmodule;
ESPRoot esproot(espmodule);
ESPAudio espaudio(espmodule);
ESPTrainer esptrainer(espmodule);
ESPJoystick espjoystick(espmodule);
ESPTelemetry esptelemetry(espmodule);

// Global ESP Settings
espsettings espSettings;
const espsettingslink espSettingsIndex[] = {
  SETTING_LINK_ARR("name",espSettings.name), // Settings must be 4 characters!
  SETTING_LINK_ARR("wmac",espSettings.wifimac),
  SETTING_LINK_ARR("btma",espSettings.blemac),
  SETTING_LINK_ARR("ssid",espSettings.ssid),
  SETTING_LINK_ARR("ip  ",espSettings.ip),
  SETTING_LINK_ARR("subn",espSettings.subnet),
  SETTING_LINK_ARR("stip",espSettings.staticip),
  SETTING_LINK_VAR("dhcp",espSettings.dhcpMode),
  SETTING_LINK_VAR("wimd",espSettings.wifiStationMode)};

//-----------------------------------------------------------------------------
// AUX Serial Implementation

void (*espReceiveCallBack)(uint8_t *buf, uint32_t len) = nullptr;
void (*espSendCb)(void *, uint8_t) = nullptr;
void *espSendCtx = nullptr;
const etx_serial_driver_t *espSerialDriver = nullptr;
void *espSerialDriverCtx = nullptr;

void espSetSendCb(void *ctx, void (*cb)(void *, uint8_t))
{
  espSendCb = nullptr;
  espSendCtx = ctx;
  espSendCb = cb;
}

inline void espReceiveData(uint8_t *buf, uint32_t len)
{
  espmodule.dataRx(buf, len);
}

void espSetSerialDriver(void *ctx, const etx_serial_driver_t *drv)
{
  espSerialDriver = nullptr;
  espSerialDriverCtx = ctx;
  espSerialDriver = drv;

  if (drv) {
    if (drv->setReceiveCb) drv->setReceiveCb(ctx, espReceiveData);
    espSetSendCb(ctx, drv->sendByte);
  } else {
    espSetSendCb(nullptr, nullptr);
  }
}

//-----------------------------------------------------------------------------

ESPModule::ESPModule()
{
  for (int i = 0; i < ESP_MAX; i++) {
    modes[i] = nullptr;
  }
}

void ESPModule::wakeup()
{
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

  // Call all modules wakeup
  for (int i = 0; i < ESP_MAX; i++) {
    if (modes[i] != nullptr) {
      if (isModeStarted(i)) modes[i]->wakeup();
    }
  }

  // Parse RX Data
  uint8_t inb;
  while (rxFifo.pop(inb)) {
    if (inb == 0 && bufferpos != 0) {
      int lenout = COBS::decode(buffer, bufferpos, (uint8_t *)&packet);
      // ESP_LOG_BUFFER_HEX("P", (uint8_t *)&packet, lenout);
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
        printf("Buffer Overflow\r\n");
        bufferpos = 0;
      }
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

// TODO -- DMA me.. This is called in ISR context
void ESPModule::dataRx(const uint8_t *data, uint32_t len)
{
  // Write everything into a FIFO.. look for null while doing it.
  // If a null is found, set a flag that it's okay to start popping
  // data out of the buffer on wakeup()
  for (uint32_t i = 0; i < len; i++) {
    rxFifo.push(data[i]);
  }
}

void ESPModule::processPacket(const packet_s &packet)
{
  int mode = packet.type & ESP_PACKET_TYPE_MSK;

  // Root Command, Ack/Nak Packet
  if(ESP_PACKET_ISACK(packet.type)) {
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
  if (!esp->isModeStarted(id()) || !esp->hasMode(id()) || len > 255) {
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
  TRACE("ROOT CMD RECEIVED");
  switch (command) {
    // On Receive this means the mode has started
    case ESP_ROOTCMD_START_MODE:
      TRACE("ESP: Unknown root command");
      break;

    // On Receive this means the mode has stopped
    case ESP_ROOTCMD_STOP_MODE:
      TRACE("ESP: Unknown root command");
      break;

    // An Event Was Received
    case ESP_ROOTCMD_CON_EVENT:
      espevent evt;
      evt.event = data[0];
      memcpy(evt.data, data + 1, len < 51 ? len - 1 : 50);
      if (ESPevent != nullptr) {
        ESPevent(&evt, userdata);  // Updates GUI..
      }
      break;

    // A Connection Managment item was received
    case ESP_ROOTCMD_CON_MGMNT:
      /*if(len == sizeof(espsettings)) {
        memcpy(&espSettings, data, sizeof(espsettings));
      } else {
        TRACE("ESP: Settings struct size does not match");
      }*/
      break;

    // A setting was returned
    case ESP_ROOTCMD_SET_VALUE: {
      // First 4 Characters are the Variable, Remainder is the Data
      if(len > 5) {
        char variable[5];
        memcpy(variable, data, 4);
        variable[4] = '\0';
        for(unsigned int i=0; i < SETTINGS_COUNT; i++) {
          if(!strcmp(variable,espSettingsIndex[i].variable)) {
            // Found the variable, make sure it's the same size
            if(len - SETTING_LEN == espSettingsIndex[i].len) {
              memcpy(espSettingsIndex[i].ptr, data+4,len-4);
            }
            break;
          }
        }
      }
      break;
    }

    // Does nothing, all values stored on the ESP
    case ESP_ROOTCMD_GET_VALUE:
      break;

    default:
      TRACE("ESP: Unknown root command");
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
  for(unsigned int i=0; i < SETTINGS_COUNT; i++) {
    writeCommand(ESP_ROOTCMD_GET_VALUE, (uint8_t*)espSettingsIndex[i].variable, SETTING_LEN);
  }
}

// Send all settings
void ESPRoot::sendSettings()
{
  uint8_t buffer[50];
  for(unsigned int i=0; i < SETTINGS_COUNT; i++) {
    memcpy(buffer, espSettingsIndex[i].variable, SETTING_LEN);
    memcpy(buffer+4, espSettingsIndex[i].ptr, espSettingsIndex[i].len);
    writeCommand(ESP_ROOTCMD_SET_VALUE, buffer, espSettingsIndex[i].len+SETTING_LEN);
  }
}