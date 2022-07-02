#include "esp.h"

#include "fifo.h"

Fifo<uint8_t, 50>espRxFifo;

ESPJoystick::dataReceived(uint8_t *data, int len)
{
  // No incoming data as a joystick device
}

void ESPJoystick::wakeup() override
{
  channeldata chdat;
  int16_t PPM_range = g_model.extendedLimits ? 640*2 : 512*2;

  int firstCh = g_model.trainerData.channelsStart;
  int lastCh = max(firstCh + MAX_OUTPUT_CHANNELS, MAX_OUTPUT_CHANNELS);

  for (int channel=firstCh; channel<lastCh; channel++) {
    chdat.ch[channel] = PPM_CH_CENTER(channel) + limit((int16_t)-PPM_range, channelOutputs[channel], (int16_t)PPM_range) / 2;
    chdat.channelmask |= 1<<channel;
  }
  write((uint8_t *)&chdat, (int)sizeof(channeldata));
}

