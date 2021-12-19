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

#include <algorithm>
#include "model_select.h"
#include "opentx.h"

#define LABELS_WIDTH 90
#define LABELS_LEFT 5
#define LABELS_TOP (60)

#if LCD_W > LCD_H
constexpr int MODEL_CELLS_PER_LINE = 3;
#else
constexpr int MODEL_CELLS_PER_LINE = 2;
#endif

constexpr coord_t MODEL_CELL_PADDING = 6;

constexpr coord_t MODEL_SELECT_CELL_WIDTH =
    (LCD_W - LABELS_WIDTH - LABELS_LEFT - (MODEL_CELLS_PER_LINE + 1) * MODEL_CELL_PADDING) /
    MODEL_CELLS_PER_LINE;

constexpr coord_t MODEL_SELECT_CELL_HEIGHT = 92;

constexpr coord_t MODEL_IMAGE_WIDTH  = MODEL_SELECT_CELL_WIDTH;
constexpr coord_t MODEL_IMAGE_HEIGHT = 72;


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
      buffer->drawText(width() / 2, 2, "(Invalid Model)",
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

ModelsPageBody::ModelsPageBody(Window *parent, const rect_t &rect) :
    FormWindow(parent, rect, FORM_FORWARD_FOCUS)
{
  update();
}

void ModelsPageBody::update(int selected)
  {
    clear();

    int index = 0;
    coord_t y = MODEL_CELL_PADDING;
    coord_t x = MODEL_CELL_PADDING;

    // TODO - Filter list by selected labels

    ModelButton* selectButton = nullptr;
    for (auto &model : modelslist) {
      auto button = new ModelButton(
          this, {x, y, MODEL_SELECT_CELL_WIDTH, MODEL_SELECT_CELL_HEIGHT},
          model);
      button->setPressHandler([=]() -> uint8_t {
        if (button->hasFocus()) {
          Menu *menu = new Menu(parent);
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

              /*modelslist.setCurrentModel(model);
              modelslist.setCurrentCategory(category);*/
              this->onEvent(EVT_KEY_FIRST(KEY_EXIT));
              checkAll();
            });
          }
          menu->addLine(STR_CREATE_MODEL, getCreateModelAction());
          menu->addLine(STR_DUPLICATE_MODEL, [=]() {
            char duplicatedFilename[LEN_MODEL_FILENAME + 1];
            memcpy(duplicatedFilename, model->modelFilename,
                   sizeof(duplicatedFilename));
            if (findNextFileIndex(duplicatedFilename, LEN_MODEL_FILENAME,
                                  MODELS_PATH)) {
              sdCopyFile(model->modelFilename, MODELS_PATH, duplicatedFilename,
                         MODELS_PATH);
              //modelslist.addModel(category, duplicatedFilename);
              update(index);
            } else {
              POPUP_WARNING("Invalid File");
            }
          });

          if (model != modelslist.getCurrentModel()) {
            // Move
            if(modelslist.getCategories().size() > 1) {
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
                }*/
              });
            }
            menu->addLine(STR_DELETE_MODEL, [=]() {
              new ConfirmDialog(
                  parent, STR_DELETE_MODEL,
                  std::string(model->modelName, sizeof(model->modelName)).c_str(), [=]() {});
                    //modelslist.removeModel(category, model);
                    //update(index < (int)category->size() - 1 ? index : index - 1);
            });
          }
        } else {
          button->setFocus(SET_FOCUS_DEFAULT);
        }
        return 1;
      });

      if (selected == index) {
        selectButton = button;
      }

      index++;

      if (index % MODEL_CELLS_PER_LINE == 0) {
        x = MODEL_CELL_PADDING;
        y += MODEL_SELECT_CELL_HEIGHT + MODEL_CELL_PADDING;
      } else {
        x += MODEL_CELL_PADDING + MODEL_SELECT_CELL_WIDTH;
      }
    }

    if (index % MODEL_CELLS_PER_LINE != 0) {
      y += MODEL_SELECT_CELL_HEIGHT + MODEL_CELL_PADDING;
    }
    setInnerHeight(y);

    /*if (category->empty()) {
      setFocus();
    } else if (selectButton) {
      selectButton->setFocus();
    }*/
  }

void ModelsPageBody::setFocus(uint8_t flag, Window *from)
{
  //if (category->empty()) {
    // use Window::setFocus() to avoid forwarding focus to nowhere
    // this crashes currently in libopenui
    //Window::setFocus(flag, from);
  //} else {
    FormWindow::setFocus(flag, from);
  //}
}

class ModelLabelsChecker : public FormGroup {
  public:
    ModelLabelsChecker(FormWindow * parent, const rect_t &rect) :
      FormGroup(parent, rect, FORWARD_SCROLL | FORM_FORWARD_FOCUS)
    {
      update();
    }

  protected:
    void update()
    {
      FormGridLayout grid;
      clear();
    }
};

class CategoryGroup: public FormGroup
{
  public:
    CategoryGroup(Window * parent, const rect_t & rect, ModelsCategory *category) :
      FormGroup(parent, rect),
      category(category)
    {
    }

    void paint(BitmapBuffer * dc) override
    {
      dc->drawSolidFilledRect(0, 0, width(), height(), COLOR_THEME_PRIMARY2);
      FormGroup::paint(dc);
    }

  protected:
    ModelsCategory *category;
};


//---- LABELS IMPLEMENTATION

class ModelLabelSelector : public FormWindow
{
  public:
    ModelLabelSelector(Window * parent, const rect_t & rect, WindowFlags windowFlags = 0) :
      FormWindow(parent, rect, windowFlags)
      {
        build();
        memclear(selectedlabels, sizeof(selectedlabels));
      }
  protected:
  void build() {
    // --- TODO... REWORK Layout for NV14?

    // Add Label Check Boxes
    int i=0;
    for(auto const &label : modelsLabels.getLabels()) {
      std::string capitalize = label;
      capitalize[0] = toupper(capitalize[0]);
      int height = i*(PAGE_LINE_HEIGHT+PAGE_LINE_SPACING);
#if 0
      new StaticText(this, {0,height,width()*3/4, PAGE_LINE_HEIGHT}, capitalize);
      new CheckBox(this, {width()*3/4+4,height,width()/4,PAGE_LINE_HEIGHT}, [=]() -> uint8_t{
        return selectedlabels[i];
      },[=](uint8_t newval) {
        TRACE("Label %s = %d", label.c_str(), (int)newval);
        selectedlabels[i] = newval;
      });
#else
      auto tb = new TextButton(this, {0,height,width()-LABELS_LEFT, PAGE_LINE_HEIGHT},capitalize, [=]() -> uint8_t {
        return 0;
      });
      tb->setBgColorHandler([=](){return COLOR_THEME_SECONDARY1;});
#endif

      if(++i == sizeof(selectedlabels)) // TODO define
        break;

    }
    this->setInnerHeight(i*(PAGE_LINE_HEIGHT+PAGE_LINE_SPACING));
  }
  void paint(BitmapBuffer *dc) override
  {
    FormWindow::paint(dc);
    /*if(hasFocus())
      dc->drawRect(0,0,width(),height(),4,255,COLOR_THEME_PRIMARY1);
    else
      dc->drawRect(0,0,width(),height(),1,255,COLOR_THEME_PRIMARY1);*/
  }
  uint8_t selectedlabels[50];
};

ModelLabelsWindow::ModelLabelsWindow() :
  Window(MainWindow::instance(), MainWindow::instance()->getRect(), NO_SCROLLBAR)
{
  setPageWidth(getParent()->width());
  focusWindow = this;

  setFocusHandler([&](bool focus) {
      TRACE("[LABELS] Focus %s",
            focus ? "gained" : "lost");
    });

  // Labels Selector - Left
  new ModelLabelSelector(this, {LABELS_LEFT,LABELS_TOP,LABELS_WIDTH, LCD_H-LABELS_TOP});

  // Models List and Filters - Right
  new ModelsPageBody(this, {LABELS_WIDTH + LABELS_LEFT,0, LCD_W-LABELS_WIDTH, LCD_H});

  /*auto sortby = new Choice(this, {300,5,100,35} , 0 , 2, [=]{});
  sortby->addValue("Name");
  sortby->addValue("ID");
  sortby->addValue("Recent");*/
}

void ModelLabelsWindow::paint(BitmapBuffer * dc)
{
  // Top Bar
  /*dc->drawSolidFilledRect(0, 0, width(), MENU_HEADER_HEIGHT , COLOR_THEME_SECONDARY1);*/
  // Main Screen
  dc->drawSolidFilledRect(LABELS_WIDTH, 0, width()-LABELS_WIDTH, height(), COLOR_THEME_SECONDARY3);
  // Labels
  dc->drawSolidFilledRect(0, 0, LABELS_WIDTH, height(), COLOR_THEME_SECONDARY1);
  // Models Select button
  //OpenTxTheme::instance()->drawTopLeftBitmap(dc);
  dc->drawBitmap(20, 5, OpenTxTheme::instance()->getIcon(ICON_MODEL_SELECT,STATE_DEFAULT),0,0,0,0,1.6);
}

void ModelLabelsWindow::onEvent(event_t event)
{
  Window::onEvent(event);
}

bool ModelLabelsWindow::onTouchEnd(coord_t x, coord_t y)
{
  if(x < (int)MENU_HEADER_BUTTONS_LEFT && y < (int)MENU_HEADER_HEIGHT) {
    deleteLater(true);
    return true;
  }
  else
    return Window::onTouchEnd(x,y);
}