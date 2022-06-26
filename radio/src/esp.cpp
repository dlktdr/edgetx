
#include "esp.h"
#include "string.h"

// ESP Base and Modes
ESPModule espmodule;
ESPAudio espaudio(espmodule);
ESPTrainer esptrainer(espmodule);
ESPJoystick espjoystick(espmodule);
ESPTelemetry esptelemetry(espmodule);

packet_s packet;

void ESPModule::wakeup()
{
  for(int i=0; i < ESP_MAX; i++) {
    if(modes[i] != nullptr) {
      modes[i]->wakeup();
    }
  }
}

void ESPMode::write(const uint8_t *dat, int len, bool iscmd)
{
  if(!esp->isModeStarted(id()) || !esp->hasMode(id())) {
    ESP_TRACE("[%d]Cannot send data, not supported or started", id());
    return;
  }

  uint8_t encodedbuffer[sizeof(packet_s) + 1];

  if(len > 256) {
    ESP_TRACE("Cannot send packet more than 256 bytes");
    return;
  }

  packet.type = id();
  packet.type |= (iscmd << ESP_PACKET_CMD_BIT);
  packet.len = len;
  packet.crc = 0xAABB;
  memcpy(packet.data, dat, len); // TODO, Remove me, extra copy for just the crc calc.
  packet.crc = crc16(0,(uint8_t *)&packet+1,len + PACKET_OVERHEAD);
  COBS::encode((uint8_t *)&packet+1, packet.len + PACKET_OVERHEAD, encodedbuffer);

  // Write the packet
  esp->write((uint8_t *)&packet, len+5);
}