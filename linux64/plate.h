#ifndef INCLUDE_PLATE_H
#define INCLUDE_PLATE_H
#include "TH_PlateID.h"
#include <map>
#include <vector>
#include <set>
#include <string>

#define MAX_CHANNEL 4

std::set<std::string> getSdkParams();

int initSdk(std::map<std::string, std::string> params);
int identify(int channel, 
                const unsigned char *data, 
                int width, 
                int height, 
                int maxNum, 
                std::vector<TH_PlateIDResult> *result); 
#endif
