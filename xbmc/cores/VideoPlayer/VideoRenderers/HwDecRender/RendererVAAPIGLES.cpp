/*
 *      Copyright (C) 2007-2015 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "RendererVAAPIGLES.h"

#include "cores/VideoPlayer/DVDCodecs/Video/VAAPI.h"
#include "cores/VideoPlayer/DVDCodecs/DVDCodecUtils.h"
#include "settings/Settings.h"
#include "settings/AdvancedSettings.h"
#include "utils/log.h"
#include "utils/GLUtils.h"

CRendererVAAPI::CRendererVAAPI()
{

}

CRendererVAAPI::~CRendererVAAPI()
{
  for (int i = 0; i < NUM_BUFFERS; ++i)
  {
    DeleteTexture(i);
  }
}

bool CRendererVAAPI::HandlesVideoBuffer(CVideoBuffer *buffer)
{
  VAAPI::CVaapiRenderPicture *pic = dynamic_cast<VAAPI::CVaapiRenderPicture*>(buffer);
  if (pic)
    return true;

  return false;
}

bool CRendererVAAPI::Configure(const VideoPicture &picture, float fps, unsigned flags, unsigned int orientation)
{
  VAAPI::CVaapiRenderPicture *pic = dynamic_cast<VAAPI::CVaapiRenderPicture*>(picture.videoBuffer);
  if (pic->textureY)
    m_isVAAPIBuffer = true;
  else
    m_isVAAPIBuffer = false;

  return CLinuxRendererGLES::Configure(picture, fps, flags, orientation);
}

bool CRendererVAAPI::ConfigChanged(const VideoPicture &picture)
{
  VAAPI::CVaapiRenderPicture *pic = dynamic_cast<VAAPI::CVaapiRenderPicture*>(picture.videoBuffer);
  if (pic->textureY && !m_isVAAPIBuffer)
    return true;

  return false;
}

bool CRendererVAAPI::Supports(ERENDERFEATURE feature)
{
  return CLinuxRendererGLES::Supports(feature);
}

bool CRendererVAAPI::Supports(ESCALINGMETHOD method)
{
  return CLinuxRendererGLES::Supports(method);
}

EShaderFormat CRendererVAAPI::GetShaderFormat()
{
  EShaderFormat ret = SHADER_NONE;

  if (m_isVAAPIBuffer)
    ret = SHADER_NV12_RRG;
  else
    ret = SHADER_NV12;

  return ret;
}

bool CRendererVAAPI::LoadShadersHook()
{
  return false;
}

bool CRendererVAAPI::RenderHook(int idx)
{
  return false;
}

bool CRendererVAAPI::CreateTexture(int index)
{
  if (!m_isVAAPIBuffer)
  {
    return CreateNV12Texture(index);
  }

  YUVBUFFER &buf = m_buffers[index];
  YuvImage &im = buf.image;
  YUVPLANE (&planes)[YuvImage::MAX_PLANES] = buf.fields[0];

  DeleteTexture(index);

  memset(&im, 0, sizeof(im));
  memset(&planes, 0, sizeof(YUVPLANE[YuvImage::MAX_PLANES]));
  im.height = m_sourceHeight;
  im.width  = m_sourceWidth;
  im.cshift_x = 1;
  im.cshift_y = 1;

  planes[0].id = 1;

  return true;
}

void CRendererVAAPI::DeleteTexture(int index)
{
  ReleaseBuffer(index);

  if (!m_isVAAPIBuffer)
  {
    DeleteNV12Texture(index);
    return;
  }

  YUVBUFFER &buf = m_buffers[index];
  buf.fields[FIELD_FULL][0].id = 0;
  buf.fields[FIELD_FULL][1].id = 0;
  buf.fields[FIELD_FULL][2].id = 0;
}

bool CRendererVAAPI::UploadTexture(int index)
{
  if (!m_isVAAPIBuffer)
  {
    return UploadNV12Texture(index);
  }

  YUVBUFFER &buf = m_buffers[index];

  VAAPI::CVaapiRenderPicture *pic = dynamic_cast<VAAPI::CVaapiRenderPicture*>(buf.videoBuffer);

  if (!pic || !pic->valid)
  {
    return false;
  }

  YuvImage &im = buf.image;
  YUVPLANE (&planes)[3] = buf.fields[0];

  planes[0].texwidth  = pic->texWidth;
  planes[0].texheight = pic->texHeight;

  planes[1].texwidth  = planes[0].texwidth  >> im.cshift_x;
  planes[1].texheight = planes[0].texheight >> im.cshift_y;
  planes[2].texwidth  = planes[1].texwidth;
  planes[2].texheight = planes[1].texheight;

  for (int p = 0; p < 3; p++)
  {
    planes[p].pixpertex_x = 1;
    planes[p].pixpertex_y = 1;
  }

  // set textures
  planes[0].id = pic->textureY;
  planes[1].id = pic->textureVU;
  planes[2].id = pic->textureVU;

  glEnable(m_textureTarget);

  for (int p=0; p<2; p++)
  {
    glBindTexture(m_textureTarget, planes[p].id);
    glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(m_textureTarget, 0);
    VerifyGLState();
  }

  CalculateTextureSourceRects(index, 3);
  glDisable(m_textureTarget);
  return true;
}

void CRendererVAAPI::AfterRenderHook(int idx)
{
  YUVBUFFER &buf = m_buffers[idx];
  VAAPI::CVaapiRenderPicture *pic = dynamic_cast<VAAPI::CVaapiRenderPicture*>(buf.videoBuffer);
  if (pic)
  {
    pic->Sync();
  }
}
