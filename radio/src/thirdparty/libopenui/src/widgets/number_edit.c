/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   libopenui - https://github.com/opentx/libopenui
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

#include "field_edit.h"

#define MY_CLASS &field_edit_class

const lv_obj_class_t field_edit_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = lv_textarea_class.constructor_cb,
    .destructor_cb = lv_textarea_class.destructor_cb,
    .event_cb = lv_textarea_class.event_cb,
    .width_def = LV_DPI_DEF / 2,
    .height_def = LV_SIZE_CONTENT,
    .editable = LV_OBJ_CLASS_EDITABLE_TRUE,
    .group_def = LV_OBJ_CLASS_GROUP_DEF_TRUE,
    .instance_size = sizeof(lv_textarea_t)
};

lv_obj_t* field_edit_create(lv_obj_t* parent)
{
  lv_obj_t* obj = lv_obj_class_create_obj(MY_CLASS, parent);
  lv_obj_class_init_obj(obj);
  return obj;
}
