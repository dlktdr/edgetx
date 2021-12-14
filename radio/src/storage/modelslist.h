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

#ifndef _MODELSLIST_H_
#define _MODELSLIST_H_

#include <algorithm>
#include <stdint.h>
#include <list>
                                                                                                                                       #include <map>
#include <string>
#include <vector>

#include "sdcard.h"
#if !defined(SDCARD_YAML)
#include "sdcard_raw.h"
#endif

#include "dataconstants.h"

// #define MODELCELL_WIDTH                172
// #define MODELCELL_HEIGHT               59

// modelXXXXXXX.bin F,FF F,3F,FF\r\n
#define LEN_MODELS_IDX_LINE (LEN_MODEL_FILENAME + sizeof(" F,FF F,3F,FF\r\n")-1)

struct ModelData;
struct ModuleData;

struct SimpleModuleData
{
  uint8_t type;
  uint8_t rfProtocol;
};

typedef union {
  struct {
	  FSIZE_t	fsize;			/* File size */
	  WORD	fdate;			/* Modified date */
	  WORD	ftime;			/* Modified time */
  };
  uint8_t data[8];
} FInfoH;

#define FILE_HASH_LENGTH (sizeof(FInfoH::data) * 2)

class ModelCell
{
  public:
    char modelFilename[LEN_MODEL_FILENAME + 1];
    char modelName[LEN_MODEL_NAME + 1] = {};
    char modelFinfoHash[FILE_HASH_LENGTH + 1];
#if LEN_BITMAP_NAME > 0
    char modelBitmap[LEN_BITMAP_NAME];
#endif
    time_t lastOpened;
    bool staleData = true;

    bool             valid_rfData;
    uint8_t          modelId[NUM_MODULES];
    SimpleModuleData moduleData[NUM_MODULES];

    explicit ModelCell(const char * name);
    explicit ModelCell(const char * name, uint8_t len);

    void save(FIL * file);

    void setModelName(char * name);
    void setModelName(char* name, uint8_t len);
    void setRfData(ModelData * model);

    void setModelId(uint8_t moduleIdx, uint8_t id);
    void setRfModuleData(uint8_t moduleIdx, ModuleData* modData);

    bool fetchRfData();
};

typedef struct {
  std::string icon;
  // Anything else?
} SLabelDetail;

typedef std::vector<std::pair<int, ModelCell *>> ModelLabelsVector;
typedef std::vector<std::string> LabelsVector;
typedef std::vector<ModelCell *> ModelsVector;
typedef std::map<int, SLabelDetail> LabelIcons;

// REMOVE MEEE
class ModelsCategory: public std::list<ModelCell *>
{
public:
  char name[LEN_MODEL_FILENAME + 1];

  explicit ModelsCategory(const char * name);
  explicit ModelsCategory(const char * name, uint8_t len);
  ~ModelsCategory();

  ModelCell * addModel(const char * name);
  void removeModel(ModelCell * model);
  void moveModel(ModelCell * model, int8_t step);

  int getModelIndex(const ModelCell* model);

  void save(FIL * file);
};
// REMOVE MEEE ^^^^^


class ModelMap : protected std::multimap<int, ModelCell *>
{
  public:
    ModelsVector getModelsByLabel(std::string);
    LabelsVector getLabelsByModel(ModelCell *);
    int addLabel(std::string);
    void addMapping(std::string, ModelCell *);
    LabelsVector getLabels();
    int size() {return std::multimap<int, ModelCell *>::size();}
    void clear() {
      labels.clear();
      std::multimap<int, ModelCell *>::clear();
    }

    /*ModelLabelsVector getUniqueModelCells()
    {
      ModelLabelsVector rval;

      std::unique_copy(begin(),
                       end(),
                       std::back_inserter(rval),
                         [](const std::pair<std::string,ModelCell *> &entry1,
                           const std::pair<std::string,ModelCell *> &entry2) {
                             return (entry1.second == entry2.second);
                         });
      return rval;
    }*/

  private:
    // Storage for the label strings, keep here to so only one copy
    // of a each label required.
    LabelsVector labels;

    int getIndexByLabel(std::string str) {
      std::transform(str.begin(), str.end(), str.begin(), ::tolower);
      for(uint16_t i=0; i < (uint16_t)labels.size(); i++) {
        if(labels.at(i) == str) {
          return i;
        }
      }
      return -1;
    }

    std::string getLabelByIndex(uint16_t index) {
      if(index < (uint16_t)labels.size())
        return labels.at(index);
      else
        return std::string();
    }
};


class ModelsList : public std::vector<ModelCell *>
{
  bool loaded;

  std::list<ModelsCategory *> categories; // To be removed
  ModelsCategory * currentCategory;       // To be removed

  ModelCell * currentModel;
  unsigned int modelsCount;

  void init();

public:

  enum class Format {
    txt,
#if defined(SDCARD_YAML)
    yaml,
    yaml_txt,
    load_default = yaml,
#else
    load_default = txt,
#endif
  };

  ModelsList();
  ~ModelsList();

  bool load(Format fmt = Format::load_default);
  void save();
  void clear();

  const std::list<ModelsCategory *> & getCategories() const
  {
    return categories;
  }

  std::list<ModelsCategory *>& getCategories()
  {
    return categories;
  }

  void setCurrentCategory(ModelsCategory * cat);

  ModelsCategory * getCurrentCategory() const
  {
    return currentCategory;
  }
  int getCurrentCategoryIdx() const;

  void setCurrentModel(ModelCell * cell);

  ModelCell * getCurrentModel() const
  {
    return currentModel;
  }

  void incModelsCount() {
    modelsCount++;
  }

  unsigned int getModelsCount() const
  {
    return modelsCount;
  }

  bool readNextLine(char * line, int maxlen);

  ModelsCategory * createCategory(bool save=true);
  ModelsCategory * createCategory(const char * name, bool save=true);
  void removeCategory(ModelsCategory * category);

  ModelCell * addModel(ModelsCategory * category, const char * name, bool save=true);
  void removeModel(ModelsCategory * category, ModelCell * model);
  void moveModel(ModelsCategory * category, ModelCell * model, int8_t step);
  void moveModel(ModelCell * model, ModelsCategory * previous_category, ModelsCategory * new_category);

  bool isModelIdUnique(uint8_t moduleIdx, char* warn_buf, size_t warn_buf_len);
  uint8_t findNextUnusedModelId(uint8_t moduleIdx);

  void onNewModelCreated(ModelCell* cell, ModelData* model);

protected:
  FIL file;

  bool loadTxt();
#if defined(SDCARD_YAML)
  bool loadYaml();
  bool loadYamlDirScanner();
#endif
};


ModelLabelsVector getUniqueLabels();

extern ModelsList modelslist;
extern ModelMap modelslabels;

#endif // _MODELSLIST_H_
