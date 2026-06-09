/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#pragma once

#include <unordered_map>
#include <vector>
#include <string>

class qshJsonParser
{
public:
  qshJsonParser();
  ~qshJsonParser(){};

  /**
   * @brief get handle to qshJsonParser get_instance
   * @return parser_instance&
   */
  static qshJsonParser& getInstance()
  {
      static qshJsonParser parserInstance;
      return parserInstance;
  }

  /* reads sensing_hub_config file & starts parsing */
  int loadFile(const char* path);
  /* returns list of supported hub ids */
  std::vector<int> getHubIdList();
  /* gets the hub id for a given hub name */
  int getHubId(std::string hubName);
  /* fetches the comm attributes for a hub id */
  int getCommHandleAttrs(int hubID);
  /* fetches the comm type for a hub id */
  int getCommType(int hubID);
  /* fetches the hub name for a hub id */
  std::string getHubName(int hubID);
  /* returns if parser state has been updated from config file */
  bool isFileParsed();

private:
  int mDeviceSocId;
  std::vector<int> mJsonSocIdList;
  int mJsonSocId;
  int readSocId();
  void resetParserState();

  std::unordered_map<std::string, int> mSensingHubNameIdMap;
  std::unordered_map<int, std::pair<std::string, int>> mSensingHubCommMap;
  std::unordered_map<int, std::string> mSensingHubIdNameMap;
  int mCurrLineNum;
  std::vector<int> mHubIds;

  bool mReadCommAttr;
  bool mIsClientCommGrp;

  int mSensingHubId;
  std::string mPlatformId;
  std::string mClientCommType;
  int mClientCommValue;

  bool mSensingHubNameIdMapFilled;
  bool mCommMapFilled;
  bool mIsFileParsed;

  int getWhitespaceLen(char const* str, uint32_t strLen);

  uint32_t parsePair(char* json, uint32_t jsonLen, char** key,
                                                      char** value);

  uint32_t parseItem(char* itemName, char* itemValue, char* json, uint32_t jsonLen);

  void updateSensingHubInfo(const char* grpName, char* key, char* value);

  int parseGroup(char const* grpName, char* json,
                          uint32_t jsonLen, uint32_t* len);

  int32_t parseConfig(char* json, uint32_t jsonLen);

  int parseConfigItem(char* json, uint32_t jsonLen);
  int parseFile(char* json, uint32_t jsonLen);
};
