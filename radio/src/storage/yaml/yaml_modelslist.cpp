/*
 * Copyright (C) OpenTX
 *
 * Based on code named
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

#include "yaml_modelslist.h"
#include "yaml_parser.h"

#include "storage/modelslist.h"

using std::list;

struct modelslist_iter
{
    enum Level {
        Root=0,
        Category=1,
        Model=2
    };

    ModelsList* root;
    uint8_t     level;
    char        current_attr[16]; // set after find_node()
};

static modelslist_iter __modelslist_iter_inst;

void* get_modelslist_iter()
{
    __modelslist_iter_inst.root = &modelslist;
    __modelslist_iter_inst.level = 0;
    return &__modelslist_iter_inst;
}

static bool to_parent(void* ctx)
{
    modelslist_iter* mi = (modelslist_iter*)ctx;
    if (mi->level == modelslist_iter::Root) {
        return false;
    }

    mi->level--;
    return true;
}

static bool to_child(void* ctx)
{
    modelslist_iter* mi = (modelslist_iter*)ctx;
    if (mi->level == modelslist_iter::Model) {
        return false;
    }

    mi->level++;
    return true;
}

static bool to_next_elmt(void* ctx)
{
    modelslist_iter* mi = (modelslist_iter*)ctx;
    if (mi->level == modelslist_iter::Root) {
        return false;
    }
    return true;
}

static bool find_node(void* ctx, char* buf, uint8_t len)
{
    modelslist_iter* mi = (modelslist_iter*)ctx;

    if (len > sizeof(modelslist_iter::current_attr)-1)
        len = sizeof(modelslist_iter::current_attr)-1;

    memcpy(mi->current_attr, buf, len);
    mi->current_attr[len] = '\0';

    list<ModelsCategory *>& cats = mi->root->getCategories();
    if (mi->level == modelslist_iter::Category) {
        ModelsCategory* cat = new ModelsCategory(buf,len);
        cats.push_back(cat);
    }

    return true;
}

static void set_attr(void* ctx, char* buf, uint8_t len)
{
    modelslist_iter* mi = (modelslist_iter*)ctx;
    list<ModelsCategory *>& cats = mi->root->getCategories();
    static bool fexists=false;
    switch(mi->level) {

    case modelslist_iter::Model:
        if (!strcmp(mi->current_attr,"filename")) {
            if (!cats.empty()) {
                // Check if the models .yml actually exists before adding it
                FILINFO fno;
                char modelpath[LEN_MODEL_FILENAME + sizeof(MODELS_PATH) + 10];
                buf[len] = '\0';
                snprintf(modelpath, sizeof(modelpath), "%s/%s", MODELS_PATH, buf);
                if (f_stat(modelpath, &fno) == FR_OK) {
                    ModelCell* model = new ModelCell(buf, len);
                    cats.back()->push_back(model);
                    mi->root->incModelsCount();
                    fexists = true;
                } else {
                    TRACE("File %s Not Found", modelpath);
                    fexists = false;
                }
            }
        }
        else if (!strcmp(mi->current_attr,"name")) {
            if (!cats.empty() && fexists) {
                ModelsCategory* cat = cats.back();
                if (!cat->empty()) {
                    ModelCell* model = cat->back();
                    model->setModelName(buf,len);
                }
            }
        }

        else if (!strcmp(mi->current_attr,"icon")) {
            if (!cats.empty() && fexists) {
                ModelsCategory* cat = cats.back();
                if (!cat->empty()) {
                    buf[len] = '\0';
                    strncpy(cat->icon, buf, sizeof(ModelsCategory::icon));
                }
            }
        }

        break;
    }
}

static const YamlParserCalls modelslistCalls = {
    to_parent,
    to_child,
    to_next_elmt,
    find_node,
    set_attr
};

const YamlParserCalls* get_modelslist_parser_calls()
{
    return &modelslistCalls;
}
