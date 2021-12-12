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

#include "yaml_labelslist.h"
#include "yaml_parser.h"

#include "debug.h"

#include "storage/modelslist.h"

#include <cstring>

using std::list;

struct labelslist_iter
{
    enum ItemData {
        Root=0,
        ModelsRoot=1,
        ModelName=2,
        ModelData=3,
        LabelsRoot=4,
        LabelName=5,
        LabelData=6,
    };

    //ModelsList* root;
    //const char* currentModel;
    //unsigned    currentModel_len;
    ModelCell   *curmodel;
    bool        modeldatavalid;
    uint8_t     level;
    char        current_attr[16]; // set after find_node()
};

static labelslist_iter __labelslist_iter_inst;

void* get_labelslist_iter()
{
    __labelslist_iter_inst.modeldatavalid = false;
    __labelslist_iter_inst.curmodel = NULL;
    __labelslist_iter_inst.level = 0;

    TRACE("YAML Label Reader Start %u", 0);

    return &__labelslist_iter_inst;
}

static bool to_parent(void* ctx)
{
    labelslist_iter* mi = (labelslist_iter*)ctx;

    if (mi->level == labelslist_iter::Root)
        return false;

    if(mi->level == labelslist_iter::LabelsRoot)
      mi->level = labelslist_iter::Root;
    else
      mi->level--;

    TRACE("YAML To Parent %u", mi->level);

    return true;
}

static bool to_child(void* ctx)
{
    labelslist_iter* mi = (labelslist_iter*)ctx;

/*    if (mi->level == labelslist_iter::LabelsRoot) {
      mi->level == labelslist_iter::Root
        return false;
    }*/

    mi->level++;

    TRACE("YAML To Child");
    TRACE("YAML Level %u", mi->level);
    return true;
}

static bool to_next_elmt(void* ctx)
{
    labelslist_iter* mi = (labelslist_iter*)ctx;
    if (mi->level == labelslist_iter::Root) {
        return false;
    }

    TRACE("YAML To Next Element");
    TRACE("YAML Current Level %u", mi->level);
    return true;
}

static bool find_node(void* ctx, char* buf, uint8_t len)
{
    labelslist_iter* mi = (labelslist_iter*)ctx;

    memcpy(mi->current_attr, buf, len);
    mi->current_attr[len] = '\0';

    if(mi->level == 1 && strcasecmp(mi->current_attr,"labels") == 0) {
      TRACE("Forced root");
      mi->level = labelslist_iter::LabelsRoot;
    }

    if(mi->level == 2)  { // Model List
      for (auto const& i : modelslist) {
          TRACE("Comparing %s <-> %s", i->modelFilename, mi->current_attr);
          if(!strcmp(i->modelFilename, mi->current_attr)) {
            TRACE("This file actually exists. Saving it");
            mi->modeldatavalid = false;
            mi->curmodel = i;
            break;
          }
        }
    }

    TRACE("YAML Find Node %s", mi->current_attr);
    TRACE("YAML Current Level %u", mi->level);
    return true;
}

static void set_attr(void* ctx, char* buf, uint8_t len)
{
  char value[40];
  memcpy(value, buf, len);
  value[len] = '\0';

  labelslist_iter* mi = (labelslist_iter*)ctx;
  TRACE("YAML Setting Attr Level %u, %s = %s", mi->level, mi->current_attr, value);

  if(mi->level == 3) {
    if(!strcasecmp(mi->current_attr, "hash")) {
      if(mi->curmodel != NULL) {
          if(!strcmp(mi->curmodel->modelFinfoHash, value)) {
            TRACE("FILE HASH MATCHES, No need to scan this model, just load the settings");

            mi->modeldatavalid = true;
          } else {
            TRACE("FILE HASH Does not Match, Open model and rebuild modelcell");
          }
      }
    } else if(mi->modeldatavalid && !strcasecmp(mi->current_attr, "name")) {
      if(mi->curmodel != NULL)
        mi->curmodel->setModelName(value);
        TRACE("Set the models name");
    } else if(mi->modeldatavalid && !strcasecmp(mi->current_attr, "labels")) {
      if(mi->curmodel != NULL) {
        char *cma;
        cma = strtok(value, ",");
        while(cma != NULL) {
          modelslabels.insert(std::pair<std::string, ModelCell *>(cma,mi->curmodel));
          TRACE(" Added A Label to the Model - %s", cma);
          cma = strtok(NULL, ",");
        }
      }
    }
  }

/*
  auto ml = mi->root;
  list<ModelsCategory*>& cats = ml->getCategories();

  switch (mi->level) {
    case modelslist_iter::Model:
      if (!strcmp(mi->current_attr, "filename")) {
        if (!cats.empty()) {
          ModelCell* model = new ModelCell(buf, len);
          auto cat = cats.back();
          cat->push_back(model);
          ml->incModelsCount();

          if (!strncmp(model->modelFilename, mi->currentModel,
                       mi->currentModel_len)) {
            ml->setCurrentCategory(cat);
            ml->setCurrentModel(model);
          }
        }
      } else if (!strcmp(mi->current_attr, "name")) {
        if (!cats.empty()) {
          auto cat = cats.back();
          if (!cat->empty()) {
            auto model = cat->back();
            model->setModelName(buf, len);
          }
        }
      }
      break;
  }*/
}

static const YamlParserCalls labelslistCalls = {
    to_parent,
    to_child,
    to_next_elmt,
    find_node,
    set_attr
};

const YamlParserCalls* get_labelslist_parser_calls()
{
    return &labelslistCalls;
}
