/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include <sys/stat.h>
#include <cstring>
#include <cstdint>
#include "qshJsonParser.h"
#include "qshLog.h"

#ifdef SNS_WEARABLES_TARGET
#define CONFIG_FILE_PATH "/vendor/etc/sensors/hub1/config/sensing_hub_info.json"
#else
#define CONFIG_FILE_PATH "/vendor/etc/sensors/config/sensing_hub_info.json"
#endif

#define MAX_DATA_VALUE_LEN 15
#define COMM_TYPE_QMI    "0"
#define COMM_TYPE_GLINK  "1"

qshJsonParser::qshJsonParser():
  mReadCommAttr(false),
  mIsClientCommGrp(false),
  mSensingHubId(-1),
  mPlatformId(""),
  mClientCommType(""),
  mClientCommValue(-1),
  mSensingHubNameIdMapFilled(false),
  mCommMapFilled(false),
  mIsFileLoaded(false)
{
}

int qshJsonParser::getCommHandleAttrs(int hubID)
{
  if(0 == mSensingHubCommMap.count(hubID))
    return -1;

  return mSensingHubCommMap.at(hubID).second;
}

std::vector<int> qshJsonParser::getHubIdList()
{
  return mHubIds;
}

int qshJsonParser:: getHubId(std::string hubName)
{
  if(0 == mSensingHubNameIdMap.count(hubName))
    return -1;

  return mSensingHubNameIdMap.at(hubName);
}

int qshJsonParser::getWhitespaceLen(char const* str, uint32_t strLen)
{
  uint32_t cidx = 0;
  while(cidx < strLen && 0 != isspace(str[cidx]))
  {
    if('\n' == str[cidx])
    {
      mCurrLineNum++;
    }
    ++cidx;
  }
  return cidx;
}

uint32_t qshJsonParser::parsePair(char* json, uint32_t jsonlen, char** key,
                           char** value)
{
  uint32_t cidx = 0;
  char* endString;
  bool failed = false;

  *value = NULL;

  if(cidx >= jsonlen || '\"' != json[cidx++])
  {
    sns_logd("parsePair: Missing opening quote line no.: %d ", mCurrLineNum);
    failed = true;
  }
  else if(NULL == (endString = strchr(&json[cidx], '\"')))
  {
    sns_logd("parsePair: Missing closing quote (%i)", mCurrLineNum);
    failed = true;
  }
  else
  {
    *key = &json[cidx];
    size_t keyLen = endString - &json[cidx];
    if(cidx + keyLen >= jsonlen)
    {
      sns_logd("parsePair: Key extends beyond buffer");
      failed = true;
    }
    else
    {
      cidx += keyLen;
      json[cidx++] = '\0'; // We want to safely use this string later
      cidx += getWhitespaceLen(&json[cidx], jsonlen - cidx);
      if(cidx >= jsonlen || ':' != json[cidx++])
      {
        sns_logd("parsePair: Missing colon Line No.: %d ", mCurrLineNum);
        failed = true;
      }
      else
      {
        cidx += getWhitespaceLen(&json[cidx], jsonlen - cidx);
        if(cidx < jsonlen && '\"' == json[cidx])
        {
          cidx++;
          *value = &json[cidx];
          endString = strchr(&json[cidx], '\"');
          if(NULL == endString)
          {
            sns_logd("parsePair: Missing closing quote %d ", mCurrLineNum);
            failed = true;
          }
          else
          {
            cidx += (uintptr_t)endString - (uintptr_t)&json[cidx];
            json[cidx++] = '\0';
            cidx += getWhitespaceLen(&json[cidx], jsonlen - cidx);
          }
        }
        else if(cidx < jsonlen && '{' != json[cidx] && '[' != json[cidx])
        {
          sns_logd("parsePair: Next element is not a quote or opening bracket:%d ", mCurrLineNum);
          failed = true;
        }
      }
      sns_logv("Parsed pair key:%s",*key);
    }
  }
  return failed ? 0 : cidx;
}

uint32_t qshJsonParser::parseItem(char* itemName, char* itemValue, char* json, uint32_t jsonlen)
{
  uint32_t cidx = 0;

  if(cidx >= jsonlen || '{' != json[cidx++])
  {
    sns_logd("parseItem: Missing opening bracket Line no. %d ", mCurrLineNum);
  }
  else
  {
    do
    {
      char* key, *value;
      uint32_t len;

      cidx += getWhitespaceLen(&json[cidx], jsonlen - cidx);
      len = parsePair(&json[cidx], jsonlen - cidx, &key, &value);
      cidx += len;

      if(0 == len || NULL == key || NULL == value)
      {
        sns_logd("parseItem: Unable to parse pair Line no. %d ", mCurrLineNum);
        return cidx;
      }
      else
      {
        if(0 == strcmp(key, "data"))
        {
          size_t valueLen = strlen(value);
          if(valueLen >= MAX_DATA_VALUE_LEN) {
            sns_loge("Value too long (%zu), truncating to %d", valueLen, MAX_DATA_VALUE_LEN - 1);
          }
          sns_logd("set item value:%s",value);
          strlcpy(itemValue, value, MAX_DATA_VALUE_LEN);
        }
      }
      cidx += getWhitespaceLen(&json[cidx], jsonlen - cidx);
    } while(cidx < jsonlen && ',' == json[cidx] && cidx++);

    if(cidx >= jsonlen || '}' != json[cidx++])
    {
      sns_logd("parseItem: Missing closing bracket Line no.  %d ", mCurrLineNum);
      cidx = 0;
    }
  }
  return cidx;
}

void qshJsonParser::updateSensingHubInfo(const char * grpName, char * key, char * value)
{
  if(0 == strcmp(grpName, ".client_comm"))
    mIsClientCommGrp = true;

  if(0 == strcmp(key, "platform_id"))
  {
     mPlatformId = value;
  }
  else if(0 == strcmp(key, "sensing_hub_id"))
  {
     mSensingHubId = atoi(value);
     mHubIds.push_back(mSensingHubId);
  }
  else if(0 == strcmp(key, "client_comm_type"))
  {
     mClientCommType = value;
  }
  else if(mIsClientCommGrp && 0 == strcmp(key, "service_id"))
  {
    if(COMM_TYPE_QMI == mClientCommType)
      mClientCommValue = atoi(value);
    mIsClientCommGrp = false;
  }
  else if(mIsClientCommGrp && strcmp(value, "apps") && strcmp(key, "link_name"))
  {
    mReadCommAttr = true;
    mIsClientCommGrp = false;
  }
  else if(0 == strcmp(key, "no_of_channels") && mReadCommAttr)
  {
    if(COMM_TYPE_GLINK ==mClientCommType)
      mClientCommValue = atoi(value);
  }

  if((-1 !=mSensingHubId) && (!mPlatformId.empty()))
  {
    mSensingHubNameIdMap.insert({mPlatformId, mSensingHubId});
    mSensingHubNameIdMapFilled = true;
  }

  if(( -1 != mSensingHubId) && (!mClientCommType.empty()) && (-1 != mClientCommValue ))
  {
    mSensingHubCommMap.insert({mSensingHubId, {(std::string)mClientCommType, mClientCommValue}});
    mCommMapFilled= true;
  }

  if(mSensingHubNameIdMapFilled && mCommMapFilled)
  {
    mSensingHubId = -1;
    mPlatformId.clear();
    mClientCommType.clear();
    mClientCommValue = -1;
    mReadCommAttr = false;
    mSensingHubNameIdMapFilled = false;
    mCommMapFilled= false;
  }
}

int qshJsonParser::parseGroup(char const* grpName, char* json,
                            uint32_t jsonlen, uint32_t* len)
{
  uint32_t cidx = 0;
  bool failed = false;

  *len = 0;

  if(cidx >= jsonlen || '{' != json[cidx++])
  {
    sns_loge("parseGroup: Missing opening bracket Line no. %d ", mCurrLineNum);
    return -1;
  }
  {
    do
    {
      char *key, *value;
      uint32_t len;

      cidx += getWhitespaceLen(&json[cidx], jsonlen - cidx);
      len = parsePair(&json[cidx], jsonlen - cidx, &key, &value);
      cidx += len;

      if(0 == len || NULL == key)
      {
        sns_loge("%s: Unable to parse pair on line No %d ", __func__, mCurrLineNum);
        return -1;
      }
      else if(NULL != value)
      {
        if(0 == strcmp(key, "owner"))
        {
          sns_logd("Owner of group:  %s ", value);
        }
        else
        {
          sns_loge("Owner of the group not found");
        }
      }
      else
      {
        if('.' == key[0])
        {
          if(0 != parseGroup(key, &json[cidx], jsonlen - cidx, &len))
          {
            sns_loge("parseGroup: Error parsing nested group:%s %d ", key, mCurrLineNum);
            return -1;
          }
          cidx += len;
        }
        else
        {
          char localValue[MAX_DATA_VALUE_LEN];
          len = parseItem(key, &localValue[0], &json[cidx], jsonlen - cidx);
          sns_logv("local key:value %s:%s", key, localValue);
          updateSensingHubInfo(grpName, key, localValue);
          cidx += len;
        }
        if(0 == len)
        {
          sns_loge("parseGroup: Error parsing item:%s %d ", key, mCurrLineNum);
          return -1;
        }
      }
      cidx += getWhitespaceLen(&json[cidx], jsonlen - cidx);
    } while(',' == json[cidx] && cidx++);

    if(cidx >= jsonlen || '}' != json[cidx++])
    {
      sns_loge("parseGroup: Missing closing bracket Line No. %d ", mCurrLineNum);
      return -1;
    }
  }
  *len = cidx;
  return 0;
}

int32_t qshJsonParser::parseConfig(char* json, uint32_t jsonlen)
{
  int32_t cidx = 0;

  if(cidx >= jsonlen || '{' != json[cidx++])
  {
    sns_loge("parseConfig: Missing open bracket %d ", mCurrLineNum);
    cidx = 0;
  }
  else
  {
    do
    {
      cidx += getWhitespaceLen(&json[cidx], jsonlen - cidx);
      if('}' == json[cidx])
        break;
    } while(cidx < jsonlen && ',' == json[cidx] && cidx++);
  }

  if(0 < cidx)
  {
    cidx += getWhitespaceLen(&json[cidx], jsonlen - cidx);
  }
  if(0 < cidx && (cidx >= jsonlen || '}' != json[cidx++]))
  {
    sns_loge("parseConfig: Missing closing bracket Line No. %d ", mCurrLineNum);
    cidx = 0;
  }
  return cidx;
}

int qshJsonParser::parseFile(char* json, uint32_t jsonlen)
{
  uint32_t cidx = 0;
  uint32_t len;
  int32_t ret;
  char *key, *value;

  mCurrLineNum = 0;
  sns_logi("%s: started parsing sensing hub info json file", __func__);
  cidx = getWhitespaceLen(json, jsonlen);
  if(cidx >= jsonlen || '{' != json[cidx++])
  {
    sns_loge("Missing open bracket in config file");
    return -1;
  }

  do {
    cidx += getWhitespaceLen(&json[cidx], jsonlen - cidx);
    len = parsePair(&json[cidx], jsonlen - cidx, &key, &value);

    if(0 == len || NULL == key || NULL != value)
    {
        sns_loge("Pair parsing failed");
        return -1;
    }
    cidx += len;
    if(0 == strcmp(key, "config"))
    {
      ret = parseConfig(&json[cidx], jsonlen - cidx);
      if(0 == ret) {
        sns_loge("Error parsing config field");
        return -1;
      }
      cidx += ret;
    }
    else if(0 != parseGroup(key, &json[cidx], jsonlen - cidx, &len))
    {
      sns_loge("Error parsing group: key=%s, %d", key, mCurrLineNum);
      return -1;
    }
    else
    {
      cidx += len;
    }
    sns_logd("Parsing next group");
  } while(cidx < jsonlen && ',' == json[cidx] && cidx++);

  sns_logi("Parsing Complete");
  return 0;
}

/* call this function to start parsing */
int qshJsonParser::loadFile()
{

  if(mIsFileLoaded)
  {
    sns_logi("config file already loaded");
    return 0;
  }

  struct stat buf;
  if(0 != stat(CONFIG_FILE_PATH, &buf))
  {
    sns_loge("stat failed for config file");
    return -1;
  }
  char* fileContent = (char*)malloc(buf.st_size + 1);
  if(nullptr == fileContent)
  {
    sns_loge("failed to allocate memory for reading file content");
    return -1;
  }
  FILE* fptr = NULL;
  fptr = fopen(CONFIG_FILE_PATH, "r");
  if(NULL == fptr)
  {
    sns_loge("failed to open sensing_hub_config file");

    free(fileContent);
    return -1;
  }
  if(fread(fileContent, sizeof(char), buf.st_size, fptr) < buf.st_size)
  {
    sns_loge("failed to read sensing_hub_config");
    free(fileContent);
    fclose(fptr);
    return -1;
  }
  fileContent[buf.st_size] = '\0';
  fclose(fptr);
  sns_logi("parsing the contents of the file");
  if(0 != parseFile(fileContent, buf.st_size))
  {
    free(fileContent);
    return -1;
  }
  free(fileContent);
  mIsFileLoaded = true;
  return 0;
}
