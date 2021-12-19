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
#include "storage/modelslist.h"
#include "libopenui.h"

class ModelSelectMenu: public TabsGroup {
  public:
    ModelSelectMenu();
    void build(int index=-1);
};

class ModelsPageBody : public FormWindow
{
  public:
    ModelsPageBody(Window *parent, const rect_t &rect);

    void update(int selected = -1);
    void setFocus(uint8_t flag = SET_FOCUS_DEFAULT, Window *from = nullptr) override;

    #if defined(HARDWARE_KEYS)
    void onEvent(event_t event) override
    {
      if (event == EVT_KEY_BREAK(KEY_ENTER)) {
        addFirstModel();
      } else {
        FormWindow::onEvent(event);
      }
    }
    #endif


  void addFirstModel() {
    Menu *menu = new Menu(this);
    menu->addLine(STR_CREATE_MODEL, getCreateModelAction());
  }


    #if defined(HARDWARE_TOUCH)
      bool onTouchEnd(coord_t x, coord_t y) override
      {
        /*if(category->size() == 0)
          addFirstModel();
        else*/
          FormWindow::onTouchEnd(x,y);
        //return true;
      }
    #endif

  protected:

    std::function<void(void)> getCreateModelAction()
    {
      return [=]() {
        /*storageCheck(true);
        auto model = modelslist.addModel(category, createModel(), false);
        model->setModelName(g_model.header.name);
        modelslist.setCurrentModel(model);
        modelslist.save();
        update(category->size() - 1);*/
      };
    }
};


class ModelLabelsWindow : public Window {
  public:
    ModelLabelsWindow();
  protected:
#if defined(HARDWARE_KEYS)
    void onEvent(event_t event) override;
#endif
#if defined(HARDWARE_TOUCH)
//    bool onTouchSlide(coord_t x, coord_t y, coord_t startX, coord_t startY, coord_t slideX, coord_t slideY) override;
    bool onTouchEnd(coord_t x, coord_t y) override;
#endif

    void paint(BitmapBuffer * dc) override;
};


#endif // _MODEL_SELECT_H_
