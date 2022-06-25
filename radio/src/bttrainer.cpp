#include "bttrainer.h"
#include "fifo.h"

Fifo<uint8_t, 50>btRxFifo;

BTTrainer::dataReceived(uint8_t *data, int len)
{
  for(int i=0; i < len; i++) {
    processTrainerByte(data[i])
  }
}

void BTTrainer::processTrainerFrame(const uint8_t * buffer)
{
  uint16_t btchannels[BLUETOOTH_TRAINER_CHANNELS];
  for (uint8_t channel=0, i=1; channel<BLUETOOTH_TRAINER_CHANNELS; channel+=2, i+=3) {
    // +-500 != 512, but close enough.
    btchannels[channel] = buffer[i] + ((buffer[i+1] & 0xf0) << 4) - 1500;
    btchannels[channel+1] = ((buffer[i+1] & 0x0f) << 4) + ((buffer[i+2] & 0xf0) >> 4) + ((buffer[i+2] & 0x0f) << 8) - 1500;
  }

  setTrainerData(btchannels, BLUETOOTH_TRAINER_CHANNELS);
}

void BTTrainer::appendTrainerByte(uint8_t data)
{
  if (bufferIndex < BLUETOOTH_LINE_LENGTH) {
    buffer[bufferIndex++] = data;
    // we check for "DisConnected", but the first byte could be altered (if received in state STATE_DATA_XOR)
    if (data == '\n') {
      if (bufferIndex >= 13) {
        if (!strncmp((char *)&buffer[bufferIndex-13], "isConnected", 11)) {
          BLUETOOTH_TRACE("BT< DisConnected" CRLF);
          state = BLUETOOTH_STATE_DISCONNECTED;
          bufferIndex = 0;
          wakeupTime += 200; // 1s
        }
      }
    }
  }
}

void BTTrainer::processTrainerByte(uint8_t data)
{
  static uint8_t dataState = STATE_DATA_IDLE;

  switch (dataState) {
    case STATE_DATA_START:
      if (data == START_STOP) {
        dataState = STATE_DATA_IN_FRAME;
        bufferIndex = 0;
      }
      else {
        appendTrainerByte(data);
      }
      break;

    case STATE_DATA_IN_FRAME:
      if (data == BYTE_STUFF) {
        dataState = STATE_DATA_XOR; // XOR next byte
      }
      else if (data == START_STOP) {
        dataState = STATE_DATA_IN_FRAME;
        bufferIndex = 0;
      }
      else {
        appendTrainerByte(data);
      }
      break;

    case STATE_DATA_XOR:
      switch (data) {
        case BYTE_STUFF ^ STUFF_MASK:
        case START_STOP ^ STUFF_MASK:
          // Expected content, save the data
          appendTrainerByte(data ^ STUFF_MASK);
          dataState = STATE_DATA_IN_FRAME;
          break;
        case START_STOP:  // Illegal situation, as we have START_STOP, try to start from the beginning
          bufferIndex = 0;
          dataState = STATE_DATA_IN_FRAME;
          break;
        default:
          // Illegal situation, start looking for a new START_STOP byte
          dataState = STATE_DATA_START;
          break;
      }
      break;

    case STATE_DATA_IDLE:
      if (data == START_STOP) {
        bufferIndex = 0;
        dataState = STATE_DATA_START;
      }
      else {
        appendTrainerByte(data);
      }
      break;
  }

  if (bufferIndex >= BLUETOOTH_PACKET_SIZE) {
    uint8_t crc = 0x00;
    for (int i = 0; i < BLUETOOTH_PACKET_SIZE - 1; i++) {
      crc ^= buffer[i];
    }
    if (crc == buffer[BLUETOOTH_PACKET_SIZE - 1]) {
      if (buffer[0] == TRAINER_FRAME) {
        processTrainerFrame(buffer);
      }
    }
    dataState = STATE_DATA_IDLE;
  }
}

void BTTrainer::pushByte(uint8_t byte)
{
  crc ^= byte;
  if (byte == START_STOP || byte == BYTE_STUFF) {
    buffer[bufferIndex++] = BYTE_STUFF;
    byte ^= STUFF_MASK;
  }
  buffer[bufferIndex++] = byte;
}

void BTTrainer::sendTrainer()
{
  int16_t PPM_range = g_model.extendedLimits ? 640*2 : 512*2;

  int firstCh = g_model.trainerData.channelsStart;
  int lastCh = firstCh + BLUETOOTH_TRAINER_CHANNELS;

  uint8_t * cur = buffer;
  bufferIndex = 0;
  crc = 0x00;

  buffer[bufferIndex++] = START_STOP; // start byte
  pushByte(TRAINER_FRAME);
  for (int channel=firstCh; channel<lastCh; channel+=2, cur+=3) {
    uint16_t channelValue1 = PPM_CH_CENTER(channel) + limit((int16_t)-PPM_range, channelOutputs[channel], (int16_t)PPM_range) / 2;
    uint16_t channelValue2 = PPM_CH_CENTER(channel+1) + limit((int16_t)-PPM_range, channelOutputs[channel+1], (int16_t)PPM_range) / 2;
    pushByte(channelValue1 & 0x00ff);
    pushByte(((channelValue1 & 0x0f00) >> 4) + ((channelValue2 & 0x00f0) >> 4));
    pushByte(((channelValue2 & 0x000f) << 4) + ((channelValue2 & 0x0f00) >> 8));
  }
  buffer[bufferIndex++] = crc;
  buffer[bufferIndex++] = START_STOP; // end byte

  write(buffer, bufferIndex);

  // If not in verbose mode output one buffer per line
  #if defined(DEBUG_BLUETOOTH) && !defined(DEBUG_BLUETOOTH_VERBOSE)
    BLUETOOTH_TRACE_TIMESTAMP();
    for(int i=0; i < bufferIndex; i++) {
      BLUETOOTH_TRACE(" %02X", buffer[i]);
    }
    BLUETOOTH_TRACE(CRLF);
  #endif

  bufferIndex = 0;
}
