/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SERIAL_H_
#define _SERIAL_H_

#include "targets/horus/board.h"

#ifdef __cplusplus

typedef enum {
    FLOW_NONE = USART_HardwareFlowControl_None,
    FLOW_RTS = USART_HardwareFlowControl_RTS,
    FLOW_CTS = USART_HardwareFlowControl_RTS,
    FLOW_RTSCTS = USART_HardwareFlowControl_RTS_CTS,
} SerialFlow;

typedef enum {
    PARITY_NONE = USART_Parity_No,
    PARITY_ODD = USART_Parity_Odd,
    PARITY_EVEN = USART_Parity_Even,
} SerialParity;

typedef enum {
    STOP_ONE=USART_StopBits_1,
    STOP_TWO=USART_StopBits_2,
    STOP_HALF=USART_StopBits_0_5,
    STOP_ONE_AND_HALF= USART_StopBits_1_5,    
} SerialStopBits;

typedef enum {
    DATA_8 = USART_WordLength_8b,
    DATA_9 = USART_WordLength_9b
} SerialDataBits;

class SerialPort {
public:
    virtual int open()=0;
    virtual int close()=0;
    virtual void flush()=0;    
    virtual void setDMAEnabled(bool d) {dma = d;}
    void setDataBits(SerialDataBits data) {this->databits = data;}
    void setBaudRate(uint32_t baud) {this->baud = baud;}
    void setFlowControl(SerialFlow flow) {this->flow = flow;}
    void setStopBits(SerialStopBits stopbits) {this->stopbits = stopbits;}
    void setStopParity(SerialParity parity) {this->parity = parity;}
    void setInvertedTX(bool inv) {invertedTX = inv;}
    void setInvertedRX(bool inv) {invertedRX = inv;}
    void setInverted(bool inv) {invertedRX = inv; invertedTX= inv;}
    void setPowerOn(bool pwr) {poweron = pwr;}
    virtual int putc(int ch)=0;
    virtual int getc()=0;
    
protected:
    uint32_t baud = 115200;
    SerialDataBits databits = DATA_8;
    SerialParity parity = PARITY_NONE;
    SerialStopBits stopbits = STOP_ONE;
    SerialFlow flow = FLOW_NONE;
    bool invertedTX=false;
    bool invertedRX=false;
    bool dma=false;
    bool poweron=false;
};

extern "C" {
#endif

void serialPutc(char c);
void serialPrintf(const char *format, ...);
void serialCrlf();

#ifdef __cplusplus
}
#endif

#define serialPrint(...) do { serialPrintf(__VA_ARGS__); serialCrlf(); } while(0)

#endif // _SERIAL_H_

