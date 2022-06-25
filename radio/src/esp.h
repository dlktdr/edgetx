#pragma once

#include <list>
#include "stdint.h"

typedef enum {
  ESP_UNKNOWN=0,
  ESP_A2DP_SOURCE,
  ESP_TRAINER_OUT,
  ESP_TRAINER_IN,
  ESP_HEADTRACKER,
  ESP_MODECOUNT
} espmode;

class ESPFunction
{
  public:
    ESPFunction(ESPModule &module);
  protected:
    void
};

class ESPModule
{
public:
  ESPModule() {for(int i=0; i < ESP_MODECOUNT; i++) rxcb[i] = nullptr;}
  void setMode(espmode mode);
  espmode mode();

  void setRxCb(espmode mode, void(*cb)(uint8_t *data, size_t len))
  {
    if(cb != nullptr) {
      rxcb[mode] = cb;
    }
  };

  int write(espmode mode, uint8_t *data, size_t len) {
    // Write to ESP with mode info

  }


private:
  espmode curmode = ESP_UNKNOWN;
  void(*rxcb[ESP_MODECOUNT])(uint8_t *data, size_t len);
};