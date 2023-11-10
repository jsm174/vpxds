#include "VPXDisplayServer.h"

#include <plog/Log.h>

#include "inc/mongoose/mongoose.h"
#include "inc/mINI/ini.h"
#include "inc/subprocess/subprocess.h"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <X11/Xlib.h>

static VPXDisplayServer* g_pServer = NULL;

void VPXDisplayServer::Forward(struct mg_http_message *hm, struct mg_connection *c)
{
   size_t i, max = sizeof(hm->headers) / sizeof(hm->headers[0]);
   struct mg_str host = mg_url_host(g_pServer->m_szESUrl.c_str());

   mg_printf(c, "%.*s ", (int) (hm->method.len), hm->method.ptr);

   if (hm->uri.len > 0)
      mg_printf(c, "%.*s ", (int) (hm->uri.len - 3), hm->uri.ptr + 3);

   mg_printf(c, "%.*s\r\n", (int) (hm->proto.len), hm->proto.ptr);

   for (i = 0; i < max && hm->headers[i].name.len > 0; i++) {
     struct mg_str *k = &hm->headers[i].name, *v = &hm->headers[i].value;
     if (mg_strcmp(*k, mg_str("Host")) == 0)
        v = &host;

     mg_printf(c, "%.*s: %.*s\r\n", (int) k->len, k->ptr, (int) v->len, v->ptr);
   }
   mg_send(c, "\r\n", 2);
   mg_send(c, hm->body.ptr, hm->body.len);
}

void VPXDisplayServer::HandleEvent2(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  struct mg_connection *c2 = (struct mg_connection *) fn_data;
  if (ev == MG_EV_READ) {
    if (c2 != NULL)
       mg_send(c2, c->recv.buf, c->recv.len);
    mg_iobuf_del(&c->recv, 0, c->recv.len);
  }
  else if (ev == MG_EV_CLOSE) {
    if (c2 != NULL)
       c2->fn_data = NULL;
  }
  (void) ev_data;
}

void VPXDisplayServer::HandleEvent(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  struct mg_connection *c2 = (struct mg_connection *)fn_data;
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    if (mg_http_match_uri(hm, "/update"))
       g_pServer->Update(c, ev_data);
    else if (mg_http_match_uri(hm, "/reset"))
       g_pServer->Reset(c, ev_data);
    else if (mg_http_match_uri(hm, "/capture"))
       g_pServer->Capture(c, ev_data);
    else if (mg_http_match_uri(hm, "/capture-es"))
       g_pServer->CaptureES(c, ev_data);
    else if (!strncmp(hm->uri.ptr, "/es", 3)) {
       c2 = mg_connect(c->mgr, g_pServer->m_szESUrl.c_str(), VPXDisplayServer::HandleEvent2, c);
       if (c2) {
          c->fn_data = c2;
          Forward(hm, c2);
          c->is_resp = 0;
          c2->is_hexdumping = 0;
       }
       else {
          mg_error(c, "Cannot create backend connection");
       }
    }
    else {
       string szHtmlFile = g_pServer->m_szBasePath + "assets/vpxds.html";
       struct mg_http_serve_opts opts = {};
       mg_http_serve_file(c, hm, szHtmlFile.c_str(), &opts);
    }

    return;
  }
  else if (ev == MG_EV_CLOSE) {
    if (c2 != NULL) {
       c2->is_closing = 1;
       c2->fn_data = NULL;
    }
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

void VPXDisplayServer::Update(struct mg_connection *c, void *ev_data)
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

   char szPath[1024];
   mg_http_get_var(&hm->query, "path", szPath, sizeof(szPath));

   if (*szPath == '\0') {
      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

   std::filesystem::path path = string(szPath);
   string szImage = m_szCachePath + path.stem().string() + "-" + szDisplay + ".png";

   if (pDisplay->pTexture) {
      SDL_DestroyTexture(pDisplay->pTexture);
      pDisplay->pTexture = NULL;
   }

   SDL_Surface* pSurface = IMG_Load(szImage.c_str());
   if (!pSurface) {
      PLOGE.printf("Invalid image: %s", szImage.c_str());
      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

   pDisplay->pTexture = SDL_CreateTextureFromSurface(pDisplay->pRenderer, pSurface);
   SDL_FreeSurface(pSurface);

   PLOGI.printf("Display %s image set to %s", szDisplay, szImage.c_str());
   mg_http_reply(c, 200, "", "display: %s, image: %s", szDisplay, szImage.c_str());
}

void VPXDisplayServer::Reset(struct mg_connection *c, void *ev_data)
{
   if (m_pBackglassDisplay && m_pBackglassDisplay->pTexture) {
      SDL_DestroyTexture(m_pBackglassDisplay->pTexture);
      m_pBackglassDisplay->pTexture = NULL;
   }

   if (m_pDMDDisplay && m_pDMDDisplay->pTexture) {
      SDL_DestroyTexture(m_pBackglassDisplay->pTexture);
      m_pBackglassDisplay->pTexture = NULL;
   }

   mg_http_reply(c, 200, "", "");
}

void VPXDisplayServer::Capture(struct mg_connection *c, void *ev_data)
{
   struct mg_http_message *hm = (struct mg_http_message *) ev_data;

   char szDisplay[10];
   mg_http_get_var(&hm->query, "display", szDisplay, sizeof(szDisplay));

   if (*szDisplay == '\0') {
      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

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

   char szPath[1024];
   mg_http_get_var(&hm->query, "path", szPath, sizeof(szPath));

   if (*szPath == '\0') {
      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

   std::filesystem::path path = string(szPath);
   string szCapture = m_szCachePath + path.stem().string() + "-" + szDisplay + ".png";

   // ffmpeg -f x11grab -video_size 1024x768 -i :0.0+1080,0 -vframes 1 "vpxfile-backglass.png"

   string szSize = std::to_string(pDisplay->width) + "x" + std::to_string(pDisplay->height);
   string szPosition = ":0.0+" + std::to_string(m_tableWidth) + ",0";

   const char* command_line[] = {"/usr/bin/ffmpeg",
                                 "-y",
                                 "-f", "x11grab",
                                 "-video_size", szSize.c_str(),
                                 "-i", szPosition.c_str(),
                                 "-vframes", "1",
                                 szCapture.c_str(),
                                 NULL};
   const char* environment[] = {"DISPLAY=:0.0",  NULL};

   struct subprocess_s subprocess;
   if (!subprocess_create_ex(command_line, 0, environment, &subprocess)) {
      int process_return;
      if (!subprocess_join(&subprocess, &process_return)) {
         PLOGI.printf("process return: %d", process_return);
         if (!process_return) {
            mg_http_reply(c, 200, "", "");
            return;
         }
      }
   }

   mg_http_reply(c, 500, "", "Server error");
}

void VPXDisplayServer::CaptureES(struct mg_connection *c, void *ev_data)
{
   struct mg_http_message *hm = (struct mg_http_message *) ev_data;

   char type[128];
   mg_http_get_var(&hm->query, "type", type, sizeof(type));

   if (*type == '\0') {
      mg_http_reply(c, 400, "", "Bad request");
      return;
   }

   PLOGI.printf("ES Capture Requested : type=%s", type);

   if (!strcmp(type, "image")) {
      string szPath = m_szBasePath + "vpxds-tmp-image.png";

      // ffmpeg -f x11grab -video_size 1080x1920 -i :0.0 -vframes 1 "vpxds-tmp-image.png"

      string szSize = std::to_string(m_tableWidth) + "x" + std::to_string(m_tableHeight);
      const char* command_line[] = {"/usr/bin/ffmpeg",
                                    "-y",
                                    "-f", "x11grab",
                                    "-video_size", szSize.c_str(),
                                    "-i", ":0.0",
                                    "-vframes", "1",
                                    szPath.c_str(),
                                    NULL};

      
      const char* environment[] = {"DISPLAY=:0.0",  NULL};

      struct subprocess_s subprocess;
      if (!subprocess_create_ex(command_line, 0, environment, &subprocess)) {
         int process_return;
         if (!subprocess_join(&subprocess, &process_return)) {
            PLOGI.printf("process returned: %d", process_return);
            if (!process_return) {
               struct mg_http_serve_opts opts = {};
               mg_http_serve_file(c, hm, szPath.c_str(), &opts);
               return;
            }
         }
      }

      mg_http_reply(c, 500, "", "Server error");
   }
   else if (!strcmp(type, "video")) {
      string szPath = m_szBasePath + "vpxds-tmp-video.mp4";

      // ffmpeg -f x11grab -video_size 1080x1920 -framerate 30 -i :0.0 -c:v libx264 -preset ultrafast -profile:v main -level 3.1 -pix_fmt yuv420p -t 10 "vpxds-tmp-video.mp4"

      string szSize = std::to_string(m_tableWidth) + "x" + std::to_string(m_tableHeight);
      const char* command_line[] = {"/usr/bin/ffmpeg",
                                    "-y",
                                    "-f", "x11grab",
                                    "-video_size", szSize.c_str(),
                                    "-framerate", "30",
                                    "-i", ":0.0",
                                    "-c:v", "libx264",
                                    "-preset", "slow",
                                    "-profile:v", "main",
                                    "-level", "3.1",
                                    "-pix_fmt", "yuv420p",
                                    "-t", "5",
                                    szPath.c_str(),
                                    NULL};
      const char* environment[] = {"DISPLAY=:0.0",  NULL};

      struct subprocess_s subprocess;
      if (!subprocess_create_ex(command_line, 0, environment, &subprocess)) {
         int process_return;
         if (!subprocess_join(&subprocess, &process_return)) {
            PLOGI.printf("process returned: %d", process_return);
            if (!process_return) {
               struct mg_http_serve_opts opts = {};
               mg_http_serve_file(c, hm, szPath.c_str(), &opts);
               return;
            }
         }
      }

      mg_http_reply(c, 500, "", "Server error");
   }
   else {
      mg_http_reply(c, 400, "", "Bad request");
   }
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

   if (ini["Settings"].has("ESURL"))
      m_szESUrl = ini["Settings"]["ESURL"];

   if (!ini["Settings"].has("TableWidth") ||
       !ini["Settings"].has("TableHeight")) {
      PLOGE << "Missing table width or height";
   }

   m_tableWidth = atoll(ini["Settings"]["TableWidth"].c_str());
   m_tableHeight = atoll(ini["Settings"]["TableHeight"].c_str());

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

   g_pServer = this;

   mg_log_set(MG_LL_NONE);

   struct mg_mgr mgr;
   struct mg_connection* conn;
   mg_mgr_init(&mgr);

   mg_http_listen(&mgr, "http://0.0.0.0:8111", VPXDisplayServer::HandleEvent, NULL);

   if (m_szESUrl.empty())
      m_szESUrl = "http://127.0.0.1:1234";

   PLOGI << "ES URL: " << m_szESUrl;

   bool quit = false;

   SDL_Event event;
   while (!quit) {
      while (SDL_PollEvent(&event)) {
         if (event.type == SDL_QUIT)
            quit = true;
      }

      RenderDisplay(m_pBackglassDisplay);
      RenderDisplay(m_pDMDDisplay);

      mg_mgr_poll(&mgr, 1000);
   }

   mg_mgr_free(&mgr);

   return 0;
}
