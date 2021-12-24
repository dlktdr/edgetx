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

#include <vector>
#include <iostream>
#include <algorithm>

#include "model_select.h"
#include "opentx.h"
#include "listbox.h"

#define LABELS_WIDTH 90
#define LABELS_LEFT 5
#define LABELS_TOP (60)

#if LCD_W > LCD_H
constexpr int MODEL_CELLS_PER_LINE = 2;
constexpr int BUTTON_HEIGHT = 30;
constexpr int BUTTON_WIDTH  = 75;
constexpr LcdFlags textFont = FONT(STD);
#else
constexpr int MODEL_CELLS_PER_LINE = 2;
constexpr int BUTTON_HEIGHT = 30;
constexpr int BUTTON_WIDTH  = 65;
constexpr LcdFlags textFont = FONT(XS);
#endif

constexpr coord_t MODEL_CELL_PADDING = 6;

constexpr coord_t MODEL_SELECT_CELL_WIDTH =
    (LCD_W - LABELS_WIDTH - LABELS_LEFT - (MODEL_CELLS_PER_LINE + 1) * MODEL_CELL_PADDING) /
    MODEL_CELLS_PER_LINE;

constexpr coord_t MODEL_SELECT_CELL_HEIGHT = 92;

class ModelButton : public Button
{
 public:
  ModelButton(FormGroup *parent, const rect_t &rect, ModelCell *modelCell) :
      Button(parent, rect), modelCell(modelCell)
  {
    load();
  }

  ~ModelButton()
  {
    if (buffer) { delete buffer; }
  }

  void load()
  {
#if defined(SDCARD_RAW)
    uint8_t version;
#endif

    PACK(struct {
      ModelHeader header;
      TimerData timers[MAX_TIMERS];
    })
    partialModel;
    const char *error = nullptr;

    if (strncmp(modelCell->modelFilename, g_eeGeneral.currModelFilename,
                LEN_MODEL_FILENAME) == 0) {
      memcpy(&partialModel.header, &g_model.header, sizeof(partialModel));
#if defined(SDCARD_RAW)
      version = EEPROM_VER;
#endif
    } else {
#if defined(SDCARD_RAW)
      error =
          readModelBin(modelCell->modelFilename, (uint8_t *)&partialModel.header,
                       sizeof(partialModel), &version);
#else
      error = readModel(modelCell->modelFilename,
                        (uint8_t *)&partialModel.header, sizeof(partialModel));
#endif
    }

    if (!error) {
      if (modelCell->modelName[0] == '\0' &&
          partialModel.header.name[0] != '\0') {

#if defined(SDCARD_RAW)
        if (version == 219) {
          int len = (int)sizeof(partialModel.header.name);
          char* str = partialModel.header.name;
          for (int i=0; i < len; i++) {
            str[i] = zchar2char(str[i]);
          }
          // Trim string
          while(len > 0 && str[len-1]) {
            if (str[len - 1] != ' ' && str[len - 1] != '\0') break;
            str[--len] = '\0';
          }
        }
#endif
        modelCell->setModelName(partialModel.header.name);
      }
    }

    delete buffer;
    buffer = new BitmapBuffer(BMP_RGB565, width(), height());
    if (buffer == nullptr) {
      return;
    }
    buffer->clear(COLOR_THEME_PRIMARY2);

    if (error) {
      buffer->drawText(width() / 2, height() / 2, "(Invalid Model)",
                       COLOR_THEME_SECONDARY1 | CENTERED);
    } else {
      GET_FILENAME(filename, BITMAPS_PATH, partialModel.header.bitmap, "");
      const BitmapBuffer *bitmap = BitmapBuffer::loadBitmap(filename);
      if (bitmap) {
        buffer->drawScaledBitmap(bitmap, 0, 0, width(), height());
        delete bitmap;
      } else {
        buffer->drawText(width() / 2, 56, "(No Picture)",
                         FONT(XXS) | COLOR_THEME_SECONDARY1 | CENTERED);
      }
    }
  }

  void paint(BitmapBuffer *dc) override
  {
    FormField::paint(dc);

    if (buffer)
      dc->drawBitmap(0, 0, buffer);

    if (modelCell == modelslist.getCurrentModel()) {
      dc->drawSolidFilledRect(0, 0, width(), 20, COLOR_THEME_ACTIVE);
      dc->drawSizedText(width() / 2, 2, modelCell->modelName,
                        LEN_MODEL_NAME,
                        COLOR_THEME_SECONDARY1 | CENTERED);
    } else {
      LcdFlags textColor;
      // if (hasFocus()) {
      //   dc->drawFilledRect(0, 0, width(), 20, SOLID, COLOR_THEME_FOCUS, 5);
      //   textColor = COLOR_THEME_PRIMARY2;
      // }
      // else {
        dc->drawFilledRect(0, 0, width(), 20, SOLID, COLOR_THEME_PRIMARY2, 5);
        textColor = COLOR_THEME_SECONDARY1;
      // }

      dc->drawSizedText(width() / 2, 2, modelCell->modelName,
                        LEN_MODEL_NAME,
                        textColor | CENTERED);
    }

    if (!hasFocus()) {
      dc->drawSolidRect(0, 0, width(), height(), 1, COLOR_THEME_SECONDARY2);
    } else {
      dc->drawSolidRect(0, 0, width(), height(), 2, COLOR_THEME_FOCUS);
    }
  }

  const char *modelFilename() { return modelCell->modelFilename; }

 protected:
  ModelCell *modelCell;
  BitmapBuffer *buffer = nullptr;
};

//-----------------------------------------------------------------------------

ModelsPageBody::ModelsPageBody(Window *parent, const rect_t &rect) :
    FormWindow(parent, rect), 
  innerWindow(this, { 4, 4, rect.w - 8, rect.h - 8 })
{
  update();
  setFocusHandler([=](bool focus) {
    if (focus) {
      if (innerWindow.getChildren().size() > 0) {
        auto firstModel = *innerWindow.getChildren().begin();
        firstModel->setFocus();
      }
    }
  });

}

#if defined(HARDWARE_KEYS)
void ModelsPageBody::onEvent(event_t event)
{
  if (event == EVT_KEY_BREAK(KEY_ENTER)) {
    addFirstModel();
  } else {
    FormWindow::onEvent(event);
  }
}
#endif


void ModelsPageBody::paint(BitmapBuffer *dc)
{
  dc->drawSolidRect(0, getScrollPositionY(), rect.w, rect.h, 2, COLOR_THEME_FOCUS);
}

void ModelsPageBody::initPressHandler(Button *button, ModelCell *model, int index)
{
  button->setPressHandler([=]() -> uint8_t {
    Menu *menu = new Menu(this);
    if (model != modelslist.getCurrentModel()) {
      menu->addLine(STR_SELECT_MODEL, [=]() {
        // we store the latest changes if any
        storageFlushCurrentModel();
        storageCheck(true);
        memcpy(g_eeGeneral.currModelFilename, model->modelFilename,
                LEN_MODEL_FILENAME);
        loadModel(g_eeGeneral.currModelFilename, false);
        storageDirty(EE_GENERAL);
        storageCheck(true);
        modelslist.setCurrentModel(model);
        this->getParent()->getParent()->deleteLater();
        checkAll();
      });
    }
    menu->addLine(STR_DUPLICATE_MODEL, [=]() {
      char duplicatedFilename[LEN_MODEL_FILENAME + 1];
      memcpy(duplicatedFilename, model->modelFilename,
              sizeof(duplicatedFilename));
      if (findNextFileIndex(duplicatedFilename, LEN_MODEL_FILENAME,
                            MODELS_PATH)) {
        sdCopyFile(model->modelFilename, MODELS_PATH, duplicatedFilename,
                    MODELS_PATH);
        modelslist.addModel(duplicatedFilename);
        update(index);
      } else {
        POPUP_WARNING("Invalid File");
      }
    });

    if (model != modelslist.getCurrentModel()) {
      // Move -- ToDo.. Should it be kept an a modelindex added
      /*if(modelslist.size() > 1) {
        menu->addLine(STR_MOVE_MODEL, [=]() {
        auto moveToMenu = new Menu(parent);
        moveToMenu->setTitle(STR_MOVE_MODEL);
          /*for (auto newcategory: modelslist.getCategories()) {
            if(category != newcategory) {
              moveToMenu->addLine(std::string(newcategory->name, sizeof(newcategory->name)), [=]() {
                modelslist.moveModel(model, category, newcategory);
                update(index < (int)category->size() - 1 ? index : index - 1);
                modelslist.save();
              });
            }
          }
        });
      }*/
      menu->addLine(STR_DELETE_MODEL, [=]() {
        new ConfirmDialog(
            parent, STR_DELETE_MODEL,
            std::string(model->modelName, sizeof(model->modelName)).c_str(), [=]() {});
              modelslist.removeModel(model);
      });
    }
    return 1;
  });
}

void ModelsPageBody::update(int selected)
{
  innerWindow.clear();

  int index = 0;
  coord_t y = 0;
  coord_t x = 0;

  // TODO - Filter list by selected labels

  ModelButton* selectButton = nullptr;
  ModelsVector models = selectedLabel == "Unlabeled" ?
    modelsLabels.getUnlabeledModels() :
    modelsLabels.getModelsByLabel(selectedLabel);

  for (auto &model : models) {
    auto button = new ModelButton(
        &innerWindow, {x, y, MODEL_SELECT_CELL_WIDTH, MODEL_SELECT_CELL_HEIGHT},
        model);
    initPressHandler(button, model, index);
    if (selected == index) {
      selectButton = button;
    }

    index++;

    if (index % MODEL_CELLS_PER_LINE == 0) {
      x = 0;
      y += MODEL_SELECT_CELL_HEIGHT + MODEL_CELL_PADDING;
    } else {
      x += MODEL_CELL_PADDING + MODEL_SELECT_CELL_WIDTH;
    }
  }

  if (index % MODEL_CELLS_PER_LINE != 0) {
    y += MODEL_SELECT_CELL_HEIGHT + MODEL_CELL_PADDING;
  }
  innerWindow.setInnerHeight(y);

  if (selectButton != nullptr) {
    selectButton->setFocus();
  }

  /*if (category->empty()) {
    setFocus();
  } else if (selectButton) {
    selectButton->setFocus();
  }*/
}

//-----------------------------------------------------------------------------

ModelLabelsWindow::ModelLabelsWindow() :
  Page(ICON_MODEL)
{
  buildBody(&body);
  buildHead(&header);

  lblselector->setNextField(mdlselector);
  lblselector->setPreviousField(newButton);
  mdlselector->setNextField(newButton);
  newButton->setPreviousField(mdlselector);
  newButton->setNextField(lblselector);

  lblselector->setFocus();
}

bool isChildOfMdlSelector(Window *window)
{
  while (window != nullptr)
  {
    if (dynamic_cast<ModelsPageBody *>(window) != nullptr)
      return true;

    window = window->getParent();
  }

  return false;
}


#if defined(HARDWARE_KEYS)
void ModelLabelsWindow::onEvent(event_t event)
{
  if (event == EVT_KEY_BREAK(KEY_PGUP)) {
    onKeyPress();
    FormField *focus = dynamic_cast<FormField *>(getFocus());
    if (isChildOfMdlSelector(focus))
      lblselector->setFocus();
    else if (focus != nullptr && focus->getPreviousField()) {
      focus->getPreviousField()->setFocus(SET_FOCUS_BACKWARD, focus);
    }
  } else if (event == EVT_KEY_BREAK(KEY_PGDN)) {
    onKeyPress();
    FormField *focus = dynamic_cast<FormField *>(getFocus());
    if (isChildOfMdlSelector(focus))
      newButton->setFocus();
    else if (focus != nullptr && focus->getNextField()) {
      focus->getNextField()->setFocus(SET_FOCUS_FORWARD, focus);
    }
  } else {
    Page::onEvent(event);
  }
}
#endif

void ModelLabelsWindow::buildHead(PageHeader *window)
{
  LcdFlags flags = 0;
  if (LCD_W < LCD_H) {
    flags = FONT(XS);
  }

  // page title
  new StaticText(window,
                  {PAGE_TITLE_LEFT, PAGE_TITLE_TOP, LCD_W - PAGE_TITLE_LEFT,
                  PAGE_LINE_HEIGHT},
                  "Select Model", 0, COLOR_THEME_PRIMARY2 | flags);

  auto curModel = modelslist.getCurrentModel();
  auto modelName = curModel != nullptr ? curModel->modelName : "None";
  std::string titleName = "Current: " + std::string(modelName);
  new StaticText(window,
                  {PAGE_TITLE_LEFT, PAGE_TITLE_TOP + PAGE_LINE_HEIGHT,
                  LCD_W - PAGE_TITLE_LEFT, PAGE_LINE_HEIGHT},
                  titleName, 0, COLOR_THEME_PRIMARY2 | flags);

  // new model button
  rect_t r = {LCD_W - (BUTTON_WIDTH + 5), 6, BUTTON_WIDTH, BUTTON_HEIGHT };
  newButton = new TextButton(window, r, "New", [=] () {
    storageCheck(true);
    auto model = modelslist.addModel("unlabeled", false);
    model->setModelName(g_model.header.name);
    modelslist.setCurrentModel(model);
    modelslist.save();
    mdlselector->update(modelslist.size() - 1);
    return 0;
  }, BUTTON_BACKGROUND | OPAQUE, textFont);

}

void ModelLabelsWindow::buildBody(FormWindow *window)
{
  // Models List and Filters - Right
  mdlselector = new ModelsPageBody(window, {LABELS_WIDTH + LABELS_LEFT + 3, 5, window->width() - LABELS_WIDTH - 3 - LABELS_LEFT, window->height() - 10});

  auto labels = modelsLabels.getLabels();
  labels.emplace_back("Unlabeled");
  lblselector = new ListBox(window, {LABELS_LEFT, 5, LABELS_WIDTH, window->height() - 10 },
    labels,
    [=] () {
      return 0;
    }, [=](uint32_t value) {
      auto label = labels[value];
      mdlselector->setLabel(label); // Update the list
    });

  /*auto sortby = new Choice(this, {300,5,100,35} , 0 , 2, [=]{});
  sortby->addValue("Name");
  sortby->addValue("ID");
  sortby->addValue("Recent");*/
}