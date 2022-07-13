
#include "opentx.h"
#include <string.h>
#include "definitions.h"
#include "esp.h"
#include "fifo.h"

void ESPTrainer::wakeup()
{
  //sendTrainer(); // TEST
}

void ESPTrainer::dataReceived(const uint8_t *data, int len)
{
  if(len != sizeof(channeldata))
    return;

  channeldata *chnldata = (channeldata*)data;
  uint16_t trch[MAX_TRAINER_CHANNELS];

  // Trainer Data Received - 32 Channels  
  for(int i=0; i < MAX_TRAINER_CHANNELS; i++) {
    if(chnldata->channelmask & 1<<i) {
      trch[i] = chnldata->ch[i];
    } else {
      trch[i] = PPM_CH_CENTER(i);
    }
  }

  setTrainerData(trch, MAX_TRAINER_CHANNELS);
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
