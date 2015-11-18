// pxCore CopyRight 2007-2015 John Robinson
// pxFont.cpp

#include "pxFont.h"
#include "pxFileDownloader.h"
#include "pxTimer.h"
#include "pxText.h"

#include <math.h>
#include <map>

struct GlyphKey {
  uint32_t mFaceId;
  uint32_t mPixelSize;
  uint32_t mCodePoint;

  // Provide a "<" operator that orders keys.
  // The way it orders them doesn't matter, all that matters is that
  // it orders them consistently.
  bool operator<(GlyphKey const& other) const {
    if (mFaceId < other.mFaceId) return true; else
      if (mFaceId == other.mFaceId) {
        if (mPixelSize < other.mPixelSize) return true; else
          if (mPixelSize == other.mPixelSize) {
            if (mCodePoint < other.mCodePoint) return true;
          }
      }
    return false;
  }
};

typedef map<GlyphKey,GlyphCacheEntry*> GlyphCache;

GlyphCache gGlyphCache;

#include "pxContext.h"

extern pxContext context;

// TODO can we eliminate direct utf8.h usage
extern "C" {
#include "utf8.h"
}

#if 0
<link href='http://fonts.googleapis.com/css?family=Fontdiner+Swanky' rel='stylesheet' type='text/css'>
#endif

FT_Library ft;


int fontDownloadsPending = 0; //must only be set in the main thread
rtMutex fontDownloadMutex;
bool fontDownloadsAvailable = false;
vector<FontDownloadRequest> completedFontDownloads;

void pxFontDownloadComplete(pxFileDownloadRequest* fileDownloadRequest)
{
  if (fileDownloadRequest != NULL)
  {
    FontDownloadRequest fontDownloadRequest;
    fontDownloadRequest.fileDownloadRequest = fileDownloadRequest;
    fontDownloadMutex.lock();
    completedFontDownloads.push_back(fontDownloadRequest);
    fontDownloadsAvailable = true;
    fontDownloadMutex.unlock();
  }
}


// Weak Map
//typedef map<rtString, pxFace*> FaceMap;
//FaceMap gFaceMap;

uint32_t gFaceId = 0;

pxFace::pxFace():mPixelSize(0), mRefCount(0), mFontData(0), mInitialized(false) { mFaceId = gFaceId++; }

void pxFace::onDownloadComplete(const FT_Byte*  fontData, FT_Long size, const char* n)
{
  printf("pxFace::onDownloadComplete %s\n",n);
  if( mFaceName.compare(n)) 
  {
    rtLogWarn("pxFace::onDownloadComplete received for face \"%s\" but this face is \"%s\"\n",n, mFaceName.cString());
    return; 
  }

  init(fontData, size, n);
  
  for (vector<pxFont*>::iterator it = mListeners.begin();
         it != mListeners.end(); ++it)
    {
      (*it)->fontLoaded();// This is really just a single pxFont... so maybe this listener, etc. is not necessary

    }
    mListeners.clear();
}

void pxFace::addListener(pxFont* pFont) 
{
  mListeners.push_back(pFont);
  
}
void pxFace::setFaceName(const char* n)
{
  mFaceName = n;
}
rtError pxFace::init(const char* n)
{
  mFaceName = n;
    
  if(FT_New_Face(ft, n, 0, &mFace))
    return RT_FAIL;
  
  setPixelSize(defaultPixelSize);
  mInitialized = true;

//  gFaceMap.insert(make_pair(n, this));


  return RT_OK;
}

rtError pxFace::init(const FT_Byte*  fontData, FT_Long size, const char* n)
{
  // We need to keep a copy of fontData since the download will be deleted.
  mFontData = (char *)malloc(size);
  memcpy(mFontData, fontData, size);
  
  if(FT_New_Memory_Face(ft, (const FT_Byte*)mFontData, size, 0, &mFace))
    return RT_FAIL;

  mFaceName = n;
  setPixelSize(defaultPixelSize);
  mInitialized = true;

  return RT_OK;
}

pxFace::~pxFace() 
{ 
  //rtLogInfo("~pxFace\n"); 
  //FaceMap::iterator it = gFaceMap.find(mFaceName);
  //if (it != gFaceMap.end())
    //gFaceMap.erase(it);
//#if 0
  //else
    //rtLogError("Could not find faceName in map");
//#endif
  //if(mFontData) {
    //free(mFontData);
    //mFontData = 0;
  //}
}

void pxFace::setPixelSize(uint32_t s)
{
  if (mPixelSize != s)
  {
    if( mInitialized) {
      FT_Set_Pixel_Sizes(mFace, 0, s);
    }
    mPixelSize = s;
  }
}
void pxFace::getHeight(uint32_t size, float& height)
{
 	// is mFace ever not valid?
	// TO DO:  check FT_IS_SCALABLE 
  if( !mInitialized) return;
  
  setPixelSize(size);
  
	FT_Size_Metrics* metrics = &mFace->size->metrics;
  	
	height = metrics->height>>6; 
}
  
void pxFace::getMetrics(uint32_t size, float& height, float& ascender, float& descender, float& naturalLeading)
{
//	printf("pxFace::getMetrics\n");
	// is mFace ever not valid?
	// TO DO:  check FT_IS_SCALABLE 
  if( !mInitialized) return;
  
  setPixelSize(size);
  
	FT_Size_Metrics* metrics = &mFace->size->metrics;
  	
	height = metrics->height>>6;
	ascender = metrics->ascender>>6; 
	descender = -metrics->descender>>6; 
  naturalLeading = height - (ascender + descender);

}
  
const GlyphCacheEntry* pxFace::getGlyph(uint32_t codePoint)
{
  GlyphKey key; 
  key.mFaceId = mFaceId; 
  key.mPixelSize = mPixelSize; 
  key.mCodePoint = codePoint;
  GlyphCache::iterator it = gGlyphCache.find(key);
  if (it != gGlyphCache.end())
    return it->second;
  else
  {
    if(FT_Load_Char(mFace, codePoint, FT_LOAD_RENDER))
      return NULL;
    else
    {
      rtLogDebug("glyph cache miss");
      GlyphCacheEntry *entry = new GlyphCacheEntry;
      FT_GlyphSlot g = mFace->glyph;
      
      entry->bitmap_left = g->bitmap_left;
      entry->bitmap_top = g->bitmap_top;
      entry->bitmapdotwidth = g->bitmap.width;
      entry->bitmapdotrows = g->bitmap.rows;
      entry->advancedotx = g->advance.x;
      entry->advancedoty = g->advance.y;
      entry->vertAdvance = g->metrics.vertAdvance; // !CLF: Why vertAdvance? SHould only be valid for vert layout of text.
      
      entry->mTexture = context.createTexture(g->bitmap.width, g->bitmap.rows, 
                                              g->bitmap.width, g->bitmap.rows, 
                                              g->bitmap.buffer);
      
      gGlyphCache.insert(make_pair(key,entry));
      return entry;
    }
  }
  return NULL;
}

void pxFace::measureText(const char* text, uint32_t size,  float sx, float sy, 
                         float& w, float& h) 
{
  
  if( !mInitialized) {
    rtLogWarn("measureText called TOO EARLY -- not initialized or font not loaded!\n");
    return;
  }

  setPixelSize(size);
  
  w = 0; h = 0;
  if (!text) 
    return;
  int i = 0;
  u_int32_t codePoint;
  
  FT_Size_Metrics* metrics = &mFace->size->metrics;
  
  h = metrics->height>>6;
  float lw = 0;
  while((codePoint = u8_nextchar((char*)text, &i)) != 0) 
  {
    const GlyphCacheEntry* entry = getGlyph(codePoint);
    if (!entry) 
      continue;
      
    if (codePoint != '\n')
    {
      lw += (entry->advancedotx >> 6) * sx;
    }
    else
    {
      h += metrics->height>>6;
      lw = 0;
    }
    w = pxMax<float>(w, lw);
//    h = pxMax<float>((g->advance.y >> 6) * sy, h);
//    h = pxMax<float>((metrics->height >> 6) * sy, h);
  }
  h *= sy;
}

void pxFace::renderText(const char *text, uint32_t size, float x, float y, 
                        float sx, float sy, 
                        float* color, float mw) 
{
  if (!text || !mInitialized) 
    return;

  int i = 0;
  u_int32_t codePoint;

  setPixelSize(size);
  FT_Size_Metrics* metrics = &mFace->size->metrics;

  while((codePoint = u8_nextchar((char*)text, &i)) != 0) 
  {
    const GlyphCacheEntry* entry = getGlyph(codePoint);
    if (!entry) 
      continue;

    float x2 = x + entry->bitmap_left * sx;
//    float y2 = y - g->bitmap_top * sy;
    float y2 = (y - entry->bitmap_top * sy) + (metrics->ascender>>6);
    float w = entry->bitmapdotwidth * sx;
    float h = entry->bitmapdotrows * sy;
    
    if (codePoint != '\n')
    {
      if (x == 0) 
      {
        float c[4] = {0, 1, 0, 1};
        context.drawDiagLine(0, y+(metrics->ascender>>6), mw, 
                             y+(metrics->ascender>>6), c);
      }
      
      pxTextureRef texture = entry->mTexture;
      pxTextureRef nullImage;
      context.drawImage(x2,y2, w, h, texture, nullImage, PX_NONE, PX_NONE, 
                        color);
      x += (entry->advancedotx >> 6) * sx;
      // TODO not sure if this is right?  seems weird commenting out to see what happens
      y += (entry->advancedoty >> 6) * sy;
    }
    else
    {
      x = 0;
      // TODO not sure if this is right?
      y += (entry->vertAdvance>>6) * sy;
    }
  }
}

void pxFace::measureTextChar(u_int32_t codePoint, uint32_t size,  float sx, float sy, 
                         float& w, float& h) 
{
  if( !mInitialized) {
    rtLogWarn("measureTextChar called TOO EARLY -- not initialized or font not loaded!\n");
    return;
  }
  
  setPixelSize(size);
  
  w = 0; h = 0;
  
  FT_Size_Metrics* metrics = &mFace->size->metrics;
  
  h = metrics->height>>6;
  float lw = 0;

  const GlyphCacheEntry* entry = getGlyph(codePoint);
  if (!entry) 
    return;

  lw = (entry->advancedotx >> 6) * sx;

  w = pxMax<float>(w, lw);

  h *= sy;
}



typedef rtRefT<pxFace> pxFaceRef;

//pxFont* gFont;


pxFont::pxFont(pxScene2d* scene, rtString faceURL):pxObject(scene), mFontDownloadRequest(NULL)
{  
  mFaceName = faceURL;
  
  loadFont();
}

pxFont::~pxFont() 
{
  if (mFontDownloadRequest != NULL)
  {
    // if there is a previous request pending then set the callback to NULL
    // the previous request will not be processed and the memory will be freed when the download is complete
    mFontDownloadRequest->setCallbackFunctionThreadSafe(NULL);
  }  
}

void pxFont::fontLoaded()
{
    mInitialized = true;

    sendReady("resolve");
    // This would normally just do mReady.send, but that's
    // being done via listeners for now.
}

void pxFont::addListener(pxText* pText) 
{
  printf("pxFont::addListener for %s\n",mFaceName.cString());
  if( !mInitialized) {
    mListeners.push_back(pText);
  } else {
    pText->fontLoaded("resolve");
  }
  
}
rtError pxFont::loadFont()
{
printf("pxFont::loadFont for %s\n",mFaceName.cString());
    const char *result = strstr(mFaceName, "http");
    int position = result - mFaceName;
    if (position == 0 && strlen(mFaceName) > 0)
    {
      if (mFontDownloadRequest != NULL)
      {
        // if there is a previous request pending then set the callback to NULL
        // the previous request will not be processed and the memory will be freed when the download is complete
        mFontDownloadRequest->setCallbackFunctionThreadSafe(NULL);
      }
      // Create the face and add this text as a listener for the download complete event.
      // This pxFace creation should only ever happen once for each font face!
      pxFaceRef f = new pxFace;
      f->setFaceName(mFaceName);
      f->addListener(this);
      mFace = f;
      // Add face to map even though it is not yet initialized, ie., hasn't downloaded yet
//      gFontMap.insert(make_pair(mFaceName, f));
      // Start the download request
      mFontDownloadRequest =
          new pxFileDownloadRequest(mFaceName, this);
      fontDownloadsPending++;
      mFontDownloadRequest->setCallbackFunction(pxFontDownloadComplete);
      pxFileDownloader::getInstance()->addToDownloadQueue(mFontDownloadRequest);

    }
    else {
      pxFaceRef f = new pxFace;
      mFace = f;
      rtError e = f->init(mFaceName);
      if (e != RT_OK)
      {
        rtLogInfo("Could not load font face %s\n", mFaceName.cString());
        sendReady("reject");
        //fontLoaded(); // font is still loaded, it's just the default font because some 
                      // error occurred.
        //mReady.send("reject",this);
        return e;
      }
      else
      {
        //printf("pxText::setFaceURL calling fontLoaded\n");
        
        fontLoaded();
        //mReady.send("resolve", this);
        //mFace = f;
      }
    }


  
  return RT_OK;
}

void pxFont::checkForCompletedDownloads(int maxTimeInMilliseconds)
{
  //printf("pxFont::checkForCompletedDownloads\n");
  double startTimeInMs = pxMilliseconds();
  if (fontDownloadsPending > 0)
  {
    fontDownloadMutex.lock();
    if (fontDownloadsAvailable)
    {
      for(vector<FontDownloadRequest>::iterator it = completedFontDownloads.begin(); it != completedFontDownloads.end(); )
      {
        FontDownloadRequest fontDownloadRequest = (*it);
        if (!fontDownloadRequest.fileDownloadRequest)
        {
          it = completedFontDownloads.erase(it);
          continue;
        }
        if (fontDownloadRequest.fileDownloadRequest->getCallbackData() != NULL)
        {
          pxFont *fontObject = (pxFont *) fontDownloadRequest.fileDownloadRequest->getCallbackData();
          fontObject->onFontDownloadComplete(fontDownloadRequest);
        }

        delete fontDownloadRequest.fileDownloadRequest;
        fontDownloadsAvailable = false;
        fontDownloadsPending--;
        it = completedFontDownloads.erase(it);
        double currentTimeInMs = pxMilliseconds();
        if ((maxTimeInMilliseconds >= 0) && (currentTimeInMs - startTimeInMs > maxTimeInMilliseconds))
        {
          break;
        }
      }
      if (fontDownloadsPending < 0)
      {
        //this is a safety check (hopefully never used)
        //to ensure downloads are still processed in the event of a fontDownloadsPending bug in the future
        fontDownloadsPending = 0;
      }
    }
    fontDownloadMutex.unlock();
  }
}

void pxFont::onFontDownloadComplete(FontDownloadRequest fontDownloadRequest)
{
  printf("pxFont::onFontDownloadComplete\n");
  mFontDownloadRequest = NULL;
  if (fontDownloadRequest.fileDownloadRequest == NULL)
  {
    sendReady("reject");
    //mReady.send("reject",this);
    return;
  }
  if (fontDownloadRequest.fileDownloadRequest->getDownloadStatusCode() == 0 &&
      fontDownloadRequest.fileDownloadRequest->getHttpStatusCode() == 200 &&
      fontDownloadRequest.fileDownloadRequest->getDownloadedData() != NULL)
  {
    // Let the face handle the completion event and notifications to listeners
    mFace->onDownloadComplete((FT_Byte*)fontDownloadRequest.fileDownloadRequest->getDownloadedData(),
                          (FT_Long)fontDownloadRequest.fileDownloadRequest->getDownloadedDataSize(),
                          fontDownloadRequest.fileDownloadRequest->getFileURL().cString());

  }
  else
  {
    rtLogWarn("Font Download Failed: %s Error: %s HTTP Status Code: %ld",
              fontDownloadRequest.fileDownloadRequest->getFileURL().cString(),
              fontDownloadRequest.fileDownloadRequest->getErrorString().cString(),
              fontDownloadRequest.fileDownloadRequest->getHttpStatusCode());
    if (mParent != NULL)
    {
      sendReady("reject");
      //mReady.send("reject",this);
    }
  }
}
void pxFont::sendReady(const char * value)
{
  for (vector<pxText*>::iterator it = mListeners.begin();
         it != mListeners.end(); ++it)
    {
      (*it)->fontLoaded(value);

    }
    mListeners.clear();
    mReady.send(value,this);
}

/*
#### getFontMetrics - returns information about the font face (font and size).  It does not convey information about the text of the font.  
* See section 3.a in http://www.freetype.org/freetype2/docs/tutorial/step2.html .  
* The returned object has the following properties:
* height - float - the distance between baselines
* ascent - float - the distance from the baseline to the font ascender (note that this is a hint, not a solid rule)
* descent - float - the distance from the baseline to the font descender  (note that this is a hint, not a solid rule)
*/
rtError pxFont::getFontMetrics(uint32_t pixelSize, rtObjectRef& o) {
    //printf("pxText2::getFontMetrics\n");  
	float height, ascent, descent, naturalLeading;
	pxTextMetrics* metrics = new pxTextMetrics(mScene);


    if(!mInitialized ) {
      rtLogWarn("getFontMetrics called TOO EARLY -- not initialized or font not loaded!\n");
      o = metrics;
      return RT_OK; // !CLF: TO DO - COULD RETURN RT_ERROR HERE TO CATCH NOT WAITING ON PROMISE
    }



	mFace->getMetrics(pixelSize, height, ascent, descent, naturalLeading);
	metrics->setHeight(height);
	metrics->setAscent(ascent);
	metrics->setDescent(descent);
  metrics->setNaturalLeading(naturalLeading);
  metrics->setBaseline(my+ascent);
	o = metrics;

	return RT_OK;
}

rtError pxFont::measureText(uint32_t pixelSize, rtString stringToMeasure, rtObjectRef& o)
{
    pxTextSimpleMeasurements* measure = new pxTextSimpleMeasurements(mScene);
    
    if(!mInitialized ) {
      rtLogWarn("measureText called TOO EARLY -- not initialized or font not loaded!\n");
      o = measure;
      return RT_OK; // !CLF: TO DO - COULD RETURN RT_ERROR HERE TO CATCH NOT WAITING ON PROMISE
    } 
    float w, h;
    mFace->measureText(stringToMeasure, pixelSize, 1.0,1.0, w, h);
    measure->setW(w);
    measure->setH(h);
    o = measure;
    
    return RT_OK; 
}

FontMap pxFontManager::mFontMap;
bool pxFontManager::init = false;
void pxFontManager::initFT(pxScene2d* scene) 
{
  if (init) {
    //printf("initFT returning; already inited\n");
    return;
  }
  init = true;
  
  if(FT_Init_FreeType(&ft)) 
  {
    rtLogError("Could not init freetype library\n");
    return;
  }
  
  // Set up default font
  pxFont * pFont = new pxFont(scene, defaultFace);
  mFontMap.insert(make_pair(defaultFace, pFont));

}
rtObjectRef pxFontManager::getFont(pxScene2d* scene, const char* s)
{
  return getFontObj(scene, s);

}
pxFont* pxFontManager::getFontObj(pxScene2d* scene, const char* s)
{
  printf("pxFontManager::getFontObj\n");
  initFT(scene);

  pxFont* pFont;

  if (!s || !s[0])
    s = defaultFace;

  FontMap::iterator it = mFontMap.find(s);
  if (it != mFontMap.end())
  {
    pFont = it->second;
    return pFont;  
    
  }
  else 
  {
    pFont = new pxFont(scene, s);
    mFontMap.insert(make_pair(s, pFont));
  }
 printf("pxFontManager::getFontObj END\n");
 return pFont;
}
   

// pxTextMetrics
rtDefineObject(pxTextMetrics, pxObject);
rtDefineProperty(pxTextMetrics, height); 
rtDefineProperty(pxTextMetrics, ascent);
rtDefineProperty(pxTextMetrics, descent);
rtDefineProperty(pxTextMetrics, naturalLeading);
rtDefineProperty(pxTextMetrics, baseline);

// pxFont
rtDefineObject(pxFont, pxObject);
rtDefineMethod(pxFont, getFontMetrics);
rtDefineMethod(pxFont, measureText);

rtDefineObject(pxTextSimpleMeasurements, pxObject);