#include "opentx.h"
#include "opentx_helpers.h"
#include "esp.h"
#include "audio.h"

// 12Bits Per Sample * 32Khz = 385,00kbps = 48,000Kb/s
// (48,000 / 256(Bytes per packet)) = 187.5 Packets
// Packet overhead = 4bytes + PacketHeader('\0') + Cobs = 6bytes
// 48,000 + 187.5*6 = 49,125Bytes p/second
// UART 8N1 @ 576000 baud = 57,600 bytes/second
// 49,125 / 57,600 = 85% of available bandwidth.

void ESPAudio::wakeup()
{

}

void ESPAudio::sendAudio(const AudioBuffer *aud)
{
  // Send Buffer in 256 byte increments
  int offset=0;
  while(offset <= aud->size) {
    int minsz = min((int)aud->size,(int)256);
    write((uint8_t *)aud->data + offset, minsz);
    offset += minsz;
  }
}