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

   static void EventHandler(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
   void EventHandler(struct mg_connection *c, int ev, void *ev_data);

   int Start();
   void SetBasePath(const string& path) { m_szBasePath = path; }

private:
   void LoadINI();
   int OpenDisplay(const string& szName, VPXDisplay* pDisplay);
   void CloseDisplay(VPXDisplay* pDisplay);
   void RenderDisplay(VPXDisplay* pDisplay);
   void UpdateRequest(struct mg_connection *c, void *ev_data);
   void B2SRequest(struct mg_connection *c, void *ev_data);
   void ResetRequest(struct mg_connection *c, void *ev_data);
   SDL_Surface* GetB2SImage(const string& filename);
   SDL_Surface* Base64ToImage(const string& image);
   vector<unsigned char> Base64Decode(const string &encoded_string);
   SDL_Surface* ResizeImage(SDL_Surface* pSourceImage, int grillheight);

   VPXDisplay* m_pBackglassDisplay;
   VPXDisplay* m_pDMDDisplay;

   string m_szCachePath;
   string m_szBasePath;
};