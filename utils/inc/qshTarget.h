/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#pragma once

#include <string>
#include <sys/stat.h>
#include <dirent.h>

/*Class to detect sensors target DSP*/
class qshTarget {
public:
  typedef enum {
    DSP_TYPE_UNKNOWN,
    DSP_TYPE_ADSP,
  } dspType;

  static dspType getDspType() {
    static bool isProbed = false;
    static dspType dsp = DSP_TYPE_UNKNOWN;
    DIR *remoteProcDir = nullptr;
    if (isProbed) {
      return dsp;
    } else {
      remoteProcDir = opendir("/sys/class/remoteproc");
      if(nullptr == remoteProcDir) {
        return dsp;
      }

      struct dirent* dirEntry;
      while((dirEntry = readdir(remoteProcDir))){
        if(0 == strcmp(dirEntry->d_name, ".") ||
           0 == strcmp(dirEntry->d_name, "..")) {
          continue;
        }

        std::string remoteprocName = "" ;
        remoteprocName = remoteprocName + "/sys/class/remoteproc/" + dirEntry->d_name + "/name";
        FILE *firmwareNamePtr = fopen(remoteprocName.c_str(), "r");
        if(nullptr == firmwareNamePtr) {
          continue;
        }
        fseek(firmwareNamePtr, 0 , SEEK_END);
        int firmwareName_size = ftell(firmwareNamePtr);
        fseek(firmwareNamePtr , 0 , SEEK_SET);

        std::string firmwareName = "";
        firmwareName.resize(firmwareName_size);

        fread(&firmwareName[0], 1, firmwareName_size,  firmwareNamePtr);
        if(firmwareNamePtr) {
          fclose(firmwareNamePtr);
        }

        int pos = -1;
        pos = firmwareName.find("adsp", 0);
        if(-1 != pos) {
          dsp = DSP_TYPE_ADSP;
          isProbed = true;
          break;
        }
      }

      if(remoteProcDir) {
        closedir(remoteProcDir);
      }

      return dsp;
    }
  }

  static bool isAdsp() {
    if (DSP_TYPE_ADSP == getDspType())
      return true;
    else
      return false;
  }

  static std::string getSSRPath() {
    std::string ssr_path ="";
    dspType dsp = getDspType();
    if (DSP_TYPE_ADSP == dsp) {
      return mAdspSysNode + "ssr";
    }
    return "";
  }

private:
static inline const std::string mSlpiSysNode = "/sys/kernel/boot_slpi/";
static inline const std::string mAdspSysNode = "/sys/kernel/boot_adsp/";
};
