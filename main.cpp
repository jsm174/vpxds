#include <SDL2/SDL.h>

#define PLOG_OMIT_LOG_DEFINES 
#define PLOG_NO_DBG_OUT_INSTANCE_ID 1

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include "VPXDisplayServer.h"

int main(int argc, char* argv[]) {
   plog::init<PLOG_DEFAULT_INSTANCE_ID>();
   plog::Logger<PLOG_DEFAULT_INSTANCE_ID>::getInstance()->setMaxSeverity(plog::info);

   char* szBasePath = SDL_GetBasePath();
   string szPath = szBasePath;
   SDL_free(szBasePath);

   string szLogPath = szPath + "vpxds.log";

   static plog::RollingFileAppender<plog::TxtFormatter> fileAppender(szLogPath.c_str(), 1024 * 1024 * 5, 1);
   plog::Logger<PLOG_DEFAULT_INSTANCE_ID>::getInstance()->addAppender(&fileAppender);

   static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
   plog::Logger<PLOG_DEFAULT_INSTANCE_ID>::getInstance()->addAppender(&consoleAppender);

   VPXDisplayServer* pServer = new VPXDisplayServer();
   pServer->SetBasePath(szPath);
 
   int res = pServer->Start();

   delete pServer;

   return res;
}