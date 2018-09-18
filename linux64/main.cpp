#include "plate.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include<opencv2/opencv.hpp>
#include<opencv/highgui.h>
#include <glog/logging.h>
#include "config/config.h"
#include "base64/base64.h"
using namespace cv;
extern int ev_server_start(int port);
void ev_server_start_multhread(int port, int nThread);

int testIdentify() {
  int rc = 0;
  Mat mat = imread("/home/panghao/1.jpg");
  return 0;
  
  std::vector<TH_PlateIDResult> result;
  rc = identify(0, mat.data, mat.cols, mat.rows, 1, &result);
  LOG(INFO) << "rc=" << rc << " result=" << result.size();
  return 0;
}

static void initGlog(const std::string &name) {
  DIR *dir = opendir("log");
  if (!dir) {
    mkdir("log", S_IRWXU);
  } else {
    closedir(dir);
  }
  google::InitGoogleLogging(name.c_str());
  google::SetLogDestination(google::INFO,std::string("log/"+ name + "info").c_str());
  google::SetLogDestination(google::WARNING,std::string("log/"+ name + "warn").c_str());
  google::SetLogDestination(google::GLOG_ERROR,std::string("log/"+ name + "error").c_str());
  FLAGS_logbufsecs = 0;
}

int main() {
  kunyan::Config config("../config.ini");
  initGlog("plate");
  std::string path = config.get("plate", "libdir");
  LOG(INFO) << path;
  char libPath[128] = {0};
  strncpy(libPath, path.c_str(), path.length());
  TH_SetSoPath(libPath);
  std::map<std::string, std::string> paramMap;
  std::set<std::string> paramSet = getSdkParams();
  auto it = paramSet.begin();
  while (it != paramSet.end()) {
    std::string value = config.get("sdk", *it);
    if (!value.empty()) {
      paramMap[*it] = value;
    }
    it++;
  }
  
  if (initSdk(paramMap) != 0) {
    LOG(ERROR) << "sdk init error";
    return -1;
  }
  sleep(5);
  // ev_server_start(10551);
  ev_server_start_multhread(10551, 4);
  while (1) {
    sleep(10);
  }
  return 0;
}