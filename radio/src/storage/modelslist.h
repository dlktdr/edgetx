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
#include "rtc.h"

// modelXXXXXXX.bin F,FF F,3F,FF\r\n
#define LEN_MODELS_IDX_LINE (LEN_MODEL_FILENAME + sizeof(" F,FF F,3F,FF\r\n")-1)

#define DEFAULT_MODEL_SORT DATE_DES

struct ModelData;
struct ModuleData;

struct SimpleModuleData
{
  uint8_t type=0;
  uint8_t rfProtocol=0;
};

typedef union {
  struct {
	  FSIZE_t	fsize;		/* File size */
	  WORD	fdate;			/* Modified date */
	  WORD	ftime;			/* Modified time */
  };
  uint8_t data[8];
} FInfoH;

#define FILE_HASH_LENGTH (sizeof(FInfoH::data) * 2) // Hex string output

class ModelCell
{
  public:
    char modelFilename[LEN_MODEL_FILENAME + 1] = "";
    char modelName[LEN_MODEL_NAME + 1] = "";
    char modelFinfoHash[FILE_HASH_LENGTH + 1] = "";
#if LEN_BITMAP_NAME > 0
    char modelBitmap[LEN_BITMAP_NAME] = "";
#endif
    gtime_t lastOpened=0;
    bool _isDirty = true;

    bool             valid_rfData;
    uint8_t          modelId[NUM_MODULES] = {0,0};
    SimpleModuleData moduleData[NUM_MODULES];

    explicit ModelCell(const char * name);
    explicit ModelCell(const char * name, uint8_t len);

    void setModelName(char * name);
    void setModelName(char* name, uint8_t len);
    void setRfData(ModelData * model);

    void setModelId(uint8_t moduleIdx, uint8_t id);
    void setRfModuleData(uint8_t moduleIdx, ModuleData* modData);
};

typedef struct {
  std::string icon;
  // Anything else?
} SLabelDetail;

typedef std::vector<std::pair<int, ModelCell *>> ModelLabelsVector;
typedef std::vector<std::string> LabelsVector;
typedef std::vector<ModelCell *> ModelsVector;
typedef enum {
  NO_SORT,
  NAME_ASC,
  NAME_DES,
  DATE_ASC,
  DATE_DES,
} ModelsSortBy;

/**
 * @brief ModelMap is a multimap of all models and their cooresponding
 *        labels.
 *
 */

class ModelMap : protected std::multimap<uint16_t, ModelCell *>
{
  public:
    ModelsVector getUnlabeledModels(ModelsSortBy sortby=DEFAULT_MODEL_SORT);
    ModelsVector getModelsByLabel(const std::string &, ModelsSortBy sortby=DEFAULT_MODEL_SORT);
    LabelsVector getLabelsByModel(ModelCell *);
    std::map<std::string, bool> getSelectedLabels(ModelCell *);
    bool isLabelSelected(const std::string &, ModelCell *);
    LabelsVector getLabels();
    int addLabel(const std::string &);
    bool addLabelToModel(const std::string &, ModelCell *, bool update=false);
    bool removeLabelFromModel(const std::string &label, ModelCell *, bool update=false);
    bool removeLabel(const std::string &);
    bool moveLabelTo(unsigned current, unsigned newind);
    bool renameLabel(const std::string &from, const std::string &to);
    std::string getCurrentLabel() {return currentlabel;};
    void setCurrentLabel(const std::string &lbl) {currentlabel = lbl; setDirty();}
    std::string getLabelString(ModelCell *, const char *noresults="");
    void setDirty(bool save=false);
    bool isDirty() {return _isDirty;}

  protected:
    void updateModelCell(ModelCell *);
    bool removeModels(ModelCell *); // Should only be called from ModelsList remove model.
    bool updateModelFile(ModelCell *);
    void sortModelsBy(ModelsVector &mv, ModelsSortBy sortby);

    void clear() {
      _isDirty=true;
      labels.clear();
      std::multimap<uint16_t, ModelCell *>::clear();
    }


  private:
    bool _isDirty=true;
    LabelsVector labels; // Storage space for discovered labels
    std::string currentlabel = "";

    int getIndexByLabel(const std::string &str)
    {
      auto a = std::find(labels.begin(), labels.end(), str);
      return a == labels.end() ? -1 : a - labels.begin();
    }

    std::string getLabelByIndex(uint16_t index) {
      if(index < (uint16_t)labels.size())
        return labels.at(index);
      else
        return std::string();
    }

    friend class ModelsList;
};


class ModelsList : public std::vector<ModelCell *>
{
  bool loaded;

  ModelCell * currentModel;

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
  const char *save();
  void clear();

  void setCurrentModel(ModelCell * cell);
  void updateCurrentModelCell();

  ModelCell * getCurrentModel() const
  {
    return currentModel;
  }

  unsigned int getModelsCount() const
  {
    return std::vector<ModelCell *>::size();
  }

  bool readNextLine(char * line, int maxlen);

  ModelCell * addModel(const char * name, bool save=true);
  bool removeModel(ModelCell * model);
  bool moveModelTo(unsigned curindex, unsigned toindex);

  bool isModelIdUnique(uint8_t moduleIdx, char* warn_buf, size_t warn_buf_len);
  uint8_t findNextUnusedModelId(uint8_t moduleIdx);

  typedef struct {
    std::string name;
    char hash[FILE_HASH_LENGTH + 1];
    bool curmodel=false;
    bool celladded=false;
  } filedat;
  std::vector<filedat> fileHashInfo;

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
extern ModelMap modelsLabels;

#endif // _MODELSLIST_H_
