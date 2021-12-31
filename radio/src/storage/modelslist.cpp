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
/**
 * @brief Gets all models which don't have any labels selected
 *
 * @return ModelsVector vector<ModelCell>
 */

ModelsVector ModelMap::getUnlabeledModels()
{
  ModelsVector unlabeledModels;
  for (auto model : modelslist) {
    if (modelsLabels.getLabelsByModel(model).size() == 0)
      unlabeledModels.emplace_back(model);
  }
  return unlabeledModels;
}

/**
 * @brief Returns all models that have a sepecified label
 *
 * @param lbl Label to search
 * @return ModelsVector aka vector<ModelCell*> of all models belonging to a label
 */

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

/**
 * @brief Gets all labels that a model currently has selected
 *
 * @param mdl Model to search
 * @return LabelsVector aka vector<string> of all labels selected by a model
 */

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

/**
 * @brief Get a map of all labels and their selection status
 * @details Returns a map of all the labels and if they are selected in a model.
 *
 * @param cell ModelCell to search
 * @return std::map<std::string, bool> Map Containing all labels with a boolean if they are selected
 */

std::map<std::string, bool> ModelMap::getSelectedLabels(ModelCell *cell)
{
  std::map<std::string, bool> rval;
  for(auto lbl : labels) { // Add all labels
    if(lbl == "")
      continue;
    rval[lbl] = false;
  }
  LabelsVector modlbl = getLabelsByModel(cell);
  for(const auto& ml : modlbl) { // Set to true if selected by the model
    rval[ml] = true;
  }
  return rval;
}

/**
 * @brief Returns if a label is selected in the model
 *
 * @param label Label to search
 * @param cell Model to search
 * @return true Label is selected
 * @return false Label not seleced OR label not found
 */

bool ModelMap::isLabelSelected(const std::string &label, ModelCell *cell)
{
  auto lbm = getLabelsByModel(cell);
  if(std::find(lbm.begin(), lbm.end(), label) == lbm.end()) {
    return false;
  }
  return true;
}

/**
 * @brief Returns a list of the current labels
 *
 * @return LabelsVector vector<string> of all labels
 */

LabelsVector ModelMap::getLabels()
{
  LabelsVector rv;
  for (auto &label : labels) {
    if(label.size() > 0)
      rv.emplace_back(label);
  }
  return rv;
}

/**
 * @brief Adds a label
 * @details  Checks if the label already exists. If it does it returns the
 *           index to it. If label doesn't exist it adds it at the end of the
 *           list and returns the new index
 *           Won't allow creation of the special case label "Unlabeled" STR_UNLABELEDMODEL
 *
 * @param lbl Adds a label to the list
 * @return int -1 on failure, label index on success
 */

int ModelMap::addLabel(const std::string &lbl)
{
  if(lbl.size() == 0)
    return -1;
  if(lbl == STR_UNLABELEDMODEL)
    return -1;
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

/**
 * @brief Adds a label to a model
 *
 * @param lbl Label to be added
 * @param cell Model to add the label
 * @return true Couldn't add label, not enough memory available in labels string
 * @return false Success
 */

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
    return true;
  }

  setDirty();
  int labelindex = addLabel(lbl);
  insert(std::pair<int, ModelCell *>(labelindex,cell));
  return false;
}

/**
 * @brief Removes a label from a model
 *
 * @param label Label to be removed
 * @param cell Model to remove the label from
 * @return true Couldn't find the label
 * @return false Success
 */

bool ModelMap::removeLabelFromModel(const std::string &label, ModelCell *cell)
{
  int lblind=getIndexByLabel(label);
  if(lblind < 0)
    return true;
  bool rv=true;
  // Erase items that match in the map
  for (ModelMap::const_iterator itr = cbegin() ; itr != cend() ; ) {
    itr = (itr->first == lblind && itr->second == cell) ? erase(itr) : std::next(itr);
    setDirty();
    rv = false;
  }
  return rv;
}

/**
 * @brief Removes a label
 * @details Remove a label from the list, only if there are no models that have
 *          the label selected. The label isn't actually removed only set blank
 *          this is to keep the proper index of the multimap.
 *          On reboot it will be re-synced again.
 *
 * @param label Label to be removed
 * @return true Label wasn't found or not empty
 * @return false Success
 */

bool ModelMap::removeLabel(const std::string &label)
{
  // Remove all the labels
  renameLabel(label, "");
  for(auto &lbl : labels) {
    if(lbl == label && getModelsByLabel(lbl).size() == 0) {
      lbl = "";
      setDirty();
      return false;
    }
  }
  return true;
}

/**
 * @brief Moves a labels order
 *
 * @param current Index of the label to move
 * @param toindex Index where to place the new label
 * @return 1 Failure
 * @return 0 Success
 */

bool ModelMap::moveLabelTo(unsigned curind, unsigned newind)
{
  if(curind == newind ||
     curind >= labels.size() ||
     newind >= labels.size())
    return true;

  if(labels.at(curind) == "")
    return true;

  if(curind < newind) { // Move forward
    std::rotate(labels.rend() - curind - 1, labels.rend() - curind, labels.rend() - newind);
  } else { // Move back
    std::rotate(labels.begin() + curind, labels.begin() + curind + 1, labels.begin() + newind + 1);
  }

  // Reload the new labels order
  modelslist.save();
  modelslist.clear();
  modelslist.load();

  return false;
}

/**
 * @brief Rename a label
 * @details Opens all models which have a label that matches the <from> string.
 *          Renames the label with <to> string and saves file.
 *          If working on the current model it replaces the label string in
 *            g_model and saves the current copy in memory.
 *
 * @param from Label to search
 * @param to Replacement label
 * @return true failure Label couldn't be found, YAML couldn't be read
 * @return false success
 */

bool ModelMap::renameLabel(const std::string &from, const std::string &to)
{
  if(from == "")
    return true;

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

    // Separate CSV
    LabelsVector lbls;
    char *cma ;
    cma = strtok(modeldata->header.labels, ",");
    int numTokens = 0;
    while(cma != NULL) {
      lbls.push_back(cma);
      cma = strtok(NULL, ",");
      numTokens++;
    }

    // Replace from->to strings
    for(auto &lbl: lbls) {
      if(lbl == from)
        lbl = to;
    }

    // Remove duplicates
    std::sort(lbls.begin(), lbls.end());
    auto last = std::unique(lbls.begin(), lbls.end());
    lbls.erase(last, lbls.end());
    lbls.resize(std::distance(lbls.begin(), last));

    // Write back
    bool comma=false;
    modeldata->header.labels[0] = '\0';
    for(auto lbl: lbls) {
      if(lbl == "")
        continue;
      if(comma)
        strcat(modeldata->header.labels, ",");
      strcat(modeldata->header.labels, lbl.c_str());
      comma = true;
    }

    char path[256];
    getModelPath(path, modcell->modelFilename);

    if(&(*modcell) == modelslist.getCurrentModel()) {
      // If working on the current model, write current data to file instead
      memcpy(g_model.header.labels, modeldata->header.labels, LABELS_LENGTH);
      fault = (writeFileYaml(path, get_modeldata_nodes(), (uint8_t*)&g_model) != NULL);
    } else {
      fault = (writeFileYaml(path, get_modeldata_nodes(), (uint8_t*)modeldata) != NULL);
    }
  }

  // Rename the label
  for(auto &lbl: labels) {
    if(lbl == from) {
      lbl = to;
      setDirty(true);
    }
  }

  free(modeldata);

  // Issue a rescan all of all models.
  modelslist.clear();
  modelslist.load();

  return fault;
}

/**
 * @brief Removes all models from the map
 *
 * @param cell Model to remove
 * @return true Failure finding a model
 * @return false Success
 */

bool ModelMap::removeModels(ModelCell *cell)
{
  bool rv=true;
  // Erase items that match in the map
  for (ModelMap::const_iterator itr = cbegin() ; itr != cend() ; ) {
    if(itr->second == cell) {
      itr = erase(itr);
      setDirty();
      rv = false;
    } else {
      itr = std::next(itr);
    }
  }
  return rv;
}

/**
 * @brief Sets the ModelMap to dirty.
 * @details Causes labels.yml to be written after a delay in sdcard_common.cpp->storageCheck()
 */

void ModelMap::setDirty(bool save)
{
  _isDirty = true;
  storageDirty(EE_LABELS);
  if(save) storageCheck(true);
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

/**
 * @brief Load and parse the models.txt file
 *
 * @return true On Success
 * @return false On Failure
 */

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

 /**
  * @brief Opens a YAML file, reads the data and updates the ModelCell
  *
  * @param cell Model to update
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

/**
 * @brief Creates a Hash based on the file information
 *
 * @param buffer Buffer to store the output data
 * @param finfo Files info handle
 * @return char* Pointer to buffer supplied
 */

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

/**
 * @brief Loads the Labels and Models from the labels.yml file
 *
 * @return true On success
 * @return false On failure
 */

bool ModelsList::loadYaml()
{
  // Clear labels + map
  modelslist.clear();
  modelsLabels.clear();
  fileHashInfo.clear();

  DEBUG_TIMER_START(debugTimerYamlScan);

  // Scan all models in folder
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

      // Store hash & filename
      filedat cf;
      FILInfoToHexStr(cf.hash, &finfo);
      cf.name = finfo.fname;
      cf.celladded = false;
      if (!strncmp(finfo.fname, g_eeGeneral.currModelFilename, LEN_MODEL_FILENAME))
        cf.curmodel = true;
      else
        cf.curmodel = false;
      fileHashInfo.push_back(cf);
      TRACE("File - %s \r\n  HASH - %s - CM: %s", finfo.fname, cf.hash, cf.curmodel?"Y":"N");
    }
    f_closedir(&moddir);
  }

  // Scan labels.yml
  char line[LEN_MODELS_IDX_LINE+1];
  FRESULT result = f_open(&file, LABELSLIST_YAML_PATH, FA_OPEN_EXISTING | FA_READ);
  if (result == FR_OK) {
    YamlParser yp;
    void* ctx = get_labelslist_iter();
    yp.init(get_labelslist_parser_calls(), ctx);
    UINT bytes_read=0;
    while (f_read(&file, line, sizeof(line), &bytes_read) == FR_OK) {
      if (bytes_read == 0) break;
      if (yp.parse(line, bytes_read) != YamlParser::CONTINUE_PARSING) break;
    }
    f_close(&file);
  }

  // Add modelcells for any remaining models that weren't in labels.yml
  for(auto &filehash : fileHashInfo) {
    ModelCell *model = NULL;
    if(filehash.celladded == false) {
      TRACE("  Created a modelcell for %s, not in labels.yml", filehash.name.c_str());
      model = new ModelCell(filehash.name.c_str());
      strcpy(model->modelFinfoHash, filehash.hash);
      modelslist.push_back(model);
      filehash.celladded = true;
      model->_isDirty = true;
    }
  }

  // Scan all models, to see if update needed
  bool updatelabelsyml=false;
  for(auto &model : modelslist) {
    // Open and read each model if it's marked as dirty
    if(model->_isDirty) {
      updatelabelsyml = true;
      modelsLabels.updateModelCell(model);
    }
  }

  fileHashInfo.clear();

  // If any items differed save the file
  if(updatelabelsyml == true) {
    TRACE("LABELS.YML Wasn't in sync. Needs to be saved");
    modelslist.save();
  } else {
    TRACE("LABELS.YML Is in Sync! No models were read");
  }

  if(!modelslist.currentModel) {
    TRACE("ERROR no Current Model Found");
    if(modelslist.size()) {
      modelslist.setCurrentModel(modelslist.at(0));
      TRACE("  - Set current model to first available");
    } else {
      TRACE("  - No Models Found, making a new one");
      // No models found, make a new one
      auto model = modelslist.addModel(createModel(), true);
      modelslist.setCurrentModel(model);
    }
  }

  return true;
}
#endif

/**
 * @brief Called to load the model data from file
 *
 * @param fmt Format::txt - Opens models.txt file, Format::yaml_txt - Opens labels.yml
 * @return true on success
 * @return false on failure
 */

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

/**
 * @brief Writes labels.yml file
 *
 * @return const char* NULL on success
 * @return const char* Error String on failure
 */

const char * ModelsList::save()
{
#if !defined(SDCARD_YAML)
  FRESULT result = f_open(&file, RADIO_MODELSLIST_PATH, FA_CREATE_ALWAYS | FA_WRITE);
#else
  FRESULT result = f_open(&file, LABELSLIST_YAML_PATH, FA_CREATE_ALWAYS | FA_WRITE);
#endif
  if (result != FR_OK) return "Couldn't open labels.yml for writing";

  // Save current selection
  f_puts("- Labels:\r\n", &file);

  std::string cursel = modelsLabels.getCurrentLabel();
  LabelsVector lbls = modelsLabels.getLabels();
  for(auto &lbl : lbls) {
    f_printf(&file, "  - %s:\r\n", lbl.c_str());
    if(lbl == cursel)
      f_printf(&file, "    - selected: true\r\n", lbl.c_str());
  }

  f_puts("  Models:\r\n", &file);
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
      f_printf(&file, "      modid0: %u\r\n",(unsigned int)model->modelId[0]);
#if NUM_MODULES == 2
      f_printf(&file, "      modid1: %u\r\n",(unsigned int)model->modelId[1]);
#endif

      f_printf(&file, "      mod0type: %u\r\n",(unsigned int)model->moduleData[0].type);
#if NUM_MODULES == 2
      f_printf(&file, "      mod1type: %u\r\n",(unsigned int)model->moduleData[1].type);
#endif

      f_printf(&file, "      mod0protocol: %u\r\n",(unsigned int)model->moduleData[0].rfProtocol);
#if NUM_MODULES == 2
      f_printf(&file, "      mod1protocol: %u\r\n",(unsigned int)model->moduleData[1].rfProtocol);
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

  f_puts("\r\n", &file);
  f_close(&file);
  modelsLabels._isDirty = false;

  return NULL;
}

/**
 * @brief set the currently loaded model.
 *
 * @param cell Model to set as current
 */

void ModelsList::setCurrentModel(ModelCell * cell)
{
  currentModel = cell;
  cell->lastOpened = (long long)time(NULL);
  updateCurrentModelCell();
  modelsLabels.setDirty();
}

/**
 * @brief Takes the current g_model data in memory and updates the ModelCell
 *        data to match.
 */

void ModelsList::updateCurrentModelCell()
{
  auto mdl = std::find(begin(), end(), currentModel);
  if(mdl != end()) {
    strcpy((*mdl)->modelBitmap,g_model.header.bitmap);
    (*mdl)->setModelName(g_model.header.name);
    (*mdl)->setRfData(&g_model);
  } else {
    TRACE("ModelList Error - Can't find current model");
  }
}

/**
 * @brief Reads a line from a file. Used by loadTxt
 *
 * @param line Storage for the read line
 * @param maxlen maximum read length
 * @return true Success
 * @return false Failure
 */

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

/**
 * @brief Creates a new ModelCell
 *
 * @param name Model Name
 * @param save True Update yaml right away
 * @return ModelCell* New Model
 */

ModelCell * ModelsList::addModel(const char * name, bool save)
{
  if(name == nullptr) return nullptr;
  ModelCell * result = new ModelCell(name);
  strcpy(result->modelFilename, name);
  push_back(result);
  if (save) this->save();
  return result;
}

/**
 * @brief Removes a model from the list and the modelmap.
 * @details The models filename is moved into the sub folder DELETED_MODELS_PATH
 *          rather than deleting the model. If left in current location would
 *          be re-scanned on boot
 *
 * @param model Model to be deleted
 * @return false Successfully removed
 * @return true Failure
 */

bool ModelsList::removeModel(ModelCell * model)
{
  erase(std::remove(begin(), end(), model), end());
  modelsLabels.removeModels(model);

  // Create deleted folder if it doesn't exist
  DIR deletedFolder;
  FRESULT result = f_opendir(&deletedFolder, DELETED_MODELS_PATH);
  if (result != FR_OK) {
    if (result == FR_NO_PATH)
      result = f_mkdir(DELETED_MODELS_PATH);
    if (result != FR_OK) {
      TRACE("Unable to create deleted models folder");
      return true;
    }
  }

  // Move model into deleted folder. If not moved will be re-added on next reboot
  if(!sdCopyFile(model->modelFilename, MODELS_PATH, model->modelFilename, DELETED_MODELS_PATH)) {
    char curFilename[sizeof(MODELS_PATH) + LEN_MODEL_FILENAME + 2] = "";
    strcat(curFilename, MODELS_PATH PATH_SEPARATOR);
    strcat(curFilename, model->modelFilename);
    TRACE("Deleting Model %s", model->modelFilename);

    if(f_unlink(curFilename) != FR_OK) {
      TRACE("Unable to delete file");
      return true;
    }
  }

  return false;
}

/**
 * @brief Moves a model in the list for customization of order
 *
 * @param curindex Index of the model to move
 * @param toindex Destination index of the model
 * @return true Failure
 * @return false Success
 */

bool ModelsList::moveModelTo(unsigned curindex, unsigned toindex)
{
  if(curindex == toindex ||
    curindex >= size() ||
    toindex >= size())
  return true;

  if(curindex < toindex) { // Move forward
    std::rotate(rend() - curindex - 1, rend() - curindex, rend() - toindex);
  } else { // Move back
    std::rotate(begin() + curindex, begin() + curindex + 1, begin() + toindex + 1);
  }

  modelsLabels.setDirty();
  return false;
}

/**
 * @brief Checks if a models RF module has a unique id
 *
 * @param moduleIdx Module number 0 <-> NUM_MODULES
 * @param warn_buf Buffer to store the warning string
 * @param warn_buf_len Length of buffer
 * @return true ID is unique
 * @return false ID is used
 */

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

/**
 * @brief Finds the next free RF Module ID
 *
 * @param moduleIdx RF Module to search (Internal/External) 0 <-> NUM_MODULES
 * @return 0 No unused ID found
 * @return uint8_t Next free ID
 */

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