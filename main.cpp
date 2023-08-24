#include <iostream>
#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <X11/Xlib.h>

#include "inc/mongoose/mongoose.h"
#include "inc/mINI/ini.h"

#define PLOG_OMIT_LOG_DEFINES 
#define PLOG_NO_DBG_OUT_INSTANCE_ID 1

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Appenders/ColorConsoleAppender.h>


using std::string;
using std::vector;

typedef struct {
   SDL_Window* pWindow;
   SDL_Renderer* pRenderer;
   SDL_Texture* pTexture;
   int width;
   int height;
} VPXDisplay;

vector<VPXDisplay> g_Displays;

static void event_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
   if (ev != MG_EV_HTTP_MSG)
      return;
  
   struct mg_http_message *hm = (struct mg_http_message *) ev_data;

   if (!mg_http_match_uri(hm, "/update")) {
      mg_http_reply(c, 200, "", "");
      return;
   }

   char szDisplay[3];
   mg_http_get_var(&hm->query, "display", szDisplay, sizeof(szDisplay));

   int display = atoll(szDisplay);

   if (display < 1 || display > g_Displays.size()) {
      PLOGE.printf("Invalid display: %d", display);

      mg_http_reply(c, 400, "", "Bad request");
      return;
   }
   
   char szImage[1024];
   mg_http_get_var(&hm->query, "image", szImage, sizeof(szImage));

   SDL_Surface* pSurface = IMG_Load(szImage);
   if (!pSurface) {
      PLOGE.printf("Invalid image: %s", szImage);

      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

   VPXDisplay* pVPXDisplay = &g_Displays[display-1];

   if (pVPXDisplay->pTexture) {
      SDL_DestroyTexture(pVPXDisplay->pTexture);
      pVPXDisplay->pTexture = NULL;
   }

   pVPXDisplay->pTexture = SDL_CreateTextureFromSurface(pVPXDisplay->pRenderer, pSurface);
   SDL_FreeSurface(pSurface);

   PLOGI.printf("Display %d image set to %s", display, szImage);

   mg_http_reply(c, 200, "", "display: %d, image: %s", display, szImage);
}

void cleanup()
{
   for (int loop = 0; loop < g_Displays.size(); loop++) {
      if (g_Displays[loop].pTexture)
         SDL_DestroyTexture(g_Displays[loop].pTexture);  
   
      if (g_Displays[loop].pRenderer)
         SDL_DestroyRenderer(g_Displays[loop].pRenderer);
   
      if (g_Displays[loop].pRenderer)
         SDL_DestroyWindow(g_Displays[loop].pWindow);
   }

   SDL_Quit();
}

int main(int argc, char* argv[])
{
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

   PLOGI << "Starting VPXDS...";

   Window focusedWindow;
   Display* pDisplay = XOpenDisplay(NULL);
 
   if (pDisplay) {
      int revertTo;
      XGetInputFocus(pDisplay, &focusedWindow, &revertTo); 

      PLOGI.printf("Currently focused window: %d", focusedWindow);
   }

   string szIniFile = szPath + "vpxds.ini";

   PLOGI.printf("Loading ini at: %s", szIniFile.c_str());

   mINI::INIFile file(szIniFile);
   mINI::INIStructure ini;
   file.read(ini);

   if (!ini.has("settings")) {
      PLOGE << "Invalid settings, exiting";
      return 1;
   }

   int displayCount = 0;

   if (ini["settings"].has("displays"))
      displayCount = atoll(ini["settings"]["displays"].c_str());

   if (displayCount == 0) {
      PLOGE << "Invalid displays, exiting";
      return 1;
   }    

   PLOGI.printf("Displays: %d", displayCount);

   if (SDL_Init(SDL_INIT_VIDEO) != 0) {
     PLOGE << "SDL_Init Error: " << SDL_GetError() << std::endl;
     return 1;
   }

   for (int loop = 0; loop < displayCount; loop++) {
      string szDisplay = "display" + std::to_string(loop + 1);

      string szDisplayX = szDisplay + "_x";
      string szDisplayY = szDisplay + "_y";
      string szDisplayWidth = szDisplay + "_width";
      string szDisplayHeight = szDisplay + "_height";

      if (!ini["settings"].has(szDisplayX) ||
          !ini["settings"].has(szDisplayY) || 
          !ini["settings"].has(szDisplayWidth) || 
          !ini["settings"].has(szDisplayHeight)) {
         PLOGE << "Invalid display, exiting";
         
         cleanup();
         return 1;
      }

      int displayX = atoll(ini["settings"][szDisplayX].c_str());
      int displayY = atoll(ini["settings"][szDisplayY].c_str());
      int displayWidth = atoll(ini["settings"][szDisplayWidth].c_str());
      int displayHeight = atoll(ini["settings"][szDisplayHeight].c_str());

      string szWindowName = "vpxds" + std::to_string(loop + 1);

      PLOGI.printf("Creating window %s, x=%d, y=%d, width=%d, height=%d", szWindowName.c_str(), displayX, displayY, displayWidth, displayHeight);

      SDL_Window* pWindow = SDL_CreateWindow(szWindowName.c_str(), displayX, displayY, displayWidth, displayHeight, SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN);

      if (!pWindow) {
         PLOGE << "SDL_CreateWindow Error: " << SDL_GetError();

         cleanup();
         return 1;
      }

      SDL_Renderer* pRenderer = SDL_CreateRenderer(pWindow, -1, SDL_RENDERER_ACCELERATED);
      if (pRenderer == NULL) {
         PLOGE << "SDL_CreateRenderer Error: " << SDL_GetError();

         cleanup();
         return 1;
      }

      VPXDisplay display;
      display.pRenderer = pRenderer;
      display.pWindow = pWindow;
      display.pTexture = NULL;
      display.width = displayWidth;
      display.height = displayHeight;

      g_Displays.push_back(display);
   }

   if (pDisplay) {
      PLOGI.printf("Returning focus to window: %d", focusedWindow);
      XSetInputFocus(pDisplay, focusedWindow, RevertToParent, CurrentTime);
      
      XCloseDisplay(pDisplay);
   }

   PLOGI << "Starting server started on port 8111";

   struct mg_mgr mgr;
   struct mg_connection* conn;
   mg_mgr_init(&mgr);

   mg_http_listen(&mgr, "http://127.0.0.1:8111", event_handler, NULL);
  
   bool quit = false;

   SDL_Event event;
   while (!quit) {
      while (SDL_PollEvent(&event)) {
         if (event.type == SDL_QUIT)
            quit = true;
      }

      for (int loop = 0; loop < g_Displays.size(); loop++) {
         SDL_SetRenderDrawColor(g_Displays[loop].pRenderer, 0, 0, 0, 255);
         SDL_RenderClear(g_Displays[loop].pRenderer);
         
         if (g_Displays[loop].pTexture) {
            SDL_Rect dest_rect = { 0, 0, 0, 0 };
            SDL_QueryTexture(g_Displays[loop].pTexture, NULL, NULL, &dest_rect.w, &dest_rect.h);
            SDL_RenderCopy(g_Displays[loop].pRenderer, g_Displays[loop].pTexture, NULL, NULL);
            SDL_RenderPresent(g_Displays[loop].pRenderer);
         }
      }

      mg_mgr_poll(&mgr, 200);
   }

   mg_mgr_free(&mgr);

   cleanup();
   return 0;
}
