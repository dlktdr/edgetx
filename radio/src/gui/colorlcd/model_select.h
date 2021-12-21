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
#include <algorithm>
#include "listbox.h"


class ModelSelectMenu: public TabsGroup {
  public:
    ModelSelectMenu();
    void build(int index=-1);
};

class ModelsPageBody : public FormWindow
{
  public:
    ModelsPageBody(Window *parent, const rect_t &rect);

    void setLabel(std::string &lbl) {selectedLabel = lbl; update();}
    void update(int selected = -1);

    #if defined(HARDWARE_KEYS)
    void onEvent(event_t event) override;
    #endif

  void deleteLater(bool detach = true, bool trash = true) override
  {
    innerWindow.deleteLater(true, false);

    Window::deleteLater(detach, trash);
  }

  void paint(BitmapBuffer *dc) override;

  void addFirstModel() {
    Menu *menu = new Menu(this);
    menu->addLine(STR_CREATE_MODEL, getCreateModelAction());
  }

  protected:
    FormWindow innerWindow;
    void initPressHandler(Button *button, ModelCell *model, int index);
    std::string selectedLabel;
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

class ModelLabelsWindow : public Page {
  public:
    ModelLabelsWindow();

#if defined (HARDWARE_KEYS)
  void onEvent(event_t event) override;
#endif

  protected:
    ListBox *lblselector;
    ModelsPageBody *mdlselector;
    TextButton *newButton;
    std::string currentLabel;

    void buildHead(PageHeader *window);
    void buildBody(FormWindow *window);
};


#endif // _MODEL_SELECT_H_
