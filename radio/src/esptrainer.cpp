
#include "opentx.h"
#include <string.h>
#include "definitions.h"
#include "esp.h"
#include "fifo.h"

#define BLUETOOTH_TRAINER_CHANNELS

typedef struct  {
  int16_t ch[32];
  uint32_t channelmask=0; // Valid Channels
} channeldata;

void ESPTrainer::wakeup()
{
  sendTrainer(); // TEST
}

void ESPTrainer::dataReceived(uint8_t *data, int len)
{
  if(len != sizeof(channeldata))
    return;

  channeldata *chnldata = (channeldata*)data;
  // Trainer Data Received - 32 Channels
  for(int i=0; i < MAX_OUTPUT_CHANNELS; i++) {
    if(chnldata->channelmask & 1<<i) // Channel Valid?
      ppmInput[i] = chnldata->ch[i] - 1500; // TODO - Is this correct offset?
  }

  // TODO REPLACE ME WITH setTrainerData(
}

void ESPTrainer::sendTrainer()
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
