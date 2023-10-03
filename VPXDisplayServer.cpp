#include "VPXDisplayServer.h"

#include <plog/Log.h>
#include "inc/mongoose/mongoose.h"
#include "inc/mINI/ini.h"
#include "inc/tinyxml2/tinyxml2.h"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <X11/Xlib.h>


void VPXDisplayServer::EventHandler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
   if (ev != MG_EV_HTTP_MSG)
      return;

   VPXDisplayServer* pServer = static_cast<VPXDisplayServer*>(fn_data);

   struct mg_http_message *hm = (struct mg_http_message *) ev_data;

   if (mg_http_match_uri(hm, "/update"))
      pServer->UpdateRequest(c, ev_data);
   else if (mg_http_match_uri(hm, "/b2s"))
      pServer->B2SRequest(c, ev_data);
   else {
      mg_http_reply(c, 200, "", "");
      return;
   }
}

VPXDisplayServer::VPXDisplayServer()
{
   m_pBackglassDisplay = NULL;
   m_pDMDDisplay = NULL;
}

VPXDisplayServer::~VPXDisplayServer()
{
   if (m_pBackglassDisplay) {
      CloseDisplay(m_pBackglassDisplay);
      delete m_pBackglassDisplay;
   }

   if (m_pDMDDisplay) {
      CloseDisplay(m_pDMDDisplay);
      delete m_pDMDDisplay;
   }

   SDL_Quit();
}

void VPXDisplayServer::UpdateRequest(struct mg_connection *c, void *ev_data)
{
   struct mg_http_message *hm = (struct mg_http_message *) ev_data;

   char szDisplay[10];
   mg_http_get_var(&hm->query, "display", szDisplay, sizeof(szDisplay));

   VPXDisplay* pDisplay = NULL;

   if (!strncasecmp("backglass", szDisplay, 9))
      pDisplay = m_pBackglassDisplay;
   else if (!strncasecmp("dmd", szDisplay, 3))
      pDisplay = m_pDMDDisplay;

   if (!pDisplay) {
      PLOGE.printf("Invalid display: %s", szDisplay);
      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

   if (pDisplay->pTexture) {
      SDL_DestroyTexture(pDisplay->pTexture);
      pDisplay->pTexture = NULL;
   }

   char szImage[1024];
   mg_http_get_var(&hm->query, "image", szImage, sizeof(szImage));

   SDL_Surface* pSurface = IMG_Load(szImage);
   if (!pSurface) {
      PLOGE.printf("Invalid image: %s", szImage);
      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

   pDisplay->pTexture = SDL_CreateTextureFromSurface(pDisplay->pRenderer, pSurface);
   SDL_FreeSurface(pSurface);

   PLOGI.printf("Display %s image set to %s", szDisplay, szImage);
   mg_http_reply(c, 200, "", "display: %s, image: %s", szDisplay, szImage);
}

void VPXDisplayServer::B2SRequest(struct mg_connection *c, void *ev_data)
{
   if (!m_pBackglassDisplay) {
      mg_http_reply(c, 400, "", "No backglass display");
      return;
   }

   if (m_pBackglassDisplay->pTexture) {
      SDL_DestroyTexture(m_pBackglassDisplay->pTexture);
      m_pBackglassDisplay->pTexture = NULL;
   }

   struct mg_http_message *hm = (struct mg_http_message *) ev_data;

   char szVpx[1024];
   mg_http_get_var(&hm->query, "vpx", szVpx, sizeof(szVpx));

   if (!std::filesystem::exists(szVpx)) {
      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

   SDL_Surface* pImage = NULL;

   std::filesystem::path path(szVpx);
   path.replace_extension(string(".png"));

   string szCacheImagePath = m_szCachePath + path.filename().string();
  
   if (!std::filesystem::exists(szCacheImagePath)) {
      pImage = GetB2SImage(string(szVpx));
      if (pImage) {
         IMG_SavePNG(pImage, szCacheImagePath.c_str());
         PLOGI.printf("Backglass image saved to %s", szCacheImagePath.c_str());
      }
   }
   else {
      pImage = IMG_Load(szCacheImagePath.c_str());
      if (pImage) {
         PLOGI.printf("Cached backglass image found at %s", szCacheImagePath.c_str());
      }
   }

   if (!pImage) {
      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

   m_pBackglassDisplay->pTexture = SDL_CreateTextureFromSurface(m_pBackglassDisplay->pRenderer, pImage);
   SDL_FreeSurface(pImage);

   mg_http_reply(c, 200, "", "");
}

int VPXDisplayServer::OpenDisplay(const string& szName, VPXDisplay* pDisplay)
{
   PLOGI.printf("Creating %s window, x=%d, y=%d, width=%d, height=%d", szName.c_str(),
      pDisplay->x, pDisplay->y, pDisplay->width, pDisplay->height);

   pDisplay->pWindow = SDL_CreateWindow(szName.c_str(),  pDisplay->x, pDisplay->y, pDisplay->width, pDisplay->height, 
      SDL_WINDOW_SKIP_TASKBAR | SDL_WINDOW_BORDERLESS | SDL_WINDOW_UTILITY | SDL_WINDOW_SHOWN);
   if (!pDisplay->pWindow) {
      PLOGE << "SDL_CreateWindow Error: " << SDL_GetError();
      return 1;
   }

   pDisplay->pRenderer = SDL_CreateRenderer(pDisplay->pWindow, -1, SDL_RENDERER_ACCELERATED);
   if (!pDisplay->pRenderer) {
     PLOGE << "SDL_CreateRenderer Error: " << SDL_GetError();
     return 1;
   }

   return 0;
}

void VPXDisplayServer::CloseDisplay(VPXDisplay* pDisplay)
{
   if (pDisplay->pTexture) {
      SDL_DestroyTexture(pDisplay->pTexture);
      pDisplay->pTexture = NULL;
   }
   if (pDisplay->pRenderer) {
      SDL_DestroyRenderer(pDisplay->pRenderer);
      pDisplay->pRenderer = NULL;
   }
   if (pDisplay->pWindow) {
      SDL_DestroyWindow(pDisplay->pWindow);
      pDisplay->pWindow = NULL;
   }
}

void VPXDisplayServer::RenderDisplay(VPXDisplay* pDisplay)
{
   if (!pDisplay)
      return;

   SDL_SetRenderDrawColor(pDisplay->pRenderer, 0, 0, 0, 255);
   SDL_RenderClear(pDisplay->pRenderer);
    
   if (pDisplay->pTexture)
      SDL_RenderCopy(pDisplay->pRenderer, pDisplay->pTexture, NULL, NULL);

   SDL_RenderPresent(pDisplay->pRenderer);
}

void VPXDisplayServer::LoadINI()
{
   string szIniFile = m_szBasePath + "vpxds.ini";

   PLOGI.printf("Loading ini at: %s", szIniFile.c_str());

   mINI::INIFile file(szIniFile);
   mINI::INIStructure ini;
   file.read(ini);

   if (!ini.has("Settings")) {
      PLOGE << "Missing settings entry";
      return;
   }

   if (ini["Settings"].has("CachePath")) {
      m_szCachePath = ini["settings"]["CachePath"];

      if (!m_szCachePath.ends_with("/"))
         m_szCachePath += "/";

      PLOGI.printf("Cache path set to: %s", m_szCachePath.c_str());
   }

   if (ini["Settings"].has("Backglass")) {
      if (atoll(ini["settings"]["Backglass"].c_str()) == 1) {
         if (ini["Settings"].has("BackglassX") ||
             ini["Settings"].has("BackglassY") || 
             ini["Settings"].has("BackglassWidth") || 
             ini["Settings"].has("BackglassHeight")) {
            m_pBackglassDisplay = new VPXDisplay();
            memset(m_pBackglassDisplay, 0, sizeof(VPXDisplay));

            m_pBackglassDisplay->x = atoll(ini["Settings"]["BackglassX"].c_str());
            m_pBackglassDisplay->y = atoll(ini["Settings"]["BackglassY"].c_str());
            m_pBackglassDisplay->width = atoll(ini["Settings"]["BackglassWidth"].c_str());
            m_pBackglassDisplay->height = atoll(ini["Settings"]["BackglassHeight"].c_str());
         }
      }
   }

   if (ini["Settings"].has("DMD")) {
      if (atoll(ini["settings"]["DMD"].c_str()) == 1) {
         if (ini["Settings"].has("DMDX") ||
             ini["Settings"].has("DMDY") || 
             ini["Settings"].has("DMDWidth") || 
             ini["Settings"].has("DMDHeight")) {
            m_pDMDDisplay = new VPXDisplay();
            memset(m_pDMDDisplay, 0, sizeof(VPXDisplay));

            m_pDMDDisplay->x = atoll(ini["Settings"]["DMDX"].c_str());
            m_pDMDDisplay->y = atoll(ini["Settings"]["DMDY"].c_str());
            m_pDMDDisplay->width = atoll(ini["Settings"]["DMDWidth"].c_str());
            m_pDMDDisplay->height = atoll(ini["Settings"]["DMDHeight"].c_str());
         }
      }
   }
}

int VPXDisplayServer::Start()
{
   PLOGI << "Starting VPXDS...";

   LoadINI();

   Window focusedWindow;
   Display* pXDisplay = XOpenDisplay(NULL);
 
   if (pXDisplay) {
      int revertTo;
      XGetInputFocus(pXDisplay, &focusedWindow, &revertTo); 

      PLOGI.printf("Currently focused window: %d", focusedWindow);
   }

   if (m_pBackglassDisplay) { 
      if (OpenDisplay("vpxds_backglass", m_pBackglassDisplay)) {
         CloseDisplay(m_pBackglassDisplay);
         delete m_pBackglassDisplay;
         m_pBackglassDisplay = NULL;
      }
   }

   if (m_pDMDDisplay) { 
      if (OpenDisplay("vpxds_dmd", m_pDMDDisplay)) {
         CloseDisplay(m_pDMDDisplay);
         delete m_pDMDDisplay;
         m_pDMDDisplay = NULL;
      }
   }

   if (pXDisplay) {
      PLOGI.printf("Returning focus to window: %d", focusedWindow);
      XSetInputFocus(pXDisplay, focusedWindow, RevertToParent, CurrentTime);
      
      XCloseDisplay(pXDisplay);
   }

   PLOGI << "Starting server started on port 8111";

   struct mg_mgr mgr;
   struct mg_connection* conn;
   mg_mgr_init(&mgr);

   mg_http_listen(&mgr, "http://0.0.0.0:8111", &VPXDisplayServer::EventHandler, this);
  
   bool quit = false;

   SDL_Event event;
   while (!quit) {
      while (SDL_PollEvent(&event)) {
         if (event.type == SDL_QUIT)
            quit = true;
      }

      RenderDisplay(m_pBackglassDisplay);
      RenderDisplay(m_pDMDDisplay);

      mg_mgr_poll(&mgr, 200);
   }

   mg_mgr_free(&mgr);

   return 0;
}

SDL_Surface* VPXDisplayServer::GetB2SImage(const string& szVpx)
{
   SDL_Surface* pImage = NULL;

   std::filesystem::path path(szVpx);
   path.replace_extension(string("directb2s"));

   std::ifstream infile(path.string());
   if (!infile.good())
      return pImage;

   char* data = nullptr;

   try {
      tinyxml2::XMLDocument b2sTree;
      std::stringstream buffer;
      std::ifstream myFile(path.string());
      buffer << myFile.rdbuf();
      myFile.close();

      auto xml = buffer.str();
      if (b2sTree.Parse(xml.c_str(), xml.size())) {
         PLOGE.printf("Failed to parse directb2s file: %s", path.string().c_str());
         return pImage;
      }
      
      if (!b2sTree.FirstChildElement("DirectB2SData")) {
         PLOGE.printf("Invalid directb2s file: %s", path.string().c_str());
         return pImage;
      }

      auto topnode = b2sTree.FirstChildElement("DirectB2SData");

      int backglassGrillHeight = std::max(topnode->FirstChildElement("GrillHeight")->IntAttribute("Value"), 0);

      if (topnode->FirstChildElement("Images")) {
         if (topnode->FirstChildElement("Images")->FirstChildElement("BackglassOffImage")) {
            pImage = Base64ToImage(topnode->FirstChildElement("Images")->FirstChildElement("BackglassOffImage")->Attribute("Value"));
            auto onimagenode = topnode->FirstChildElement("Images")->FirstChildElement("BackglassOnImage");
            if (onimagenode) {
               if (pImage)
                  SDL_FreeSurface(pImage);
               pImage = Base64ToImage(onimagenode->Attribute("Value"));
            }
         }
         else {
            if (topnode->FirstChildElement("Images")->FirstChildElement("BackglassImage"))
               pImage = Base64ToImage(topnode->FirstChildElement("Images")->FirstChildElement("BackglassImage")->Attribute("Value"));
         }

         if (pImage && backglassGrillHeight > 0) {
            SDL_Surface* pResizedImage = ResizeImage(pImage, backglassGrillHeight);
            if (pResizedImage) {
                SDL_FreeSurface(pImage);
                pImage = pResizedImage;
            }
         }
      }
   }

   catch (...) {
   }

   return pImage;
}

SDL_Surface* VPXDisplayServer::Base64ToImage(const string& image)
{
   vector<unsigned char> imageData = Base64Decode(image);
   SDL_RWops* rwops = SDL_RWFromConstMem(imageData.data(), imageData.size());
   SDL_Surface* pImage = IMG_Load_RW(rwops, 0);
   SDL_RWclose(rwops);

   return pImage;
}

vector<unsigned char> VPXDisplayServer::Base64Decode(const string &encoded_string)
{
   static const string base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

   string input = encoded_string;
   input.erase(std::remove(input.begin(), input.end(), '\r'), input.end());
   input.erase(std::remove(input.begin(), input.end(), '\n'), input.end());

   int in_len = input.size();
   int i = 0, j = 0, in_ = 0;
   unsigned char char_array_4[4], char_array_3[3];
   vector<unsigned char> ret;

   while (in_len-- && (input[in_] != '=') && (std::isalnum(input[in_]) || (input[in_] == '+') || (input[in_] == '/'))) {
      char_array_4[i++] = input[in_];
      in_++;
      if (i == 4) {
         for (i = 0; i < 4; i++)
            char_array_4[i] = base64_chars.find(char_array_4[i]);

         char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
         char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
         char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

         for (i = 0; i < 3; i++)
            ret.push_back(char_array_3[i]);
         i = 0;
     }
   }

   if (i) {
      for (j = i; j < 4; j++)
         char_array_4[j] = 0;

      for (j = 0; j < 4; j++)
         char_array_4[j] = base64_chars.find(char_array_4[j]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
   }

   return ret;
}

SDL_Surface* VPXDisplayServer::ResizeImage(SDL_Surface* pSourceImage, int grillheight)
{
   SDL_Surface* pImageWithoutGrill = SDL_CreateRGBSurface(0, pSourceImage->w, pSourceImage->h - grillheight, pSourceImage->format->BitsPerPixel,
      pSourceImage->format->Rmask, pSourceImage->format->Gmask, pSourceImage->format->Bmask, pSourceImage->format->Amask);

   if (!pImageWithoutGrill)
      return NULL;

   SDL_BlitSurface(pSourceImage, NULL, pImageWithoutGrill, NULL);

   return pImageWithoutGrill;
}
