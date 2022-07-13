#include "esp.h"
#include "fifo.h"

#define JOYSTICK_MAX_CHANNELS 8

void ESPJoystick::wakeup()
{
  channeldata chdat;
  int16_t PPM_range = g_model.extendedLimits ? 640*2 : 512*2;

  int firstCh = g_model.trainerData.channelsStart;
  int lastCh = max(firstCh + JOYSTICK_MAX_CHANNELS, JOYSTICK_MAX_CHANNELS);

  for (int channel=firstCh; channel<lastCh; channel++) {
    chdat.ch[channel] = PPM_CH_CENTER(channel) + limit((int16_t)-PPM_range, channelOutputs[channel], (int16_t)PPM_range) / 2;
    chdat.channelmask |= 1<<channel;
  }
  write((uint8_t *)&chdat, (int)sizeof(channeldata));
}

void ESPJoystick::dataReceived(const uint8_t *data, int len)
{
  
  // No incoming data as a joystick device
}
