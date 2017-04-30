/*

 pxCore Copyright 2005-2017 John Robinson

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

// pxText.cpp

#include "pxText.h"
#include "rtFileDownloader.h"
#include "pxTimer.h"
#include "pxFont.h"
#include "pxContext.h"

extern pxContext context;


pxText::pxText(pxScene2d* scene):pxObject(scene),
                                 isItalic(false),
                                 isBold(false),
                                 mListenerAdded(false)
{
  float c[4] = {1, 1, 1, 1};
  memcpy(mTextColor, c, sizeof(mTextColor));
  // Default to use default font
  mFont = pxFontManager::getFont(defaultFont);
  mPixelSize = defaultPixelSize;
  mDirty = true;
}

pxText::~pxText()
{
  if (mListenerAdded)
  {
    if (getFontResource())
    {
      getFontResource()->removeListener(this);
    }
    mListenerAdded = false;
  }
}

void pxText::onInit()
{
  mInitialized = true;

  if( getFontResource()->isFontLoaded()) {
    resourceReady("resolve");
  }
}
rtError pxText::text(rtString& s) const { s = mText; return RT_OK; }

void pxText::sendPromise() 
{ 
  if(mInitialized && mFontLoaded && !((rtPromise*)mReady.getPtr())->status()) 
  {
    //rtLogDebug("pxText SENDPROMISE\n");
    mReady.send("resolve",this); 
  }
}

rtError pxText::setText(const char* s) 
{ 
  //rtLogInfo("pxText::setText\n");
  if( !mText.compare(s)){
    rtLogDebug("pxText.setText setting to same value %s and %s\n", mText.cString(), s);
    return RT_OK;
  }
  mText = s; 
  if( getFontResource()->isFontLoaded())
  {
    createNewPromise();
    getFontResource()->measureTextInternal(s, mPixelSize, 1.0, 1.0, mw, mh);
  }
  return RT_OK; 
}

rtError pxText::setItalic(bool var) 
{
  isItalic = var;
  if (getFontResource()->isFontLoaded()) 
  {
    createNewPromise();
    getFontResource()->setItalic(isItalic);
  }
  return RT_OK;
}

rtError pxText::setBold(bool var) 
{
  isBold = var;
  if (getFontResource()->isFontLoaded()) 
  {
    createNewPromise();
    getFontResource()->setBold(isBold);
  }
  return RT_OK;
}

rtError pxText::setPixelSize(uint32_t v) 
{   
  //rtLogInfo("pxText::setPixelSize\n");
  mPixelSize = v; 
  if( getFontResource()->isFontLoaded())
  {
    createNewPromise();
    getFontResource()->measureTextInternal(mText, mPixelSize, 1.0, 1.0, mw, mh);
  }
  return RT_OK; 
}

void pxText::resourceReady(rtString readyResolution)
{
  if( !readyResolution.compare("resolve"))
  {    
    mFontLoaded=true;
    // pxText gets its height and width from the text itself, 
    // so measure it
    getFontResource()->measureTextInternal(mText, mPixelSize, 1.0, 1.0, mw, mh);
    mDirty=true;  
    mScene->mDirty = true;
    // !CLF: ToDo Use pxObject::onTextureReady() and rename it.
    if( mInitialized) 
      pxObject::onTextureReady();
    
  }
  else 
  {
      pxObject::onTextureReady();
      mReady.send("reject",this);
  }     
}
       
void pxText::update(double t)
{
  pxObject::update(t);
  
#if 1
  if (mDirty)
  {
#if 0
    // TODO magic number
    if (mText.length() >= 5)
    {
      setPainting(true);
      setPainting(false);
    }
    else
      setPainting(true);
#else
    // TODO make this configurable
    // TODO make caching more intelligent given scaling
    if (mText.length() >= 10 && msx == 1.0 && msy == 1.0)
    {
      mCached = NULL;
      pxContextFramebufferRef cached = context.createFramebuffer(getFBOWidth() > MAX_TEXTURE_WIDTH?MAX_TEXTURE_WIDTH:getFBOWidth(),getFBOHeight() > MAX_TEXTURE_HEIGHT?MAX_TEXTURE_HEIGHT:getFBOHeight());//mw,mh);
      if (cached.getPtr())
      {
        pxContextFramebufferRef previousSurface = context.getCurrentFramebuffer();
        context.setFramebuffer(cached);
        pxMatrix4f m;
        context.setMatrix(m);
        context.setAlpha(1.0);
        context.clear((mw>MAX_TEXTURE_WIDTH?MAX_TEXTURE_WIDTH:mw), (mh>MAX_TEXTURE_HEIGHT?MAX_TEXTURE_HEIGHT:mh));
        draw();
        context.setFramebuffer(previousSurface);
        mCached = cached;
      }
    }
    else mCached = NULL;
    
#endif
    
    mDirty = false;
    }
#else
  mDirty = false;
#endif
  
}

void pxText::draw() 
{
  static pxTextureRef nullMaskRef;
  if( getFontResource()->isFontLoaded())
  {
    // TODO not very intelligent given scaling
    if (msx == 1.0 && msy == 1.0 && mCached.getPtr() && mCached->getTexture().getPtr())
    {
      context.drawImage(0, 0, (mw>MAX_TEXTURE_WIDTH?MAX_TEXTURE_WIDTH:mw), (mh>MAX_TEXTURE_HEIGHT?MAX_TEXTURE_HEIGHT:mh), mCached->getTexture(), nullMaskRef);
    }
    else
    {
      getFontResource()->renderText(mText, mPixelSize, 0, 0, msx, msy, mTextColor, mw,
                                    isItalic, isBold);
    }
  }  
  //else {
    //if (!mFontLoaded && getFontResource()->isDownloadInProgress())
      //getFontResource()->raiseDownloadPriority();
    //}
}

rtError pxText::setFontUrl(const char* s)
{
  //rtLogInfo("pxText::setFaceUrl for %s\n",s);
  if (!s || !s[0]) {
    s = defaultFont;
  }
  mFontLoaded = false;
  createNewPromise();

  mFont = pxFontManager::getFont(s);
  mListenerAdded = true;
  getFontResource()->addListener(this);
  
  return RT_OK;
}

rtError pxText::setFont(rtObjectRef o) 
{ 
  mFontLoaded = false;
  createNewPromise();

  // !CLF: TODO: Need validation/verification of o
  mFont = o; 
  mListenerAdded = true;
  getFontResource()->addListener(this);
    
  return RT_OK; 
}

float pxText::getOnscreenWidth()
{
  return (mw > MAX_TEXTURE_WIDTH?MAX_TEXTURE_WIDTH*msx:mw*msx);
}
float pxText::getOnscreenHeight()
{
  return (mh > MAX_TEXTURE_HEIGHT?MAX_TEXTURE_HEIGHT*msy:mh*msy);
}
  

rtDefineObject(pxText, pxObject);
rtDefineProperty(pxText, text);
rtDefineProperty(pxText, textColor);
rtDefineProperty(pxText, pixelSize);
rtDefineProperty(pxText, fontUrl);
rtDefineProperty(pxText, font);
rtDefineProperty(pxText, italic);
rtDefineProperty(pxText, bold);
