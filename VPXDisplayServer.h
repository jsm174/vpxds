#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

struct VPXDisplay {
   SDL_Window* pWindow;
   SDL_Renderer* pRenderer;
   SDL_Texture* pTexture;
   int x;
   int y;
   int width;
   int height;
};

class VPXDisplayServer
{
public:
   VPXDisplayServer();
   ~VPXDisplayServer();

   static void Forward(struct mg_http_message *hm, struct mg_connection *c);
   static void HandleEvent(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
   static void HandleEvent2(struct mg_connection *c, int ev, void *ev_data, void *fn_data);

   int Start();
   void SetBasePath(const string& path) { m_szBasePath = path; }

private:
   void LoadINI();
   int OpenDisplay(const string& szName, VPXDisplay* pDisplay);
   void CloseDisplay(VPXDisplay* pDisplay);
   void RenderDisplay(VPXDisplay* pDisplay);
   void Update(struct mg_connection *c, void *ev_data);
   void Reset(struct mg_connection *c, void *ev_data);
   void Capture(struct mg_connection *c, void *ev_data);
   void CaptureES(struct mg_connection *c, void *ev_data);
   bool ProcessPNG(const std::string& filename);

   VPXDisplay* m_pBackglassDisplay;
   VPXDisplay* m_pDMDDisplay;

   string m_szCachePath;
   string m_szBasePath;
   string m_szESUrl;

   int m_tableWidth;
   int m_tableHeight;
};