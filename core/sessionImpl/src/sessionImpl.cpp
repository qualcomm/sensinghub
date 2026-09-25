/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
 
#include <vector>
#include <string>
#include <sys/stat.h>
#include <glob.h>

#include "SessionFactory.h"
#include "qmiSession.h"
#include "glinkSession.h"
#include "qshJsonParser.h"

using namespace ::com::quic::sensinghub::session::V1_0::implementation;
using namespace std;

#define SENSING_HUB_1   1
#define SENSING_HUB_2   2

#define COMM_TYPE_QMI    0
#define COMM_TYPE_GLINK  1

vector<std::string> sensors_config_paths = {
    "/vendor/etc/sensors/hub1/config/",
    "/vendor/etc/sensors/config/",
    "/lib/firmware/qcom/shikra/sensors/config/"
};

static string sensors_config_path;

extern "C"
{
  ISession* getSession(int hub_id);
  void* getSensingHubIds();
}

string getConfigFilesPath() {

  struct stat buf;
  if (sensors_config_path.empty()) {
    for (auto path : sensors_config_paths) {
      if(0 == stat(path.c_str(), &buf) && S_ISDIR(buf.st_mode)) {
        sensors_config_path = path;
        break;
      }
    }
  }
  return sensors_config_path;
}

static vector<string> getConfigFiles() {
  vector<string> files;
  glob_t glob_result{};
  string file_path;
  file_path = getConfigFilesPath() + "*sensing_hub_info.json";
  if (glob(file_path.c_str(), GLOB_NOSORT, NULL, &glob_result) == 0) {
    for (size_t i = 0; i < glob_result.gl_pathc; ++i)
        files.emplace_back(glob_result.gl_pathv[i]);
  }
  globfree(&glob_result);
  return files;
}

int getCommType(int hub_id)
{
  auto& parser = qshJsonParser::getInstance();
  bool has_config = false;

  if(parser.isFileParsed()){
    std::vector<int>* vec = new std::vector<int>(parser.getHubIdList());
    if (!vec->empty())
    	has_config = true;
  }else{
    auto files = getConfigFiles();
    for (const auto& file : files) {
      struct stat buf;
      if (stat(file.c_str(), &buf) != 0)
        continue;
      if (parser.loadFile(file.c_str()) != -1) {
        has_config = true;
        break;
      }
    }
  }
  if (!has_config)
    return COMM_TYPE_QMI;

  if (hub_id == -1)
    hub_id = SENSING_HUB_1;

  if(parser.getCommType(hub_id) == -1)
    return COMM_TYPE_QMI;
  return parser.getCommType(hub_id);
}

ISession* getSession(int hub_id)
{
  try {
    switch (getCommType(hub_id)) {
      case COMM_TYPE_QMI:
        sns_logi("Creating QMI session");
        return new qmiSession();

      case COMM_TYPE_GLINK: {
        if (hub_id == -1)
          hub_id = SENSING_HUB_1;

        auto& parser = qshJsonParser::getInstance();
        string hub_name = parser.getHubName(hub_id);

        if (hub_name.empty()) {
          sns_loge("Invalid hub id %d", hub_id);
          return nullptr;
        }

        sns_logi("Creating GLINK session");
        return new glinkSession(
        parser.getCommHandleAttrs(hub_id), hub_id, hub_name);
      }

      default:
        sns_loge("Unsupported hub id %d", hub_id);
        return nullptr;
    }
  }
  catch (const exception& e) {
    sns_loge("Session creation failed: %s", e.what());
    return nullptr;
  }
}

void* getSensingHubIds() {
  auto& parser = qshJsonParser::getInstance();

  if(parser.isFileParsed()){
    std::vector<int>* vec = new std::vector<int>(parser.getHubIdList());
    if (vec->empty())
      vec->push_back(SENSING_HUB_1);
    return static_cast<void*>(vec);
  }

  vector<int>* hub_list = new vector<int>();
  auto files = getConfigFiles();

  for (const auto& file : files) {
    struct stat buf;
    if (stat(file.c_str(), &buf) != 0)
      continue;

    if (parser.loadFile(file.c_str()) != -1) {
      auto hub_ids = parser.getHubIdList();
      for (auto id : hub_ids) {
        sns_logi("Hub id %d (comm %d)", id, getCommType(id));
        hub_list->push_back(id);
      }
      break;
    }
  }

  if (hub_list->empty())
    hub_list->push_back(SENSING_HUB_1);
  return hub_list;
}
