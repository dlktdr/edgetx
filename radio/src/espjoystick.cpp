#include "esp.h"

#include "fifo.h"

Fifo<uint8_t, 50>espRxFifo;

ESPJoystick::dataReceived(uint8_t *data, int len)
{
  // Ignore incoming telemetry, for now.
}

void ESPJoystick::wakeup() override
{

}

