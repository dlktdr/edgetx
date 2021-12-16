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
//#define DEBUG_LABELS

#ifdef DEBUG_LABELS
#define TRACE_LABELS(...) TRACE(__VA_ARGS__)
#else
#define TRACE_LABELS(...)
#endif

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

    ModelCell   *curmodel;
    bool        modeldatavalid;
    uint8_t     level;
    char        current_attr[16]; // set after find_node()
};

static labelslist_iter __labelslist_iter_inst;

void* get_labelslist_iter()
{
  // Thoughts... record the time here.
  // If time > xxx while parsing models. pop up a window showing updating?

  __labelslist_iter_inst.modeldatavalid = false;
  __labelslist_iter_inst.curmodel = NULL;
  __labelslist_iter_inst.level = 0;

  TRACE_LABELS("YAML Label Reader Start %u", 0);

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

    TRACE_LABELS("YAML To Parent %u", mi->level);

    return true;
}

static bool to_child(void* ctx)
{
    labelslist_iter* mi = (labelslist_iter*)ctx;

    mi->level++;

    TRACE_LABELS("YAML To Child");
    TRACE_LABELS("YAML Level %u", mi->level);
    return true;
}

static bool to_next_elmt(void* ctx)
{
    labelslist_iter* mi = (labelslist_iter*)ctx;
    if (mi->level == labelslist_iter::Root) {
        return false;
    }

    TRACE_LABELS("YAML To Next Element");
    TRACE_LABELS("YAML Current Level %u", mi->level);
    return true;
}

static bool find_node(void* ctx, char* buf, uint8_t len)
{
    labelslist_iter* mi = (labelslist_iter*)ctx;

    memcpy(mi->current_attr, buf, len);
    mi->current_attr[len] = '\0';

    TRACE_LABELS("YAML On Node %s", mi->current_attr);
    TRACE_LABELS("YAML Current Level %u", mi->level);

    // If in the labels node, force to labelsroot enum
    if(mi->level == labelslist_iter::ModelsRoot && strcasecmp(mi->current_attr,"labels") == 0) {
      TRACE_LABELS("Forced root");
      mi->level = labelslist_iter::LabelsRoot;
      TRACE_LABELS("YAML New Level %u", mi->level);
    }

    if(mi->level == labelslist_iter::ModelName)  { // Model List
      bool found=false;
      for (auto const& i : modelslist) {
          TRACE_LABELS("Comparing %s <-> %s", mi->current_attr, i->modelFilename);
          if(!strcmp(i->modelFilename, mi->current_attr)) {
            TRACE_LABELS("This file actually exists. Saving the pointer");
            mi->modeldatavalid = false;
            mi->curmodel = i;
            found=true;
            break;
          }
        }
      if(!found) {
        mi->curmodel = NULL;
        TRACE_LABELS("File does not exist in /MODELS");
      }
    }
    return true;
}

static void set_attr(void* ctx, char* buf, uint8_t len)
{
  char value[40];
  memcpy(value, buf, len);
  value[len] = '\0';

  labelslist_iter* mi = (labelslist_iter*)ctx;
  TRACE_LABELS("YAML Attr Level %u, %s = %s", mi->level, mi->current_attr, value);

  // Model Section
  if(mi->level == labelslist_iter::ModelData) {

    // File Hash
    if(!strcasecmp(mi->current_attr, "hash")) {
      if(mi->curmodel != NULL) {
          if(!strcmp(mi->curmodel->modelFinfoHash, value)) {
            TRACE_LABELS("FILE HASH MATCHES, No need to scan this model, just load the settings");
            mi->modeldatavalid = true;
            mi->curmodel->staleData = false; // TODO is this redundant?
          } else {
            TRACE_LABELS("FILE HASH Does not Match, Open model and rebuild modelcell");
            mi->modeldatavalid = false;
            mi->curmodel->staleData = true;
          }
      }

    // Model Name
    } else if(mi->modeldatavalid && !strcasecmp(mi->current_attr, "name")) {
      if(mi->curmodel != NULL) {
        mi->curmodel->setModelName(value);
        TRACE_LABELS("Set the models name");
      }

    // Model ID0
    } else if(mi->modeldatavalid && !strcasecmp(mi->current_attr, "id0")) {
      if(mi->curmodel != NULL) {
        mi->curmodel->setModelId(0, atoi(value));
        TRACE_LABELS("Set the models id0 to %s", value);
      }

#if NUM_MODULES == 2
    // Model ID0
    } else if(mi->modeldatavalid && !strcasecmp(mi->current_attr, "id1")) {
      if(mi->curmodel != NULL) {
        mi->curmodel->setModelId(1, atoi(value));
        TRACE_LABELS("Set the models id1 to %s", value);
      }
#endif


    // Model Bitmap
#if LEN_BITMAP_NAME > 0
    } else if(mi->modeldatavalid && !strcasecmp(mi->current_attr, "bitmap")) {
      if(mi->curmodel != NULL) {
        // TODO Check if it exists ?
        strcpy(mi->curmodel->modelBitmap, value);
        TRACE_LABELS("Set the models bitmap");
      }
#endif

    // Model Labels
    } else if(mi->modeldatavalid && !strcasecmp(mi->current_attr, "labels")) {
      if(mi->curmodel != NULL) {
        char *cma;
        cma = strtok(value, ",");
        while(cma != NULL) {
          modelsLabels.addLabelToModel(cma,mi->curmodel);
          TRACE_LABELS(" Adding the label - %s", cma);
          cma = strtok(NULL, ",");
        }
      }
    }

  // Label Section
  } else if(mi->level == labelslist_iter::LabelData) {
    if(!strcasecmp(mi->current_attr, "icon")) {
      TRACE_LABELS("Label Icon - %s", value);
      // TODO - Check icon exists, or ignore it.

    }
  }
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
