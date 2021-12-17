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

#ifndef _MODEL_SELECT_H_
#define _MODEL_SELECT_H_

#include "tabsgroup.h"

class ModelSeletWidget: public Window {
  public:
    ModelSeletWidget();
    void build();
};

class ModelSelectMenu: public Window {
  public:
    ModelSelectMenu(Window * parent, const rect_t & rect, WindowFlags windowFlags = 0, LcdFlags textFlags = 0);
    void build(int index=-1);
    void paint(BitmapBuffer * dc) override;
#if defined(HARDWARE_KEYS)
    void onEvent(event_t event) override;
#endif
//TODO ADD IF
    bool onTouchEnd(coord_t x, coord_t y) override;
  protected:
    int sidebarWidth=50;
};

#endif // _MODEL_SELECT_H_
