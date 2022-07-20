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

#include "hw_esp.h"

#include "opentx.h"

#define SET_DIRTY() storageDirty(EE_GENERAL)

static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(3),
                                     LV_GRID_TEMPLATE_LAST};

static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

class ESPDetailsDialog : public Dialog
{
  StaticText* l_addr;
  StaticText* r_addr;

  void setAddr(StaticText* label, const char* addr)
  {
    if (addr[0] == '\0') {
      label->setText("---");
    } else {
      label->setText(addr);
    }
  }

 public:
  ESPDetailsDialog(Window* parent) : Dialog(parent, "", rect_t{})
  {
    char title[40];
    snprintf(title, sizeof(title),"%s %s",STR_ESP_MODES[g_eeGeneral.espMode-1],"Settings");

    content->setTitle(title);
    setCloseWhenClickOutside(true);

    auto form = &content->form;
    form->setFlexLayout();
    FlexGridLayout grid(col_dsc, row_dsc);

switch (g_eeGeneral.espMode) {
    case ESP_TELEMETRY:
      break;
    case ESP_TRAINER: {
      auto line = form->newLine(&grid);
      new StaticText(line, rect_t{}, STR_NAME, 0, COLOR_THEME_PRIMARY1);
      new TextEdit(line, rect_t{}, espSettings.name,
                        sizeof(espsettings::name));

      line = form->newLine(&grid);
      new StaticText(line, rect_t{}, "Bluetooth Address", 0, COLOR_THEME_PRIMARY1);
      new StaticText(line, rect_t{}, espSettings.blemac,0, COLOR_THEME_PRIMARY1);

      line = form->newLine(&grid);
      new TextButton(line, rect_t{}, "Send Settings",[=]() -> uint8_t {
        esproot.sendSettings();
        return 0;
      });
      line = form->newLine(&grid);
      new TextButton(line, rect_t{}, "Get Settings",[=]() -> uint8_t {
        esproot.getSettings();
        return 0;
      });

      break;
    }
    case ESP_JOYSTICK:

      break;
    case ESP_AUDIO:

      break;
    case ESP_FTP: {
      // ESP Radio Name
      auto line = form->newLine(&grid);
      new StaticText(line, rect_t{}, STR_NAME, 0, COLOR_THEME_PRIMARY1);
      new RadioTextEdit(line, rect_t{}, g_eeGeneral.bluetoothName,
                        LEN_BLUETOOTH_NAME);

      break;
    }
    case ESP_IMU:
      break;
    default:
      break;
  }


    content->setWidth(LCD_W * 0.8);
    content->updateSize();
  }

  void checkEvents() override
  {
    // setAddr(l_addr, bluetooth.localAddr);
    // setAddr(r_addr, bluetooth.distantAddr);
  }
};

static void esp_mode_changed(lv_event_t* e)
{
  lv_obj_t* target = lv_event_get_target(e);
  auto obj = (lv_obj_t*)lv_event_get_user_data(e);
  auto choice = (Choice*)lv_obj_get_user_data(target);
  if (!obj || !choice) return;

  if (choice->getIntValue() != ESP_ROOT) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

// Connection Events. TODO make me safer
static void esp_event(const espevent* evt, void* user)
{
  auto connWind = (ESPConnectionWindow*)user;
  connWind->EspEventCB(evt);
}

ESPConnectionWindow::ESPConnectionWindow(Window* parent) :
    FormGroup(parent, rect_t{})
{
  esproot.setEventCB(esp_event, this);

  setFlexLayout();
  FlexGridLayout grid(col_dsc, row_dsc, 2);

  // ESP BLE - Client (Discover -> Scan for addresses, Connect and Disconnect)

  // ESP BLE - Server (Show if a client is connected, Show PHY, RSSI)

  // ESP BTEDR - Device (Pair -> Scan for names, Connect, Disconnect, Passcodes)

  // ESP WIFI AP - Scan for SSID's, Enter Passkey

  // ESP WIFI STA - Textbox SSID, Textbox Password, Connected Devices + IP List

  if (espaudio.isStarted()) {
  } else if (espmodule.isModeStarted(ESP_TRAINER)) {
  } else if (espmodule.isModeStarted(ESP_JOYSTICK)) {
  } else if (espmodule.isModeStarted(ESP_AUDIO)) {
  } else if (espmodule.isModeStarted(ESP_FTP)) {
  } else {
  }

  auto line = newLine(&grid);
  new StaticText(line, rect_t{}, STR_MODE, 0, COLOR_THEME_PRIMARY1);
}

ESPConnectionWindow::~ESPConnectionWindow()
{
  esproot.setEventCB(nullptr, nullptr);
}

void ESPConnectionWindow::EspEventCB(const espevent* evt)
{
  switch (evt->event) {
    case ESP_EVT_MESSAGE:
      new MessageDialog(this, "Message", (char*)evt->data);
      break;
    case ESP_EVT_DISCOVER_STARTED:
      break;
    case ESP_EVT_DISCOVER_COMPLETE:
      break;
    case ESP_EVT_DEVICE_FOUND:
      break;
    case ESP_EVT_CONNECTED:
      break;
    case ESP_EVT_DISCONNECTED:
      break;
    case ESP_EVT_PIN_REQUEST:
      break;
    case ESP_EVT_IP_OBTAINED:
      break;
  }
}

ESPConfigWindow::ESPConfigWindow(Window* parent) : FormGroup(parent, rect_t{})
{
  setFlexLayout();
  FlexGridLayout grid(col_dsc, row_dsc, 2);

  auto line = newLine(&grid);
  new StaticText(line, rect_t{}, STR_MODE, 0, COLOR_THEME_PRIMARY1);

  auto box = new FormGroup(line, rect_t{});
  box->setFlexLayout(LV_FLEX_FLOW_ROW_WRAP, lv_dpx(8));
  lv_obj_set_style_grid_cell_x_align(box->getLvObj(), LV_GRID_ALIGN_STRETCH, 0);
  lv_obj_set_style_flex_cross_place(box->getLvObj(), LV_FLEX_ALIGN_CENTER, 0);

  // ESP Mode
  auto mode = new Choice(box, rect_t{}, STR_ESP_MODES, 1, ESP_IMU,
                         GET_SET_DEFAULT(g_eeGeneral.espMode));
  auto btn =
      new TextButton(box, rect_t{}, LV_SYMBOL_SETTINGS, [=]() -> uint8_t {
        new ESPDetailsDialog(Layer::back());
        return 0;
      });
  btn->padAll(lv_dpx(4));
  btn->setWidth(LV_DPI_DEF / 2);

  lv_obj_add_event_cb(mode->getLvObj(), esp_mode_changed,
                      LV_EVENT_VALUE_CHANGED, btn->getLvObj());

}
