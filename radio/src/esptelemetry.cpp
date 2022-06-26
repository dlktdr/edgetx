#include "opentx.h"
#include "esp.h"

void ESPTelemetry::dataReceived(uint8_t *data, int len)
{
  // Ignore incoming telemetry, for now.
}

void ESPTelemetry::wakeup()
{

}

void ESPTelemetry::pushByte(uint8_t byte)
{
  crc ^= byte;
  if (byte == START_STOP || byte == BYTE_STUFF) {
    buffer[bufferIndex++] = BYTE_STUFF;
    byte ^= STUFF_MASK;
  }
  buffer[bufferIndex++] = byte;
}

void ESPTelemetry::sendSportTelemetry(const uint8_t * packet)
{
  crc = 0x00;

  buffer[bufferIndex++] = START_STOP; // start byte
  for (uint8_t i=0; i<sizeof(SportTelemetryPacket); i++) {
    pushByte(packet[i]);
  }
  buffer[bufferIndex++] = crc;
  buffer[bufferIndex++] = START_STOP; // end byte

  if (bufferIndex >= 2*FRSKY_SPORT_PACKET_SIZE) {
    write(buffer, bufferIndex);
    bufferIndex = 0;
  }

  // If not in verbose mode output one buffer per line
  #if defined(DEBUG_BLUETOOTH) && !defined(DEBUG_BLUETOOTH_VERBOSE)
    BLUETOOTH_TRACE_TIMESTAMP();
    for(int i=0; i < bufferIndex; i++) {
      BLUETOOTH_TRACE(" %02X", buffer[i]);
    }
    BLUETOOTH_TRACE(CRLF);
  #endif
}

void ESPTelemetry::sendRawTelemetry(const uint8_t * dat, int len)
{
  write(dat, len);
}