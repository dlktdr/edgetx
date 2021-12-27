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

#include "modelslist.h"
#include <algorithm>
#include <time.h>

using std::list;

#if defined(SDCARD_YAML)
#include "opentx.h"
#include "storage/sdcard_yaml.h"
#include "yaml/yaml_parser.h"
#include "yaml/yaml_labelslist.h"
#include "yaml/yaml_datastructs.h"
#endif

#include "myeeprom.h"
#include "datastructs.h"
#include "pulses/modules_helpers.h"
#include "strhelpers.h"
#include "datastructs.h"

#include <cstring>

ModelsList modelslist;
ModelMap modelsLabels;

ModelCell::ModelCell(const char* name) : valid_rfData(false)
{
  strncpy(modelFilename, name, sizeof(modelFilename)-1);
  modelFilename[sizeof(modelFilename)-1] = '\0';
#if LEN_BITMAP_NAME > 0
    modelBitmap[0] = '\0';
#endif

}

ModelCell::ModelCell(const char* name, uint8_t len) : valid_rfData(false)
{
  if (len > sizeof(modelFilename) - 1) len = sizeof(modelFilename) - 1;

  memcpy(modelFilename, name, len);
  modelFilename[len] = '\0';
#if LEN_BITMAP_NAME > 0
    modelBitmap[0] = '\0';
#endif
}

void ModelCell::setModelName(char * name)
{
  strncpy(modelName, name, LEN_MODEL_NAME);
  modelName[LEN_MODEL_NAME] = '\0';

  if (modelName[0] == '\0') {
    char * tmp;
    strncpy(modelName, modelFilename, LEN_MODEL_NAME);
    tmp = (char *) memchr(modelName, '.',  LEN_MODEL_NAME);
    if (tmp != nullptr)
      *tmp = 0;
  }
}

void ModelCell::setModelName(char* name, uint8_t len)
{
  if (len > LEN_MODEL_NAME-1)
    len = LEN_MODEL_NAME-1;

  memcpy(modelName, name, len);
  modelName[len] = '\0';

  if (modelName[0] == 0) {
    char * tmp;
    strncpy(modelName, modelFilename, LEN_MODEL_NAME);
    tmp = (char *) memchr(modelName, '.',  LEN_MODEL_NAME);
    if (tmp != nullptr)
      *tmp = 0;
  }
}

void ModelCell::setModelId(uint8_t moduleIdx, uint8_t id)
{
  modelId[moduleIdx] = id;
}

void ModelCell::setRfData(ModelData* model)
{
  for (uint8_t i = 0; i < NUM_MODULES; i++) {
    modelId[i] = model->header.modelId[i];
    setRfModuleData(i, &(model->moduleData[i]));
    TRACE("<%s/%i> : %X,%X,%X",
          strlen(modelName) ? modelName : modelFilename,
          i, moduleData[i].type, moduleData[i].rfProtocol, modelId[i]);
  }
  valid_rfData = true;
}

void ModelCell::setRfModuleData(uint8_t moduleIdx, ModuleData* modData)
{
  moduleData[moduleIdx].type = modData->type;
  if (modData->type != MODULE_TYPE_MULTIMODULE) {
    moduleData[moduleIdx].rfProtocol = (uint8_t)modData->rfProtocol;
  }
  else {
    // do we care here about MM_RF_CUSTOM_SELECTED? probably not...
    moduleData[moduleIdx].rfProtocol = modData->getMultiProtocol();
  }
}

//-----------------------------------------------------------------------------

ModelsVector ModelMap::getUnlabeledModels()
{
  ModelsVector unlabeledModels;
  for (auto model : modelslist) {
    if (modelsLabels.getLabelsByModel(model).size() == 0)
      unlabeledModels.emplace_back(model);
  }
  return unlabeledModels;
}

ModelsVector ModelMap::getModelsByLabel(const std::string &lbl)
{
  int index = getIndexByLabel(lbl);
  if(index < 0)
    return ModelsVector();
  ModelsVector rv;
  for (auto it = begin(); it != end(); ++it) {
    if(it->first == index)
      rv.push_back(it->second);
  }
  return rv;
}

LabelsVector ModelMap::getLabelsByModel(ModelCell *mdl)
{
  if(mdl == nullptr) return LabelsVector();
  LabelsVector rv;
  for (auto it = begin(); it != end(); ++it) {
    if(it->second == mdl) {
      rv.push_back(getLabelByIndex(it->first));
    }
  }
  return rv;
}

std::map<std::string, bool> ModelMap::getSelectedLabels(ModelCell *cell)
{
  std::map<std::string, bool> rval;
  // Loop through all labels and add to the map default to false
  for(auto lbl : labels) {
      rval[lbl] = false;
  }
  // Loop through all labels selected by the model, set them true
  LabelsVector modlbl = getLabelsByModel(cell);
  for(const auto& ml : modlbl) {
    rval[ml] = true;
  }
  return rval;
}

bool ModelMap::isLabelSelected(const std::string &label, ModelCell *cell)
{
  auto lbm = getLabelsByModel(cell);
  if(std::find(lbm.begin(), lbm.end(), label) == lbm.end()) {
    return false;
  }
  return true;
}

LabelsVector ModelMap::getLabels()
{
  LabelsVector capitalizedLabels;
  for (auto label : labels) {
    capitalizedLabels.emplace_back(label);
  }

  return capitalizedLabels;
}

int ModelMap::addLabel(const std::string &lbl)
{
  // Add a new label if if doesn't already exist in the list
  // Returns the index to the label
  int ind = getIndexByLabel(lbl);
  if(ind < 0) {
    labels.push_back(lbl);
    setDirty();
    TRACE("Added a label %s", lbl.c_str());
    return labels.size() - 1;
  }
  return ind;
}

bool ModelMap::addLabelToModel(const std::string &lbl, ModelCell *cell)
{
  // First check that there aren't too many labels on this model
  LabelsVector lbs = getLabelsByModel(cell);
  int sz = 0;
  for(auto const &it : lbs) {
    sz = it.size() + 1; // Label length + ','
  }
  sz += lbl.size() + 1; // New label + ','
  if(sz > LABELS_LENGTH - 1) {
    TRACE("Cannot add the %s label to the model. Too many labels", lbl.c_str());
    return false;
  }

  setDirty();
  int labelindex = addLabel(lbl);
  insert(std::pair<int, ModelCell *>(labelindex,cell));
  return true;
}

bool ModelMap::removeLabelFromModel(const std::string &label, ModelCell *cell)
{
  int lblind=getIndexByLabel(label);
  if(lblind < 0)
    return false;
  bool rv=false;
  // Erase items that match in the map
  for (ModelMap::const_iterator itr = cbegin() ; itr != cend() ; ) {
    itr = (itr->first == lblind && itr->second == cell) ? erase(itr) : std::next(itr);
    setDirty();
    rv = true;
  }
  return rv;
}

bool ModelMap::renameLabel(const std::string &from, const std::string &to)
{
  ModelData *modeldata = (ModelData*)malloc(sizeof(ModelData));
  if(!modeldata) {
    TRACE("Labels: Out Of Memory");
    return true;
  }

  bool fault = false;
  ModelsVector mods = getModelsByLabel(from); // Find all models to be renamed
  for (const auto &modcell: mods) {
    readModelYaml(modcell->modelFilename, (uint8_t*)modeldata, sizeof(ModelData));

    // Make sure there is room to rename
    int nlen = strlen(modeldata->header.labels) + to.size() - from.size();
    if(nlen > LABELS_LENGTH - 1) {
      fault = true;
      TRACE("Labels: Rename Error! Labels too long on %s - %s", modeldata->header.name,
                                                                modcell->modelFilename);
      continue;
    }
    char buffer[LABELS_LENGTH];
    char *loc = strstr(modeldata->header.labels, from.c_str());
    if(loc == NULL) {
      TRACE("Labels: Rename Error! Could not find label in model?");
      fault = true;
      continue;
    }
    strncpy(buffer, modeldata->header.labels, loc - modeldata->header.labels);
    TRACE("Lables after rename 1: %s", buffer);
    strcat(buffer, to.c_str());
    TRACE("Lables after rename 2: %s", buffer);
    strcat(buffer, loc + from.size());
    TRACE("Lables after rename 3: %s", buffer);

    char path[256];
    getModelPath(path, modcell->modelFilename);

    if(modcell == modelslist.getCurrentModel()) {
      // If working on the current model, write current data to file instead
      memcpy(modeldata->header.labels,g_model.header.labels, LABELS_LENGTH);
      fault = (writeFileYaml(path, get_modeldata_nodes(), (uint8_t*)&g_model) == NULL);
    } else {
      fault = (writeFileYaml(path, get_modeldata_nodes(), (uint8_t*)modeldata) == NULL);
    }
  }

  free(modeldata);

  // Issue a rescan all of all models. This will cause a decent delay
  // depending how many files were renamed above.
  modelslist.load();

  return fault;
}

bool ModelMap::removeModels(ModelCell *cell)
{
  bool rv=false;
  // Erase items that match in the map
  for (ModelMap::const_iterator itr = cbegin() ; itr != cend() ; ) {
    itr = (itr->second == cell) ? erase(itr) : std::next(itr);
    setDirty();
    rv = true;
  }
  return rv;
}

void ModelMap::setDirty()
{
  _isDirty = true;
  storageDirty(EE_LABELS);
}

//-----------------------------------------------------------------------------

ModelsList::ModelsList()
{
  init();
}

ModelsList::~ModelsList()
{
  clear();
}

void ModelsList::init()
{
  loaded = false;
  currentModel = nullptr;
}

void ModelsList::clear()
{
  std::vector<ModelCell *>::clear();
  init();
}

bool ModelsList::loadTxt()
{
  char line[LEN_MODELS_IDX_LINE+1];
  ModelCell * model = nullptr;

  FRESULT result = f_open(&file, RADIO_MODELSLIST_PATH, FA_OPEN_EXISTING | FA_READ);
  if (result == FR_OK) {

    // TXT reader
    while (readNextLine(line, LEN_MODELS_IDX_LINE)) {
      int len = strlen(line); // TODO could be returned by readNextLine
      if (len > 2 && line[0] == '[' && line[len-1] == ']') {
        line[len-1] = '\0';
      }
      else if (len > 0) {
        model = new ModelCell(line);
        push_back(model);
        if (!strncmp(line, g_eeGeneral.currModelFilename, LEN_MODEL_FILENAME)) {
          currentModel = model;
        }
      }
    }

    f_close(&file);
    return true;
  }

  return false;
}

#if defined(SDCARD_YAML)

/* updateModelCell
 *     Opens a YAML file, reads the data and updates the ModelCell
  */

void ModelMap::updateModelCell(ModelCell *cell)
{
  modelsLabels.removeModels(cell);

  ModelData *model = (ModelData*)malloc(sizeof(ModelData)); // TODO HOPE this isn't too much extra ram on some targets... :()
  if(!model) {
    TRACE("Labels: Out Of Memory");
    return;
  }

  readModelYaml(cell->modelFilename, (uint8_t*)model, sizeof(ModelData));
  strcpy(cell->modelName, model->header.name);
  strcpy(cell->modelBitmap, model->header.bitmap);
  char *cma;
  cma = strtok(model->header.labels, ",");
  while(cma != NULL) {
    modelsLabels.addLabelToModel(cma,cell);
    cma = strtok(NULL, ",");
  }

  // Save Module Data
  cell->setRfData(model);
  for(int i=0; i < NUM_MODULES; i++) {
    cell->setRfModuleData(i,model->moduleData);
  }

  cell->_isDirty = false;
  free(model);
}

// Simple Hash of the Files

char *FILInfoToHexStr(char buffer[17], FILINFO *finfo)
{
  FInfoH fh;
  fh.fdate = finfo->fdate;
  fh.fsize = finfo->fsize;
  fh.ftime = finfo->ftime;
  char *str = buffer;
  for(unsigned int i=0; i < sizeof(FInfoH::data); i++) {
    sprintf(str,"%02x",fh.data[i]);
    str+=2;
  }
  return buffer;
}

bool ModelsList::loadYaml()
{
  // Clear labels + map
  modelslist.clear();
  modelsLabels.clear();

  // 1) Scan /MODELS/ for all .yml models
  //    - Create a ModelCell for each model in ModelList
  //    - Create a hash based on a the file info (name,size,modified,etc...) Used to detect a change in the file

  // 2) Read Labels.yml
  //    - Check if the modelfile name exists in Modellist. Ignore if the file isn't there will be removed from labels.yml on next write
  //    - If file exists in modelslist compare the file hash to the Modellist one.
  //    - If has different open the .yml file and grab all information, update modelslist
  //         Labels
  //         Model Name
  //         Module ID's
  //         Set Last Opened to now() (Sort by last used option)

  // 3) Loop over all ModelsList items which are now synced to real files
  //     Add label to ModelLabelsUnorderedMultiMap<std::string, ModelCell*>

  // 4) Fav label will always be created if it doesn't already exist *** TODO

  DEBUG_TIMER_START(debugTimerYamlScan);

  // 1) Scan /MODELS/ for all .yml models

  DIR moddir;
  FILINFO finfo;
  if (f_opendir(&moddir, PATH_SEPARATOR MODELS_PATH) == FR_OK) {
    for (;;) {
      FRESULT res = f_readdir(&moddir, &finfo);
      if (res != FR_OK || finfo.fname[0] == 0) break;
      if(finfo.fattrib & AM_DIR) continue;
      int len = strlen(finfo.fname);
      if (len < 5 ||
          strcasecmp(finfo.fname + len - 4, YAML_EXT) || // Skip non .yml files
          strcasecmp(finfo.fname, LABELS_FILENAME) == 0  || // Skip labels.yml file
          strcasecmp(finfo.fname, MODELS_FILENAME) == 0  || // Skip models.yml file
          (finfo.fattrib & AM_DIR)) { // Skip sub dirs
        continue;
      }

      char hbuf[FILE_HASH_LENGTH + 1];
      FILInfoToHexStr(hbuf, &finfo);
      TRACE("File - %s \r\n  HASH - %s", finfo.fname, hbuf);

      // Create and add the model
      ModelCell *model = new ModelCell(finfo.fname);
      strcpy(model->modelFinfoHash, hbuf);
      modelslist.push_back(model);

      // See if this is the current model
      if (!strncmp(finfo.fname, g_eeGeneral.currModelFilename, LEN_MODEL_FILENAME)) {
        TRACE("FOUND CURRENT MODEL, loading");
        currentModel = model;
      }
    }
    f_closedir(&moddir);
  }

  DEBUG_TIMER_SAMPLE(debugTimerYamlScan);
#ifdef DEBUG_TIMERS
  TRACE("  $$$$$$$$$$$  Time to scan models and create file hashes %lu us\r\n\r\n", debugTimers[debugTimerYamlScan].getLast());
#endif

  // 2) Read Labels.yml

  char line[LEN_MODELS_IDX_LINE+1];
  FRESULT result = f_open(&file, LABELSLIST_YAML_PATH, FA_OPEN_EXISTING | FA_READ);
  if (result == FR_OK) {
    YamlParser yp;
    void* ctx = get_labelslist_iter();

    yp.init(get_labelslist_parser_calls(), ctx);

    UINT bytes_read=0;
    while (f_read(&file, line, sizeof(line), &bytes_read) == FR_OK) {

      // reached EOF?
      if (bytes_read == 0)
        break;

      if (yp.parse(line, bytes_read) != YamlParser::CONTINUE_PARSING)
        break;
    }

    f_close(&file);
  }

  DEBUG_TIMER_SAMPLE(debugTimerYamlScan);
#ifdef DEBUG_TIMERS
  TRACE("  $$$$$$$$$$$  Time to compare models %lu us", debugTimers[debugTimerYamlScan].getLast());
#endif

  // Scan all models, see which ones need updating
  for(const auto &model: modelslist) {
    if(model->_isDirty) {
      modelsLabels.updateModelCell(model);
    }
  }

  DEBUG_TIMER_SAMPLE(debugTimerYamlScan);
#ifdef DEBUG_TIMERS
  TRACE("  $$$$$$$$$$$  Time to scan all models that needed updating %lu us\r\n\r\n", debugTimers[debugTimerYamlScan].getLast());
#endif

  // Scan all models, see which ones need updating
  for(const auto &label: modelsLabels.getLabels()) {
    TRACE("LABEL Found %s", label.c_str());
  }

  if(!modelslist.currentModel) {
    TRACE("ERROR no Current Model Found");
    // TODO
  }

  // Save output
  modelslist.save();

  return true;
}
#endif

bool ModelsList::load(Format fmt)
{
  if (loaded)
    return true;

  bool res = false;
#if !defined(SDCARD_YAML)
  (void)fmt;
  res = loadTxt();
#else
  FILINFO fno;
  if (fmt == Format::txt ||
      (fmt == Format::yaml_txt &&
       f_stat(MODELSLIST_YAML_PATH, &fno) != FR_OK &&
       f_stat(FALLBACK_MODELSLIST_YAML_PATH, &fno) != FR_OK)) {
    res = loadTxt();
  } else {
    res = loadYaml();
  }
#endif

  if (!currentModel) { // TODO
  }

  loaded = true;
  return res;
}

const char * ModelsList::save()
{
#if !defined(SDCARD_YAML)
  FRESULT result = f_open(&file, RADIO_MODELSLIST_PATH, FA_CREATE_ALWAYS | FA_WRITE);
#else
  FRESULT result = f_open(&file, LABELSLIST_YAML_PATH, FA_CREATE_ALWAYS | FA_WRITE);
#endif
  if (result != FR_OK) return "Couldn't open labels.yml for writing";
  f_puts("- Models:\r\n", &file);
  for (auto const& model : modelslist) {
    f_puts("  - ", &file);
    f_puts(model->modelFilename, &file);
    f_puts(":\r\n", &file);

      f_puts("    - hash: \"", &file);
      f_puts(model->modelFinfoHash, &file);
      f_puts("\"\r\n", &file);

      f_puts("      name: \"", &file);
      f_puts(model->modelName, &file);
      f_puts("\"\r\n", &file);

      // TODO Maybe make sub-items.. or not use at all?
      f_printf(&file, "      id1: %u\r\n",(unsigned int)model->modelId[0]);
#if NUM_MODULES == 2
      f_printf(&file, "      id2: %u\r\n",(unsigned int)model->modelId[1]);
#endif

      f_printf(&file, "      mod1type: %u\r\n",(unsigned int)model->moduleData[0].type);
#if NUM_MODULES == 2
      f_printf(&file, "      mod2type: %u\r\n",(unsigned int)model->moduleData[1].type);
#endif

      f_puts("      labels: \"", &file);
      LabelsVector labels = modelsLabels.getLabelsByModel(model);
      bool comma=false;
      for(auto const& label: labels) {
        if(comma) {
          f_puts(",", &file);
        }
        f_puts(label.c_str(), &file);
        comma = true;
      }
      f_puts("\"\r\n", &file);

#if LEN_BITMAP_NAME > 0
      f_puts("      bitmap: \"", &file);
      f_puts(model->modelBitmap, &file);
      f_puts("\"\r\n", &file);
#endif

      f_printf(&file, "      lastopen: %lld\r\n", (long long)model->lastOpened);
  }

  // Save current selection
  f_puts("\r\n  Labels:\r\n", &file);
  f_puts("  - selection:", &file);
  f_puts(modelsLabels.getCurrentLabel().c_str(), &file);
  f_puts("\r\n", &file);

  f_puts("\r\n", &file);
  f_close(&file);
  modelsLabels._isDirty = false;
  return NULL;
}

void ModelsList::setCurrentModel(ModelCell * cell)
{
  currentModel = cell;
  cell->lastOpened = (long long)time(NULL);
  modelsLabels.setDirty();

  /// TODO
  /*
  if (!currentModel->valid_rfData)
    currentModel->fetchRfData();
  */
}

void ModelsList::updateCurrentModelCell()
{
  auto mdl = std::find(begin(), end(), currentModel);
  if(mdl != end()) {
    strcpy((*mdl)->modelBitmap,g_model.header.bitmap);
    (*mdl)->setModelName(g_model.header.name);
    (*mdl)->setRfData(&g_model);
    for(int i=0; i < NUM_MODULES; i++) {
      //(*mdl)->setModelId(i, g_model.);
    }
  } else {
    TRACE("ModelList Error - Can't find current model");
  }
}

bool ModelsList::readNextLine(char * line, int maxlen)
{
  if (f_gets(line, maxlen, &file) != NULL) {
    int curlen = strlen(line) - 1;
    if (line[curlen] == '\n') { // remove unwanted chars if file was edited using windows
      if (line[curlen - 1] == '\r') {
        line[curlen - 1] = 0;
      }
      else {
        line[curlen] = 0;
      }
    }
    return true;
  }
  return false;
}


ModelCell * ModelsList::addModel(const char * name, bool save)
{
  if(name == nullptr) return nullptr;
  ModelCell * result = new ModelCell(name);
  push_back(result);
  if (save) this->save();
  return result;
}

void ModelsList::removeModel(ModelCell * model)
{
  std::remove(modelslist.begin(), modelslist.end(), model);
  modelsLabels.removeModels(model);
  save();
}

bool ModelsList::isModelIdUnique(uint8_t moduleIdx, char* warn_buf, size_t warn_buf_len)
{
  ModelCell* modelCell = modelslist.getCurrentModel();
  if (!modelCell || !modelCell->valid_rfData) {
    // in doubt, pretend it's unique
    return true;
  }

  uint8_t modelId = modelCell->modelId[moduleIdx];
  uint8_t type = modelCell->moduleData[moduleIdx].type;
  uint8_t rfProtocol = modelCell->moduleData[moduleIdx].rfProtocol;

  uint8_t additionalOnes = 0;
  char* curr = warn_buf;
  curr[0] = 0;

  bool hit_found = false;

  for (auto it = begin(); it != end(); it++) {
    if (modelCell == *it)
      continue;

    if (!(*it)->valid_rfData)
      continue;

    if ((type != MODULE_TYPE_NONE) &&
        (type       == (*it)->moduleData[moduleIdx].type) &&
        (rfProtocol == (*it)->moduleData[moduleIdx].rfProtocol) &&
        (modelId    == (*it)->modelId[moduleIdx])) {

      // Hit found!
      hit_found = true;

      const char * modelName = (*it)->modelName;
      const char * modelFilename = (*it)->modelFilename;

      // you cannot rely exactly on WARNING_LINE_LEN so using WARNING_LINE_LEN-2 (-2 for the ",")
      if ((warn_buf_len - 2 - (curr - warn_buf)) > LEN_MODEL_NAME) {
        if (warn_buf[0] != 0)
          curr = strAppend(curr, ", ");
        if (modelName[0] == 0) {
          size_t len = min<size_t>(strlen(modelFilename),LEN_MODEL_NAME);
          curr = strAppendFilename(curr, modelFilename, len);
        }
        else
          curr = strAppend(curr, modelName, LEN_MODEL_NAME);
      }
      else {
        additionalOnes++;
      }
    }
  }

  if (additionalOnes && (warn_buf_len - (curr - warn_buf) >= 7)) {
    curr = strAppend(curr, " (+");
    curr = strAppendUnsigned(curr, additionalOnes);
    curr = strAppend(curr, ")");
  }

  return !hit_found;
}

uint8_t ModelsList::findNextUnusedModelId(uint8_t moduleIdx)
{
  ModelCell * modelCell = modelslist.getCurrentModel();
  if (!modelCell || !modelCell->valid_rfData) {
    return 0;
  }

  uint8_t type = modelCell->moduleData[moduleIdx].type;
  uint8_t rfProtocol = modelCell->moduleData[moduleIdx].rfProtocol;

  uint8_t usedModelIds[(MAX_RXNUM + 7) / 8];
  memset(usedModelIds, 0, sizeof(usedModelIds));

  for (auto it = begin(); it != end(); it++){
    if (modelCell == *it)
      continue;

    if (!(*it)->valid_rfData)
      continue;

    // match module type and RF protocol
    if ((type != MODULE_TYPE_NONE) &&
        (type       == (*it)->moduleData[moduleIdx].type) &&
        (rfProtocol == (*it)->moduleData[moduleIdx].rfProtocol)) {

      uint8_t id = (*it)->modelId[moduleIdx];
      uint8_t mask = 1 << (id & 7u);
      usedModelIds[id >> 3u] |= mask;
    }
  }

  for (uint8_t id = 1; id <= getMaxRxNum(moduleIdx); id++) {
    uint8_t mask = 1u << (id & 7u);
    if (!(usedModelIds[id >> 3u] & mask)) {
      // found free ID
      return id;
    }
  }

  // failed finding something...
  return 0;
}

void ModelsList::onNewModelCreated(ModelCell* cell, ModelData* model)
{
  cell->setModelName(model->header.name);
  cell->setRfData(model);

  uint8_t new_id = findNextUnusedModelId(INTERNAL_MODULE);
  model->header.modelId[INTERNAL_MODULE] = new_id;
  cell->setModelId(INTERNAL_MODULE, new_id);
}
