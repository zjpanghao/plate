#include "plate.h"
#include <vector>
#include <glog/logging.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <set>

static const char *sdkParams[] = {
  "locateTreshHold",
  "ocrThreshHold",
  "minPlateWidth",
  "maxPlateWidth",
  "maxImageWidth",
  "maxImageHeight",
  "twoYellowOn",
  "lean",
  "shadow"
};


std::set<std::string> getSdkParams() {
  std::set<std::string> paramSet;
  for (int i = 0; i < sizeof(sdkParams) / sizeof(char*);i++) {
    paramSet.insert(std::string(sdkParams[i]));
  }
  return paramSet;
}

static std::vector<TH_PlateIDCfg*>& getPlateCfg() {
  static std::vector<TH_PlateIDCfg*> cfg;
  return cfg;
}

// static unsigned char mem2[40000000];			// 100M

#define CHECK_GET_FROM_MAP_VALUE(it, m, s, d) \
  (it = m.find(s), it == m.end() ? d : it->second)

#define CHECK_GET_INT_FROM_MAP_VALUE(it, m, s, d) \
  (it = m.find(s), it == m.end() ? d : atoi(it->second.c_str()))

#define CHECK_GET_BOOL_FROM_MAP_VALUE(it, m, s, d) \
  (it = m.find(s), it == m.end() ? d : "true" == it->second ? true : false)

#define DEFAULT_MIN_PLATE_WIDTH  60
#define DEFAULT_MAX_PLATE_WIDTH  400
#define DEFAULT_MAX_IMAGE_WIDTH  2560
#define DEFAULT_MAX_IMAGE_HEIGHT  1200
#define DEFAULT_LOCATE_PLATE_THRESH_HOLD 5
#define DEFAULT_OCR_THRESH_HOLD 2

int initSdk(std::map<std::string, std::string> params) {
  int rc = 0;
  for (int channel = 0; channel < MAX_CHANNEL; channel++) {
    unsigned char *mem2 = (unsigned char*)malloc(40000000);
    unsigned char *mem1 = (unsigned char*)malloc(16*1024);	
    auto &vec = getPlateCfg();
    TH_PlateIDCfg *pConfig  = new TH_PlateIDCfg();
    TH_PlateIDCfg &c_Config = *pConfig;
    auto it = params.begin();
  	c_Config.nMinPlateWidth = 
      CHECK_GET_INT_FROM_MAP_VALUE(it, params, "minPlateWidth", DEFAULT_MIN_PLATE_WIDTH);
  	c_Config.nMaxPlateWidth = 
      CHECK_GET_INT_FROM_MAP_VALUE(it, params, "maxPlateWidth", DEFAULT_MAX_PLATE_WIDTH);
  	c_Config.bMovingImage = 0;
  	c_Config.bOutputSingleFrame = 0;
  	c_Config.nMaxImageWidth = 
      CHECK_GET_INT_FROM_MAP_VALUE(it, params, "maxImageWidth", DEFAULT_MAX_IMAGE_WIDTH);
  	c_Config.nMaxImageHeight = 
      CHECK_GET_INT_FROM_MAP_VALUE(it, params, "maxImageHeight", DEFAULT_MAX_IMAGE_HEIGHT);
  	c_Config.pMemory = mem2;
  	c_Config.nMemorySize= 40000000;
  	c_Config.bIsFieldImage = 0;
  	c_Config.bUTF8 = 1;
    c_Config.pFastMemory=mem1;
    c_Config.nFastMemorySize=16*1024;
  
    if ((rc = TH_InitPlateIDSDK(&c_Config)) != 0) {
      LOG(ERROR) << "error init sdk " << rc;
      return -1;
    }
    int plateTh = 
    CHECK_GET_INT_FROM_MAP_VALUE(it, params, "locateTreshHold", DEFAULT_LOCATE_PLATE_THRESH_HOLD);
    int ocrTh = 
    CHECK_GET_INT_FROM_MAP_VALUE(it, params, "ocrThreshHold", DEFAULT_OCR_THRESH_HOLD);
    TH_SetRecogThreshold(plateTh, ocrTh, &c_Config);
    bool enable = CHECK_GET_BOOL_FROM_MAP_VALUE(it, params, "twoYellowOn", false);
    if (enable) {
      TH_SetEnabledPlateFormat(PARAM_TWOROWYELLOW_ON, &c_Config);
    }
    TH_SetImageFormat(ImageFormatRGB, 0, 0, &c_Config);
   #if 1
    enable = CHECK_GET_BOOL_FROM_MAP_VALUE(it, params, "lean", true);
    TH_SetEnableLeanCorrection(enable, &c_Config);
    enable = CHECK_GET_BOOL_FROM_MAP_VALUE(it, params, "shadow", true);
    TH_SetEnableShadow(enable, &c_Config);
   #endif 
    // char order[10] = "»¦";
    //TH_SetProvinceOrder(order, &c_Config);
    vec.push_back(pConfig);
  }
  return rc;
}

int identify(int channel, 
                const unsigned char *data, 
                int width, 
                int height, 
                int maxNum, 
                std::vector<TH_PlateIDResult> *result) {
  if (maxNum > 100) {
    return -1;
  }
  std::vector<TH_PlateIDResult> resVec(100, {0});
  TH_RECT rect = {width/16, height/9, width*7/8, height*7/9};
  int rc = TH_RecogImage(data, width, height, &resVec[0], &maxNum, &rect, getPlateCfg()[channel]);
  if (rc != 0) {
    return rc;
  } 
  resVec.resize(maxNum);
  result->swap(resVec);
  return 0;
}


