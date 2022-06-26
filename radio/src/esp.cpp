
#include "esp.h"
#include "string.h"

ESPModule espmodule;
ESPAudio espaudio(espmodule);
ESPTrainer esptrainer(espmodule);
ESPJoystick espjoystick(espmodule);
ESPTelemetry esptelemetry(espmodule);

packet_s packet;

// AUX Serial Implementation
void (*espReceiveCallBack)(uint8_t* buf, uint32_t len) = nullptr;
void (*espSendCb)(void*, uint8_t) = nullptr;
void* espSendCtx = nullptr;
const etx_serial_driver_t* espSerialDriver = nullptr;
void* espSerialDriverCtx = nullptr;

void espSetSendCb(void* ctx, void (*cb)(void*, uint8_t))
{
  espSendCb = nullptr;
  espSendCtx = ctx;
  espSendCb = cb;
}

void espReceiveData(uint8_t* buf, uint32_t len)
{
  espmodule.dataRx(buf, len); // TODO: Quick and dirty.. No using two ESP's right now
}

void espSetSerialDriver(void* ctx, const etx_serial_driver_t* drv) {
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

void ESPModule::wakeup()
{
  for(int i=0; i < ESP_MAX; i++) {
    if(modes[i] != nullptr) {
      modes[i]->wakeup();
    }
  }

  writeString("BLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\nBLASTING OUT A HUGE AMOUNT OF DATA\r\n");
}

void ESPModule::write(const uint8_t * data, uint8_t length)
{
  if (!espSendCb) return;

  for(int i=0;i < length; i++) {
    espSendCb(espSendCtx, data[i]);
  }
}

void ESPModule::dataRx(const uint8_t *data, uint32_t len)
{
  // Receive data function, Called from ISR
  TRACE("Got Data %d",len);

  // Write everything into a FIFO.. look for null while doing it.


}

//-----------------------------------------------------------------------------

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