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
constexpr rect_t detailsDialogRect = {50, 50, 400, 100};
constexpr int labelWidth = 150;
#else
constexpr int MODEL_CELLS_PER_LINE = 2;
constexpr int BUTTON_HEIGHT = 30;
constexpr int BUTTON_WIDTH  = 65;
constexpr LcdFlags textFont = FONT(XS);
constexpr rect_t detailsDialogRect = {5, 50, LCD_W - 10, 340};
constexpr int labelWidth = 120;
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
    const char *error = nullptr;

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
      GET_FILENAME(filename, BITMAPS_PATH, modelCell->modelBitmap, "");
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
        ModelCell *newmodel = modelslist.addModel(duplicatedFilename);
        // Duplicated model should have same labels as orig. Add them
        for(const auto &lbl : modelsLabels.getLabelsByModel(model)) {
          modelsLabels.addLabelToModel(lbl, newmodel);
        }
        // Copy name & new filename
        strcpy(newmodel->modelName, model->modelName);
        strcpy(newmodel->modelFilename,duplicatedFilename);
        update();
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
            std::string(model->modelName, sizeof(model->modelName)).c_str(), [=]() {
              modelslist.removeModel(model);
              update();
            });
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
  ModelsVector models = selectedLabel == STR_UNLABELEDMODEL ?
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

class NewLabelDialog : public Dialog
{
  public:
    NewLabelDialog(Window *parent, std::function<void (std::string label)> saveHandler = nullptr) :
      Dialog(parent, "Enter New Label", detailsDialogRect),
      saveHandler(saveHandler),
      label("")
    {
      FormGridLayout grid(detailsDialogRect.w);
      grid.setLabelWidth(labelWidth);
      grid.spacer(8);

      new StaticText(&content->form, grid.getLabelSlot(), "Label", 0, COLOR_THEME_PRIMARY1);
      new TextEdit(&content->form, grid.getFieldSlot(), label, 20);
      grid.nextLine();

      rect_t r = {detailsDialogRect.w - (BUTTON_WIDTH + 5), grid.getWindowHeight() + 5, BUTTON_WIDTH, BUTTON_HEIGHT };
      new TextButton(&content->form, r, STR_SAVE, [=] () {
        if (saveHandler != nullptr)
          saveHandler(label);
        deleteLater();
        return 0;
      }, BUTTON_BACKGROUND | OPAQUE, textFont);
      r.x -= (BUTTON_WIDTH + 5);
      new TextButton(&content->form, r, STR_CANCEL, [=] () {
        deleteLater();
        return 0;
      }, BUTTON_BACKGROUND | OPAQUE, textFont);
    }

  protected:
    std::function<void (std::string label)> saveHandler;
    char label[20];

};

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
    auto model = modelslist.addModel(createModel(), false);
    modelslist.setCurrentModel(model);
    modelslist.updateCurrentModelCell();
    mdlselector->update(modelslist.size() - 1); // TODO.. foce to unlabeled category first?
    return 0;
  }, BUTTON_BACKGROUND | OPAQUE, textFont);

  r.x -= (BUTTON_WIDTH + 10);
  new TextButton(window, r, "New Label",
    [=] () {
      new NewLabelDialog(window,
        [=] (std::string label) {
          if(modelsLabels.addLabel(label) >= 0) {
            auto labels = getLabels();
            lblselector->setNames(labels);
            mdlselector->setLabel(label); // Update the list
          }
        });
      return 0;
    }, BUTTON_BACKGROUND | OPAQUE, textFont);

}

void ModelLabelsWindow::buildBody(FormWindow *window)
{
  // Models List and Filters - Right
  mdlselector = new ModelsPageBody(window, {LABELS_WIDTH + LABELS_LEFT + 3, 5, window->width() - LABELS_WIDTH - 3 - LABELS_LEFT, window->height() - 10});

  lblselector = new ListBox(window, {LABELS_LEFT, 5, LABELS_WIDTH, window->height() - 10 },
    getLabels(),
    [=] () {
      return 0;
    }, [=](uint32_t value) {
      auto label = getLabels()[value];
      mdlselector->setLabel(label); // Update the list
    });

  /*auto sortby = new Choice(this, {300,5,100,35} , 0 , 2, [=]{});
  sortby->addValue("Name");
  sortby->addValue("ID");
  sortby->addValue("Recent");*/
}