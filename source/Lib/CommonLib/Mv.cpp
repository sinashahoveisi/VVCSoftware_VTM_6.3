/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2019, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     Mv.cpp
    \brief    motion vector class
*/

#include "Mv.h"

#include "Common.h"
#include "Slice.h"
#include <chrono>

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::nanoseconds;

long long int timeOfRoundAffineMv = 0;
long long int timeOfChangeAffinePrecInternal2Amvr = 0;
long long int timeOfRoundAffinePrecInternal2Amvr  = 0;
long long int timeOfCopyAffineMvFrom              = 0;

const MvPrecision Mv::m_amvrPrecision[4] = { MV_PRECISION_QUARTER, MV_PRECISION_INT, MV_PRECISION_4PEL, MV_PRECISION_HALF }; // for cu.imv=0, 1, 2 and 3
const MvPrecision Mv::m_amvrPrecAffine[3] = { MV_PRECISION_QUARTER, MV_PRECISION_SIXTEENTH, MV_PRECISION_INT }; // for cu.imv=0, 1 and 2
const MvPrecision Mv::m_amvrPrecIbc[3] = { MV_PRECISION_INT, MV_PRECISION_INT, MV_PRECISION_4PEL }; // for cu.imv=0, 1 and 2

void roundAffineMv( int& mvx, int& mvy, int nShift )
{
  auto      start   = high_resolution_clock::now();
  const int nOffset = 1 << (nShift - 1);
  mvx = (mvx + nOffset - (mvx >= 0)) >> nShift;
  mvy = (mvy + nOffset - (mvy >= 0)) >> nShift;
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<nanoseconds>(stop - start);
  timeOfRoundAffineMv = timeOfRoundAffineMv + duration.count();
}

void clipMv( Mv& rcMv, const Position& pos, const struct Size& size, const SPS& sps, const PPS& pps )
{
  if (sps.getWrapAroundEnabledFlag())
  {
    wrapClipMv(rcMv, pos, size, &sps, &pps);
    return;
  }

  int iMvShift = MV_FRACTIONAL_BITS_INTERNAL;
  int iOffset = 8;
  int iHorMax = ( pps.getPicWidthInLumaSamples() + iOffset - (int)pos.x - 1 ) << iMvShift;
  int iHorMin = ( -( int ) sps.getMaxCUWidth()   - iOffset - ( int ) pos.x + 1 ) << iMvShift;

  int iVerMax = ( pps.getPicHeightInLumaSamples() + iOffset - (int)pos.y - 1 ) << iMvShift;
  int iVerMin = ( -( int ) sps.getMaxCUHeight()   - iOffset - ( int ) pos.y + 1 ) << iMvShift;

  rcMv.setHor( std::min( iHorMax, std::max( iHorMin, rcMv.getHor() ) ) );
  rcMv.setVer( std::min( iVerMax, std::max( iVerMin, rcMv.getVer() ) ) );
}

bool wrapClipMv( Mv& rcMv, const Position& pos, const struct Size& size, const SPS *sps, const PPS *pps )
{
  bool wrapRef = true;
  int iMvShift = MV_FRACTIONAL_BITS_INTERNAL;
  int iOffset = 8;
  int iHorMax = ( pps->getPicWidthInLumaSamples() + sps->getMaxCUWidth() - size.width + iOffset - (int)pos.x - 1 ) << iMvShift;
  int iHorMin = ( -( int ) sps->getMaxCUWidth()                                      - iOffset - ( int ) pos.x + 1 ) << iMvShift;
  int iVerMax = ( pps->getPicHeightInLumaSamples() + iOffset - (int)pos.y - 1 ) << iMvShift;
  int iVerMin = ( -( int ) sps->getMaxCUHeight()   - iOffset - ( int ) pos.y + 1 ) << iMvShift;
  int mvX = rcMv.getHor();

  if(mvX > iHorMax)
  {
    mvX -= ( sps->getWrapAroundOffset() << iMvShift );
    mvX = std::min( iHorMax, std::max( iHorMin, mvX ) );
    wrapRef = false;
  }
  if(mvX < iHorMin)
  {
    mvX += ( sps->getWrapAroundOffset() << iMvShift );
    mvX = std::min( iHorMax, std::max( iHorMin, mvX ) );
    wrapRef = false;
  }

  rcMv.setHor( mvX );
  rcMv.setVer( std::min( iVerMax, std::max( iVerMin, rcMv.getVer() ) ) );
  return wrapRef;
}

//! \}
