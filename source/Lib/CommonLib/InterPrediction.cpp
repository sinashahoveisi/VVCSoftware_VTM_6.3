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

/** \file     Prediction.cpp
    \brief    prediction class
*/


#include "InterPrediction.h"

#include "Buffer.h"
#include "UnitTools.h"
#include "MCTS.h"

#include "ApproximatAdderSubtraction.h"

#include <memory.h>
#include <algorithm>
#include <iostream>
#include <math.h>
#include <limits>
#include <chrono>

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::nanoseconds;

using namespace std;

long long int timeOfSimdVerticalSobelFilter = 0;
long long int timeOfSimdHorizontalSobelFilter = 0;
long long int timeOfSimdEqualCoeffComputer   = 0;
long long int timeOfXFinalPaddedMCForDMVR   = 0;
long long int timeOfXProcessDMVR            = 0;
long long int timeOfXBIPMVRefine                  = 0;
long long int timeOfXPrefetch                  = 0;
long long int timeOfXWeightedTriangleBlk_SSE = 0;
long long int timeOfSimdInterpolateLuma10Bit2P16  = 0;
long long int timeOfSimdInterpolateLuma10Bit2P4   = 0;
long long int timeOfXPredAffineBlk           = 0;
long long int timeOfXDMVRCost                = 0;
long long int timeOfApplyBiOptFlow           = 0;
long long int timeOfXAddBIOAvg4              = 0;
long long int timeOfXBioGradFilter           = 0;
long long int timeOfXSubPuBio                = 0;
long long int timeOfXPredInterBlk            = 0;
long long int timeOfRightShiftMSB                 = 0;
long long int timeIsSubblockVectorSpreadOverLimit = 0;
long long int timeOfSimdInterpolateN2_10BIT_M4    = 0;
long long int timeOfSimdInterpolateLuma10Bit2P8   = 0;
long long int timeOfXinitMC                       = 0;
long long int timeOfSimdFilter                    = 0;
long long int timeOfXPad                          = 0;
long long int timeOfXSubPelErrorSrfc              = 0;
long long int timeOfXDMVRSubPixelErrorSurface     = 0;
long long int timeOfDiv_for_maxq7                 = 0;
long long int timeOfUpdateCandList                = 0;

//! \ingroup CommonLib
//! \{

// ====================================================================================================================
// Constructor / destructor / initialize
// ====================================================================================================================

InterPrediction::InterPrediction()
:
  m_currChromaFormat( NUM_CHROMA_FORMAT )
, m_maxCompIDToPred ( MAX_NUM_COMPONENT )
, m_pcRdCost        ( nullptr )
, m_storedMv        ( nullptr )
, m_skipPROF (false)
, m_encOnly  (false)
, m_isBi     (false)
, m_gradX0(nullptr)
, m_gradY0(nullptr)
, m_gradX1(nullptr)
, m_gradY1(nullptr)
, m_subPuMC(false)
{
  for( uint32_t ch = 0; ch < MAX_NUM_COMPONENT; ch++ )
  {
    for( uint32_t refList = 0; refList < NUM_REF_PIC_LIST_01; refList++ )
    {
      m_acYuvPred[refList][ch] = nullptr;
    }
  }

  for( uint32_t c = 0; c < MAX_NUM_COMPONENT; c++ )
  {
    for( uint32_t i = 0; i < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; i++ )
    {
      for( uint32_t j = 0; j < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; j++ )
      {
        m_filteredBlock[i][j][c] = nullptr;
      }

      m_filteredBlockTmp[i][c] = nullptr;
    }
  }
  m_cYuvPredTempDMVRL1 = nullptr;
  m_cYuvPredTempDMVRL0 = nullptr;
  for (uint32_t ch = 0; ch < MAX_NUM_COMPONENT; ch++)
  {
    m_cRefSamplesDMVRL0[ch] = nullptr;
    m_cRefSamplesDMVRL1[ch] = nullptr;
  }
}

InterPrediction::~InterPrediction()
{
  destroy();
}

void InterPrediction::destroy()
{
  for( uint32_t i = 0; i < NUM_REF_PIC_LIST_01; i++ )
  {
    for( uint32_t c = 0; c < MAX_NUM_COMPONENT; c++ )
    {
      xFree( m_acYuvPred[i][c] );
      m_acYuvPred[i][c] = nullptr;
    }
  }

  for( uint32_t c = 0; c < MAX_NUM_COMPONENT; c++ )
  {
    for( uint32_t i = 0; i < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; i++ )
    {
      for( uint32_t j = 0; j < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; j++ )
      {
        xFree( m_filteredBlock[i][j][c] );
        m_filteredBlock[i][j][c] = nullptr;
      }

      xFree( m_filteredBlockTmp[i][c] );
      m_filteredBlockTmp[i][c] = nullptr;
    }
  }

  m_triangleBuf.destroy();

  if (m_storedMv != nullptr)
  {
    delete[]m_storedMv;
    m_storedMv = nullptr;
  }

  xFree(m_gradX0);   m_gradX0 = nullptr;
  xFree(m_gradY0);   m_gradY0 = nullptr;
  xFree(m_gradX1);   m_gradX1 = nullptr;
  xFree(m_gradY1);   m_gradY1 = nullptr;
  xFree(m_cYuvPredTempDMVRL0);
  m_cYuvPredTempDMVRL0 = nullptr;
  xFree(m_cYuvPredTempDMVRL1);
  m_cYuvPredTempDMVRL1 = nullptr;
  for (uint32_t ch = 0; ch < MAX_NUM_COMPONENT; ch++)
  {
    xFree(m_cRefSamplesDMVRL0[ch]);
    m_cRefSamplesDMVRL0[ch] = nullptr;
    xFree(m_cRefSamplesDMVRL1[ch]);
    m_cRefSamplesDMVRL1[ch] = nullptr;
  }
  m_IBCBuffer.destroy();
}

void InterPrediction::init( RdCost* pcRdCost, ChromaFormat chromaFormatIDC, const int ctuSize )
{
  m_pcRdCost = pcRdCost;


  // if it has been initialised before, but the chroma format has changed, release the memory and start again.
  if( m_acYuvPred[REF_PIC_LIST_0][COMPONENT_Y] != nullptr && m_currChromaFormat != chromaFormatIDC )
  {
    destroy();
  }

  m_currChromaFormat = chromaFormatIDC;
  if( m_acYuvPred[REF_PIC_LIST_0][COMPONENT_Y] == nullptr ) // check if first is null (in which case, nothing initialised yet)
  {
    for( uint32_t c = 0; c < MAX_NUM_COMPONENT; c++ )
    {
      int extWidth = MAX_CU_SIZE + (2 * BIO_EXTEND_SIZE + 2) + 16;
      int extHeight = MAX_CU_SIZE + (2 * BIO_EXTEND_SIZE + 2) + 1;
      extWidth = extWidth > (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION) + 16) ? extWidth : MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION) + 16;
      extHeight = extHeight > (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION) + 1) ? extHeight : MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION) + 1;
      for( uint32_t i = 0; i < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; i++ )
      {
        m_filteredBlockTmp[i][c] = ( Pel* ) xMalloc( Pel, ( extWidth + 4 ) * ( extHeight + 7 + 4 ) );

        for( uint32_t j = 0; j < LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL; j++ )
        {
          m_filteredBlock[i][j][c] = ( Pel* ) xMalloc( Pel, extWidth * extHeight );
        }
      }

      // new structure
      for( uint32_t i = 0; i < NUM_REF_PIC_LIST_01; i++ )
      {
        m_acYuvPred[i][c] = ( Pel* ) xMalloc( Pel, MAX_CU_SIZE * MAX_CU_SIZE );
      }
    }

    m_triangleBuf.create(UnitArea(chromaFormatIDC, Area(0, 0, MAX_CU_SIZE, MAX_CU_SIZE)));

    m_iRefListIdx = -1;

    m_gradX0 = (Pel*)xMalloc(Pel, BIO_TEMP_BUFFER_SIZE);
    m_gradY0 = (Pel*)xMalloc(Pel, BIO_TEMP_BUFFER_SIZE);
    m_gradX1 = (Pel*)xMalloc(Pel, BIO_TEMP_BUFFER_SIZE);
    m_gradY1 = (Pel*)xMalloc(Pel, BIO_TEMP_BUFFER_SIZE);
  }

  if (m_cYuvPredTempDMVRL0 == nullptr && m_cYuvPredTempDMVRL1 == nullptr)
  {
    m_cYuvPredTempDMVRL0 = (Pel*)xMalloc(Pel, (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION)) * (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION)));
    m_cYuvPredTempDMVRL1 = (Pel*)xMalloc(Pel, (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION)) * (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION)));
    for (uint32_t ch = 0; ch < MAX_NUM_COMPONENT; ch++)
    {
      m_cRefSamplesDMVRL0[ch] = (Pel*)xMalloc(Pel, (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION) + NTAPS_LUMA) * (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION) + NTAPS_LUMA));
      m_cRefSamplesDMVRL1[ch] = (Pel*)xMalloc(Pel, (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION) + NTAPS_LUMA) * (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION) + NTAPS_LUMA));
    }
  }
#if !JVET_J0090_MEMORY_BANDWITH_MEASURE
  m_if.initInterpolationFilter( true );
#endif

  if (m_storedMv == nullptr)
  {
    const int MVBUFFER_SIZE = MAX_CU_SIZE / MIN_PU_SIZE;
    m_storedMv = new Mv[MVBUFFER_SIZE*MVBUFFER_SIZE];
  }
  if (m_IBCBuffer.bufs.empty())
  {
    m_IBCBufferWidth = 128 * 128 / ctuSize;
    m_IBCBuffer.create(UnitArea(chromaFormatIDC, Area(0, 0, m_IBCBufferWidth, ctuSize)));
  }
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

bool InterPrediction::xCheckIdenticalMotion( const PredictionUnit &pu )
{
  const Slice &slice = *pu.cs->slice;

  if( slice.isInterB() && !pu.cs->pps->getWPBiPred() )
  {
    if( pu.refIdx[0] >= 0 && pu.refIdx[1] >= 0 )
    {
      int RefPOCL0 = slice.getRefPic( REF_PIC_LIST_0, pu.refIdx[0] )->getPOC();
      int RefPOCL1 = slice.getRefPic( REF_PIC_LIST_1, pu.refIdx[1] )->getPOC();

      if( RefPOCL0 == RefPOCL1 )
      {
        if( !pu.cu->affine )
        {
          if( pu.mv[0] == pu.mv[1] )
          {
            return true;
          }
        }
        else
        {
          if ( (pu.cu->affineType == AFFINEMODEL_4PARAM && (pu.mvAffi[0][0] == pu.mvAffi[1][0]) && (pu.mvAffi[0][1] == pu.mvAffi[1][1]))
            || (pu.cu->affineType == AFFINEMODEL_6PARAM && (pu.mvAffi[0][0] == pu.mvAffi[1][0]) && (pu.mvAffi[0][1] == pu.mvAffi[1][1]) && (pu.mvAffi[0][2] == pu.mvAffi[1][2])) )
          {
            return true;
          }
        }
      }
    }
  }

  return false;
}

void InterPrediction::xSubPuMC( PredictionUnit& pu, PelUnitBuf& predBuf, const RefPicList &eRefPicList /*= REF_PIC_LIST_X*/ )
{

  // compute the location of the current PU
  Position puPos    = pu.lumaPos();
  Size puSize       = pu.lumaSize();

  int numPartLine, numPartCol, puHeight, puWidth;
  {
    numPartLine = std::max(puSize.width >> ATMVP_SUB_BLOCK_SIZE, 1u);
    numPartCol = std::max(puSize.height >> ATMVP_SUB_BLOCK_SIZE, 1u);
    puHeight = numPartCol == 1 ? puSize.height : 1 << ATMVP_SUB_BLOCK_SIZE;
    puWidth = numPartLine == 1 ? puSize.width : 1 << ATMVP_SUB_BLOCK_SIZE;
  }

  PredictionUnit subPu;

  subPu.cs        = pu.cs;
  subPu.cu        = pu.cu;
  subPu.mergeType = MRG_TYPE_DEFAULT_N;

  bool isAffine = pu.cu->affine;
  subPu.cu->affine = false;

  // join sub-pus containing the same motion
  bool verMC = puSize.height > puSize.width;
  int  fstStart = (!verMC ? puPos.y : puPos.x);
  int  secStart = (!verMC ? puPos.x : puPos.y);
  int  fstEnd = (!verMC ? puPos.y + puSize.height : puPos.x + puSize.width);
  int  secEnd = (!verMC ? puPos.x + puSize.width : puPos.y + puSize.height);
  int  fstStep = (!verMC ? puHeight : puWidth);
  int  secStep = (!verMC ? puWidth : puHeight);

  m_subPuMC = true;

  for (int fstDim = fstStart; fstDim < fstEnd; fstDim += fstStep)
  {
    for (int secDim = secStart; secDim < secEnd; secDim += secStep)
    {
      int x = !verMC ? secDim : fstDim;
      int y = !verMC ? fstDim : secDim;
      const MotionInfo &curMi = pu.getMotionInfo(Position{ x, y });

      int length = secStep;
      int later  = secDim + secStep;

      while (later < secEnd)
      {
        const MotionInfo &laterMi = !verMC ? pu.getMotionInfo(Position{ later, fstDim }) : pu.getMotionInfo(Position{ fstDim, later });
        if (laterMi == curMi)
        {
          length += secStep;
        }
        else
        {
          break;
        }
        later += secStep;
      }
      int dx = !verMC ? length : puWidth;
      int dy = !verMC ? puHeight : length;

      subPu.UnitArea::operator=(UnitArea(pu.chromaFormat, Area(x, y, dx, dy)));
      subPu = curMi;
      PelUnitBuf subPredBuf = predBuf.subBuf(UnitAreaRelative(pu, subPu));
      subPu.mmvdEncOptMode = 0;
      subPu.mvRefine = false;
      motionCompensation(subPu, subPredBuf, eRefPicList);
      secDim = later - secStep;
    }
  }
  m_subPuMC = false;

  pu.cu->affine = isAffine;
}
void InterPrediction::xSubPuBio(PredictionUnit& pu, PelUnitBuf& predBuf, const RefPicList &eRefPicList /*= REF_PIC_LIST_X*/, PelUnitBuf* yuvDstTmp /*= NULL*/)
{
  auto start = high_resolution_clock::now();
  // compute the location of the current PU
  Position puPos = pu.lumaPos();
  Size puSize = pu.lumaSize();

  PredictionUnit subPu;

  subPu.cs = pu.cs;
  subPu.cu = pu.cu;
  subPu.mergeType = pu.mergeType;
  subPu.mmvdMergeFlag = pu.mmvdMergeFlag;
  subPu.mmvdEncOptMode = pu.mmvdEncOptMode;
  subPu.mergeFlag = pu.mergeFlag;
  subPu.mhIntraFlag = pu.mhIntraFlag;
  subPu.mvRefine = pu.mvRefine;
  subPu.refIdx[0] = pu.refIdx[0];
  subPu.refIdx[1] = pu.refIdx[1];
  int  fstStart = puPos.y;
  int  secStart = puPos.x;
  // dont use calculateSum
  int  fstEnd = puPos.y + puSize.height;
  int  secEnd = puPos.x + puSize.width;
  //int  secEnd  = calculateSum(puPos.x , puSize.width, 8 ,7);
  int  fstStep = std::min((int)MAX_BDOF_APPLICATION_REGION, (int)puSize.height);
  int  secStep = std::min((int)MAX_BDOF_APPLICATION_REGION, (int)puSize.width);
  // dont use calculateSum
  for (int fstDim = fstStart; fstDim < fstEnd; fstDim += fstStep)
  {
     // dont use calculateSum
    for (int secDim = secStart; secDim < secEnd; secDim += secStep)
    {
      int x = secDim;
      int y = fstDim;
      int dx = secStep;
      int dy = fstStep;

      const MotionInfo &curMi = pu.getMotionInfo(Position{ x, y });

      subPu.UnitArea::operator=(UnitArea(pu.chromaFormat, Area(x, y, dx, dy)));
      subPu = curMi;
      PelUnitBuf subPredBuf = predBuf.subBuf(UnitAreaRelative(pu, subPu));

      if (yuvDstTmp)
      {
        PelUnitBuf subPredBufTmp = yuvDstTmp->subBuf(UnitAreaRelative(pu, subPu));
        motionCompensation(subPu, subPredBuf, eRefPicList, true, true, &subPredBufTmp);
      }
      else
      motionCompensation(subPu, subPredBuf, eRefPicList);
    }
  }
  auto stop                = high_resolution_clock::now();
  auto duration            = duration_cast<nanoseconds>(stop - start);
  timeOfXSubPuBio          = timeOfXSubPuBio + duration.count();
}

void InterPrediction::xPredInterUni(const PredictionUnit& pu, const RefPicList& eRefPicList, PelUnitBuf& pcYuvPred, const bool& bi
                                   , const bool& bioApplied
                                   , const bool luma, const bool chroma
)
{
  const SPS &sps = *pu.cs->sps;

  int iRefIdx = pu.refIdx[eRefPicList];
  Mv mv[3];
  bool isIBC = false;
  CHECK( !CU::isIBC( *pu.cu ) && pu.lwidth() == 4 && pu.lheight() == 4, "invalid 4x4 inter blocks" );
  if (CU::isIBC(*pu.cu))
  {
    isIBC = true;
  }
  if( pu.cu->affine )
  {
    CHECK( iRefIdx < 0, "iRefIdx incorrect." );

    mv[0] = pu.mvAffi[eRefPicList][0];
    mv[1] = pu.mvAffi[eRefPicList][1];
    mv[2] = pu.mvAffi[eRefPicList][2];
  }
  else
  {
    mv[0] = pu.mv[eRefPicList];
  }

  if( !pu.cu->affine )
  {
    clipMv( mv[0], pu.cu->lumaPos(), pu.cu->lumaSize(), sps, *pu.cs->pps );
  }

  for( uint32_t comp = COMPONENT_Y; comp < pcYuvPred.bufs.size() && comp <= m_maxCompIDToPred; comp++ )
  {
    const ComponentID compID = ComponentID( comp );
    if (compID == COMPONENT_Y && !luma)
      continue;
    if (compID != COMPONENT_Y && !chroma)
      continue;
    if ( pu.cu->affine )
    {
      CHECK( bioApplied, "BIO is not allowed with affine" );
      m_iRefListIdx = eRefPicList;
      xPredAffineBlk( compID, pu, pu.cu->slice->getRefPic( eRefPicList, iRefIdx )->unscaledPic, mv, pcYuvPred, bi, pu.cu->slice->clpRng( compID ), pu.cu->slice->getScalingRatio( eRefPicList, iRefIdx ));
    }
    else
    {
      if (isIBC)
      {
        xPredInterBlk(compID, pu, pu.cu->slice->getPic(), mv[0], pcYuvPred, bi, pu.cu->slice->clpRng(compID)
          , bioApplied
          , isIBC
        );
      }
      else
      {
        xPredInterBlk( compID, pu, pu.cu->slice->getRefPic( eRefPicList, iRefIdx )->unscaledPic, mv[0], pcYuvPred, bi, pu.cu->slice->clpRng( compID ), bioApplied, isIBC, pu.cu->slice->getScalingRatio( eRefPicList, iRefIdx ) );
      }
    }
  }
}

void InterPrediction::xPredInterBi(PredictionUnit& pu, PelUnitBuf &pcYuvPred, PelUnitBuf* yuvPredTmp /*= NULL*/)
{
  const PPS   &pps   = *pu.cs->pps;
  const Slice &slice = *pu.cs->slice;
  CHECK( !pu.cu->affine && pu.refIdx[0] >= 0 && pu.refIdx[1] >= 0 && ( pu.lwidth() + pu.lheight() == 12 ), "invalid 4x8/8x4 bi-predicted blocks" );
  WPScalingParam *wp0;
  WPScalingParam *wp1;
  int refIdx0 = pu.refIdx[REF_PIC_LIST_0];
  int refIdx1 = pu.refIdx[REF_PIC_LIST_1];
  pu.cs->slice->getWpScaling(REF_PIC_LIST_0, refIdx0, wp0);
  pu.cs->slice->getWpScaling(REF_PIC_LIST_1, refIdx1, wp1);

  bool bioApplied = false;
  if (pu.cs->sps->getBDOFEnabledFlag() && (!pu.cs->slice->getDisBdofDmvrFlag()))
  {
    if (pu.cu->affine || m_subPuMC)
    {
      bioApplied = false;
    }
    else
    {
      const bool biocheck0 = !((wp0[COMPONENT_Y].bPresentFlag || wp1[COMPONENT_Y].bPresentFlag) && slice.getSliceType() == B_SLICE);
      const bool biocheck1 = !(pps.getUseWP() && slice.getSliceType() == P_SLICE);
      if (biocheck0
        && biocheck1
        && PU::isBiPredFromDifferentDir(pu)
        && (pu.Y().height >= 8)
        && (pu.Y().width >= 8)
        && ((pu.Y().height * pu.Y().width) >= 128)
       )
      {
        bioApplied = true;
      }
    }

    if (bioApplied && pu.mhIntraFlag)
      bioApplied = false;

    if (bioApplied && pu.cu->smvdMode)
    {
      bioApplied = false;
    }

    if (pu.cu->cs->sps->getUseGBi() && bioApplied && pu.cu->GBiIdx != GBI_DEFAULT)
    {
      bioApplied = false;
    }
  }
  if (pu.mmvdEncOptMode == 2 && pu.mmvdMergeFlag) {
    bioApplied = false;
  }
  bool dmvrApplied = false;
  dmvrApplied = (pu.mvRefine) && PU::checkDMVRCondition(pu);

  bool samePicSize = PU::isRefPicSameSize( pu );
  dmvrApplied = dmvrApplied && samePicSize;
  bioApplied = bioApplied && samePicSize;

  for (uint32_t refList = 0; refList < NUM_REF_PIC_LIST_01; refList++)
  {
    if( pu.refIdx[refList] < 0)
    {
      continue;
    }

    RefPicList eRefPicList = (refList ? REF_PIC_LIST_1 : REF_PIC_LIST_0);

    CHECK(CU::isIBC(*pu.cu) && eRefPicList != REF_PIC_LIST_0, "Invalid interdir for ibc mode");
    CHECK(CU::isIBC(*pu.cu) && pu.refIdx[refList] != MAX_NUM_REF, "Invalid reference index for ibc mode");
    CHECK((CU::isInter(*pu.cu) && pu.refIdx[refList] >= slice.getNumRefIdx(eRefPicList)), "Invalid reference index");
    m_iRefListIdx = refList;

    PelUnitBuf pcMbBuf = ( pu.chromaFormat == CHROMA_400 ?
                           PelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[refList][0], pcYuvPred.Y())) :
                           PelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[refList][0], pcYuvPred.Y()), PelBuf(m_acYuvPred[refList][1], pcYuvPred.Cb()), PelBuf(m_acYuvPred[refList][2], pcYuvPred.Cr())) );

    if (pu.refIdx[0] >= 0 && pu.refIdx[1] >= 0)
    {
      if (dmvrApplied)
      {
        if (yuvPredTmp)
          xPredInterUni(pu, eRefPicList, pcMbBuf, true, false, true, true);
        continue;
      }
      xPredInterUni ( pu, eRefPicList, pcMbBuf, true
        , bioApplied
        , true, true
      );
    }
    else
    {
      if( ( (pps.getUseWP() && slice.getSliceType() == P_SLICE) || (pps.getWPBiPred() && slice.getSliceType() == B_SLICE) ) )
      {
        xPredInterUni ( pu, eRefPicList, pcMbBuf, true
          , bioApplied
          , true, true
        );
      }
      else
      {
        xPredInterUni( pu, eRefPicList, pcMbBuf, pu.cu->triangle
          , bioApplied
          , true, true
        );
      }
    }
  }
  CPelUnitBuf srcPred0 = ( pu.chromaFormat == CHROMA_400 ?
                           CPelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[0][0], pcYuvPred.Y())) :
                           CPelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[0][0], pcYuvPred.Y()), PelBuf(m_acYuvPred[0][1], pcYuvPred.Cb()), PelBuf(m_acYuvPred[0][2], pcYuvPred.Cr())) );
  CPelUnitBuf srcPred1 = ( pu.chromaFormat == CHROMA_400 ?
                           CPelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[1][0], pcYuvPred.Y())) :
                           CPelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[1][0], pcYuvPred.Y()), PelBuf(m_acYuvPred[1][1], pcYuvPred.Cb()), PelBuf(m_acYuvPred[1][2], pcYuvPred.Cr())) );
  if( !pu.cu->triangle && (!dmvrApplied) && (!bioApplied) && pps.getWPBiPred() && slice.getSliceType() == B_SLICE && pu.cu->GBiIdx==GBI_DEFAULT)
  {
    xWeightedPredictionBi( pu, srcPred0, srcPred1, pcYuvPred, m_maxCompIDToPred );
    if (yuvPredTmp)
      yuvPredTmp->copyFrom(pcYuvPred);
  }
  else if( !pu.cu->triangle && pps.getUseWP() && slice.getSliceType() == P_SLICE )
  {
    xWeightedPredictionUni( pu, srcPred0, REF_PIC_LIST_0, pcYuvPred, -1, m_maxCompIDToPred );
    if (yuvPredTmp)
      yuvPredTmp->copyFrom(pcYuvPred);
  }
  else
  {
    if (dmvrApplied)
    {
      if (yuvPredTmp)
      {
        yuvPredTmp->addAvg(srcPred0, srcPred1, slice.clpRngs(), false);
      }
      xProcessDMVR(pu, pcYuvPred, slice.clpRngs(), bioApplied);
    }
    else
    {
      xWeightedAverage( pu, srcPred0, srcPred1, pcYuvPred, slice.getSPS()->getBitDepths(), slice.clpRngs(), bioApplied, yuvPredTmp);
    }
  }
}

void InterPrediction::xPredInterBlk ( const ComponentID& compID, const PredictionUnit& pu, const Picture* refPic, const Mv& _mv, PelUnitBuf& dstPic, const bool& bi, const ClpRng& clpRng
                                     , const bool& bioApplied
                                     , bool isIBC
                                     , const std::pair<int, int> scalingRatio
                                     , SizeType dmvrWidth
                                     , SizeType dmvrHeight
                                     , bool bilinearMC
                                     , Pel *srcPadBuf
                                     , int32_t srcPadStride
                                    )
{
  auto start = high_resolution_clock::now();
  JVET_J0090_SET_REF_PICTURE( refPic, compID );
  const ChromaFormat  chFmt = pu.chromaFormat;
  const bool          rndRes = !bi;

  int shiftHor = MV_FRACTIONAL_BITS_INTERNAL + ::getComponentScaleX(compID, chFmt);
  int shiftVer = MV_FRACTIONAL_BITS_INTERNAL + ::getComponentScaleY(compID, chFmt);

  bool  wrapRef = false;
  Mv    mv(_mv);
  if( !isIBC && pu.cs->sps->getWrapAroundEnabledFlag() )
  {
    wrapRef = wrapClipMv( mv, pu.blocks[0].pos(), pu.blocks[0].size(), pu.cs->sps, pu.cs->pps );
  }

  bool useAltHpelIf = pu.cu->imv == IMV_HPEL;

  if( !isIBC && xPredInterBlkRPR( scalingRatio, *pu.cs->pps, CompArea( compID, chFmt, pu.blocks[compID], Size( dstPic.bufs[compID].width, dstPic.bufs[compID].height ) ), refPic, mv, dstPic.bufs[compID].buf, dstPic.bufs[compID].stride, bi, wrapRef, clpRng, 0, useAltHpelIf ) )
  {
    CHECK( bilinearMC, "DMVR should be disabled with RPR" );
    CHECK( bioApplied, "BDOF should be disabled with RPR" );
  }
  else
  {
  int xFrac = mv.hor & ((1 << shiftHor) - 1);
  int yFrac = mv.ver & ((1 << shiftVer) - 1);
  if (isIBC)
  {
    xFrac = yFrac = 0;
    JVET_J0090_SET_CACHE_ENABLE( false );
  }

  PelBuf &dstBuf  = dstPic.bufs[compID];
  unsigned width  = dstBuf.width;
  unsigned height = dstBuf.height;

  CPelBuf refBuf;
  {
    Position offset = pu.blocks[compID].pos().offset( mv.getHor() >> shiftHor, mv.getVer() >> shiftVer );
    if (dmvrWidth)
    {
      refBuf = refPic->getRecoBuf(CompArea(compID, chFmt, offset, Size(dmvrWidth, dmvrHeight)), wrapRef);
    }
    else
    refBuf = refPic->getRecoBuf( CompArea( compID, chFmt, offset, pu.blocks[compID].size() ), wrapRef);
  }

  if (NULL != srcPadBuf)
  {
    refBuf.buf = srcPadBuf;
    refBuf.stride = srcPadStride;
  }
  if (dmvrWidth)
  {
    width = dmvrWidth;
    height = dmvrHeight;
  }
  // backup data
  int backupWidth = width;
  int backupHeight = height;
  Pel *backupDstBufPtr = dstBuf.buf;
  int backupDstBufStride = dstBuf.stride;

  if (bioApplied && compID == COMPONENT_Y)
  {
    width = width + 2 * BIO_EXTEND_SIZE + 2;
    height = height + 2 * BIO_EXTEND_SIZE + 2;

    // change MC output
    dstBuf.stride = width;
    dstBuf.buf = m_filteredBlockTmp[2 + m_iRefListIdx][compID] + 2 * dstBuf.stride + 2;
  }


  if( yFrac == 0 )
  {
    m_if.filterHor(compID, (Pel*)refBuf.buf, refBuf.stride, dstBuf.buf, dstBuf.stride, backupWidth, backupHeight, xFrac, rndRes, chFmt, clpRng, bilinearMC, bilinearMC, useAltHpelIf);
  }
  else if( xFrac == 0 )
  {
    m_if.filterVer(compID, (Pel*)refBuf.buf, refBuf.stride, dstBuf.buf, dstBuf.stride, backupWidth, backupHeight, yFrac, true, rndRes, chFmt, clpRng, bilinearMC, bilinearMC, useAltHpelIf);
  }
  else
  {
    PelBuf tmpBuf = dmvrWidth ? PelBuf(m_filteredBlockTmp[0][compID], Size(dmvrWidth, dmvrHeight)) : PelBuf(m_filteredBlockTmp[0][compID], pu.blocks[compID]);
    if (dmvrWidth == 0)
      tmpBuf.stride = dstBuf.stride;

    int vFilterSize = isLuma(compID) ? NTAPS_LUMA : NTAPS_CHROMA;
    if (bilinearMC)
    {
      vFilterSize = NTAPS_BILINEAR;
    }
    m_if.filterHor(compID, (Pel*)refBuf.buf - ((vFilterSize >> 1) - 1) * refBuf.stride, refBuf.stride, tmpBuf.buf, tmpBuf.stride, backupWidth, backupHeight + vFilterSize - 1, xFrac, false, chFmt, clpRng, bilinearMC, bilinearMC, useAltHpelIf);
    JVET_J0090_SET_CACHE_ENABLE( false );
    m_if.filterVer(compID, (Pel*)tmpBuf.buf + ((vFilterSize >> 1) - 1) * tmpBuf.stride, tmpBuf.stride, dstBuf.buf, dstBuf.stride, backupWidth, backupHeight, yFrac, false, rndRes, chFmt, clpRng, bilinearMC, bilinearMC, useAltHpelIf);
  }
  JVET_J0090_SET_CACHE_ENABLE( srcPadStride == 0 ); // Enabled only in non-DMVR process, In DMVR process, srcPadStride is always non-zero
  if (bioApplied && compID == COMPONENT_Y)
  {
    const int shift = std::max<int>(2, (IF_INTERNAL_PREC - clpRng.bd));
    int xOffset = (xFrac < 8) ? 1 : 0;
    int yOffset = (yFrac < 8) ? 1 : 0;
    const Pel* refPel = refBuf.buf - yOffset * refBuf.stride - xOffset;
    Pel* dstPel = m_filteredBlockTmp[2 + m_iRefListIdx][compID] + dstBuf.stride + 1;
    for (int w = 0; w < (width - 2 * BIO_EXTEND_SIZE); w++)
    {
      Pel val = leftShift_round(refPel[w], shift);
      dstPel[w] = val - (Pel)IF_INTERNAL_OFFS;
    }

    refPel = refBuf.buf + (1 - yOffset)*refBuf.stride - xOffset;
    dstPel = m_filteredBlockTmp[2 + m_iRefListIdx][compID] + 2 * dstBuf.stride + 1;
    for (int h = 0; h < (height - 2 * BIO_EXTEND_SIZE - 2); h++)
    {
      Pel val = leftShift_round(refPel[0], shift);
      dstPel[0] = val - (Pel)IF_INTERNAL_OFFS;

      val = leftShift_round(refPel[width - 3], shift);
      dstPel[width - 3] = val - (Pel)IF_INTERNAL_OFFS;

      refPel += refBuf.stride;
      dstPel += dstBuf.stride;
    }

    refPel = refBuf.buf + (height - 2 * BIO_EXTEND_SIZE - 2 + 1 - yOffset)*refBuf.stride - xOffset;
    dstPel = m_filteredBlockTmp[2 + m_iRefListIdx][compID] + (height - 2 * BIO_EXTEND_SIZE)*dstBuf.stride + 1;
    for (int w = 0; w < (width - 2 * BIO_EXTEND_SIZE); w++)
    {
      Pel val = leftShift_round(refPel[w], shift);
      dstPel[w] = val - (Pel)IF_INTERNAL_OFFS;
    }

    // restore data
    width = backupWidth;
    height = backupHeight;
    dstBuf.buf = backupDstBufPtr;
    dstBuf.stride = backupDstBufStride;
  }
  }
  auto stop                    = high_resolution_clock::now();
  auto duration                = duration_cast<nanoseconds>(stop - start);
  timeOfXPredInterBlk          = timeOfXPredInterBlk + duration.count();
}

bool InterPrediction::isSubblockVectorSpreadOverLimit( int a, int b, int c, int d, int predType )
{
  auto start     = high_resolution_clock::now();
  int s4 = ( 4 << 11 );
  int filterTap = 6;

  if ( predType == 3 )
  {
    int refBlkWidth  = std::max( std::max( 0, 4 * a + s4 ), std::max( 4 * c, 4 * a + 4 * c + s4 ) ) - std::min( std::min( 0, 4 * a + s4 ), std::min( 4 * c, 4 * a + 4 * c + s4 ) );
    //int refBlkWidth  = calculateSum(std::max( std::max( 0, calculateSum(4 * a , s4,8, 7) ), std::max( 4 * c, calculateSum(calculateSum(4 * a , 4 * c,8, 7) , s4,8, 7) ) ) , -std::min( std::min( 0, calculateSum(4 * a , s4,8, 7) ), std::min( 4 * c, calculateSum(calculateSum(4 * a , 4 * c,8, 7) , s4,8, 7) ) ),8, 7);
    
    int refBlkHeight = std::max( std::max( 0, 4 * b ), std::max( 4 * d + s4, 4 * b + 4 * d + s4 ) ) - std::min( std::min( 0, 4 * b ), std::min( 4 * d + s4, 4 * b + 4 * d + s4 ) );
    //int refBlkHeight = std::max( std::max( 0, 4 * b ), std::max( calculateSum(4 * d , s4,8, 7), calculateSum(calculateSum(4 * b , 4 * d,8, 7) , s4,8, 7) ) ) - std::min( std::min( 0, 4 * b ), std::min( calculateSum(4 * d , s4,8, 7), calculateSum(calculateSum(4 * b , 4 * d,8, 7) , s4, 16 ,12) ) );
   
    refBlkWidth  = ( refBlkWidth >> 11 ) + filterTap + 3;
    //refBlkWidth = calculateSum((refBlkWidth >> 11) , filterTap + 3,8, 7);

    refBlkHeight = ( refBlkHeight >> 11 ) + filterTap + 3;
    //refBlkHeight = calculateSum((refBlkHeight >> 11) , filterTap + 3,8, 7);

    if ( refBlkWidth * refBlkHeight > ( filterTap + 9 ) * ( filterTap + 9 ) )
    {
      auto stop                = high_resolution_clock::now();
      auto duration            = duration_cast<nanoseconds>(stop - start);
      timeIsSubblockVectorSpreadOverLimit = timeIsSubblockVectorSpreadOverLimit + duration.count();
      return true;
    }
  }
  else
  {
    int refBlkWidth = std::max(0, 4 * a + s4) - std::min(0, 4 * a + s4);
    //int refBlkWidth = calculateSum(std::max(0, calculateSum(4 * a, s4,8, 7)) ,-std::min(0, calculateSum(4 * a , s4,8, 7)),8, 7);
    

    int refBlkHeight = std::max( 0, 4 * b ) - std::min( 0, 4 * b );
    //int refBlkHeight = calculateSum(std::max(0, 4 * b) ,-std::min(0, 4 * b),8, 7);

    refBlkWidth  = ( refBlkWidth >> 11 ) + filterTap + 3;
    //refBlkWidth = calculateSum((refBlkWidth >> 11) , filterTap + 3,8, 7);

    refBlkHeight = ( refBlkHeight >> 11 ) + filterTap + 3;
    //refBlkHeight = calculateSum((refBlkHeight >> 11) , filterTap + 3,8, 7);

    if ( refBlkWidth * refBlkHeight > ( filterTap + 9 ) * ( filterTap + 5 ) )
    {
      auto stop                           = high_resolution_clock::now();
      auto duration                       = duration_cast<nanoseconds>(stop - start);
      timeIsSubblockVectorSpreadOverLimit = timeIsSubblockVectorSpreadOverLimit + duration.count();
      return true;
    }

    refBlkWidth  = std::max( 0, 4 * c ) - std::min( 0, 4 * c );
    //refBlkWidth = calculateSum(std::max(0, 4 * c) , -std::min(0, 4 * c),8, 7);

    refBlkHeight = std::max( 0, 4 * d + s4 ) - std::min( 0, 4 * d + s4 );
    //refBlkHeight = calculateSum(std::max(0, calculateSum(4 * d, s4,8, 7)) , -std::min(0, calculateSum(4 * d , s4,8, 7)),8, 7);

    refBlkWidth  = ( refBlkWidth >> 11 ) + filterTap + 3;
    //refBlkWidth = calculateSum((refBlkWidth >> 11) , filterTap + 3, 8, 7);

    refBlkHeight = ( refBlkHeight >> 11 ) + filterTap + 3;
    //refBlkHeight = calculateSum((refBlkHeight >> 11) , filterTap + 3, 8, 7);

    if ( refBlkWidth * refBlkHeight > ( filterTap + 5 ) * ( filterTap + 9 ) )
    {
      auto stop                           = high_resolution_clock::now();
      auto duration                       = duration_cast<nanoseconds>(stop - start);
      timeIsSubblockVectorSpreadOverLimit = timeIsSubblockVectorSpreadOverLimit + duration.count();
      return true;
    }
  }
  auto stop                           = high_resolution_clock::now();
  auto duration                       = duration_cast<nanoseconds>(stop - start);
  timeIsSubblockVectorSpreadOverLimit = timeIsSubblockVectorSpreadOverLimit + duration.count();
  return false;
}



void InterPrediction::xPredAffineBlk( const ComponentID& compID, const PredictionUnit& pu, const Picture* refPic, const Mv* _mv, PelUnitBuf& dstPic, const bool& bi, const ClpRng& clpRng, const std::pair<int, int> scalingRatio )
{
  auto start = high_resolution_clock::now();
  JVET_J0090_SET_REF_PICTURE( refPic, compID );
  const ChromaFormat chFmt = pu.chromaFormat;
  int iScaleX = ::getComponentScaleX( compID, chFmt );
  int iScaleY = ::getComponentScaleY( compID, chFmt );

  Mv mvLT =_mv[0];
  Mv mvRT =_mv[1];
  Mv mvLB =_mv[2];


  // get affine sub-block width and height
  const int width  = pu.Y().width;
  const int height = pu.Y().height;
  int blockWidth = AFFINE_MIN_BLOCK_SIZE;
  int blockHeight = AFFINE_MIN_BLOCK_SIZE;

  CHECK(blockWidth  > (width >> iScaleX ), "Sub Block width  > Block width");
  CHECK(blockHeight > (height >> iScaleY), "Sub Block height > Block height");
  const int MVBUFFER_SIZE = MAX_CU_SIZE / MIN_PU_SIZE;

  const int cxWidth  = width  >> iScaleX;
  const int cxHeight = height >> iScaleY;
  const int iHalfBW  = blockWidth  >> 1;
  const int iHalfBH  = blockHeight >> 1;

  const int iBit = MAX_CU_DEPTH;
  int iDMvHorX, iDMvHorY, iDMvVerX, iDMvVerY;
  iDMvHorX = (mvRT - mvLT).getHor() << (iBit - floorLog2(cxWidth));
  //iDMvHorX = (mvRT - mvLT).getHor() << (calculateSum(iBit , - floorLog2(cxWidth), 8, 7));
  iDMvHorY = (mvRT - mvLT).getVer() << (iBit - floorLog2(cxWidth));
  //iDMvHorY = (mvRT - mvLT).getVer() << (calculateSum(iBit , - floorLog2(cxWidth), 8, 7));
  if ( pu.cu->affineType == AFFINEMODEL_6PARAM )
  {
    iDMvVerX = (mvLB - mvLT).getHor() << (iBit - floorLog2(cxHeight));
    //iDMvVerX = (mvLB - mvLT).getHor() << (calculateSum(iBit, -floorLog2(cxHeight), 8, 7));
    iDMvVerY = (mvLB - mvLT).getVer() << (iBit - floorLog2(cxHeight));
    //iDMvVerY = (mvLB - mvLT).getVer() << (calculateSum(iBit, -floorLog2(cxHeight), 8, 7));
  }
  else
  {
    iDMvVerX = -iDMvHorY;
    iDMvVerY = iDMvHorX;
  }

  int iMvScaleHor = mvLT.getHor() << iBit;
  int iMvScaleVer = mvLT.getVer() << iBit;
  const SPS &sps    = *pu.cs->sps;
  const int iMvShift = 4;
  const int iOffset  = 8;

  const int iHorMax = ( pu.cs->pps->getPicWidthInLumaSamples() + iOffset - pu.Y().x - 1 ) << iMvShift;
  //const int iHorMax = (calculateSum(calculateSum((int)pu.cs->pps->getPicWidthInLumaSamples(), iOffset,8, 7) , -(int) pu.Y().x - 1,8, 7)) << iMvShift;

  const int iHorMin = (      -(int)pu.cs->pcv->maxCUWidth  - iOffset - (int)pu.Y().x + 1 ) << iMvShift;
  //const int iHorMin = (calculateSum(calculateSum(-(int) pu.cs->pcv->maxCUWidth, -iOffset,8, 7) , - (int) pu.Y().x + 1,8, 7)) << iMvShift;

  const int iVerMax = ( pu.cs->pps->getPicHeightInLumaSamples() + iOffset - pu.Y().y - 1 ) << iMvShift;
  //const int iVerMax = (calculateSum(calculateSum((int)pu.cs->pps->getPicHeightInLumaSamples() , iOffset,8, 7) , -(int) pu.Y().y - 1,8, 7)) << iMvShift;

  const int iVerMin = (-(int)pu.cs->pcv->maxCUHeight - iOffset - (int)pu.Y().y + 1 ) << iMvShift;
  //const int iVerMin = (calculateSum(calculateSum(-(int) pu.cs->pcv->maxCUHeight, -iOffset,8, 7) , - (int) pu.Y().y + 1,8, 7) ) << iMvShift;


  const int vFilterSize = isLuma(compID) ? NTAPS_LUMA : NTAPS_CHROMA;

  const int shift = iBit - 4 + MV_FRACTIONAL_BITS_INTERNAL;
  //const int shift = calculateSum(iBit - 4 , MV_FRACTIONAL_BITS_INTERNAL,8, 7);

  bool      wrapRef = false;
  const bool subblkMVSpreadOverLimit = isSubblockVectorSpreadOverLimit( iDMvHorX, iDMvHorY, iDMvVerX, iDMvVerY, pu.interDir );

  bool enablePROF = (sps.getUsePROF()) && (!m_skipPROF) && (compID == COMPONENT_Y);
  enablePROF &= !((pu.cu->affineType == AFFINEMODEL_6PARAM && _mv[0] == _mv[1] && _mv[0] == _mv[2]) || (pu.cu->affineType == AFFINEMODEL_4PARAM && _mv[0] == _mv[1]));
  enablePROF &= !subblkMVSpreadOverLimit;
  const int profThres = 1 << (iBit + (m_isBi ? 1 : 0));
  enablePROF &= !m_encOnly || pu.cu->slice->getCheckLDC() || iDMvHorX > profThres || iDMvHorY > profThres || iDMvVerX > profThres || iDMvVerY > profThres || iDMvHorX < -profThres || iDMvHorY < -profThres || iDMvVerX < -profThres || iDMvVerY < -profThres;
  enablePROF &= pu.cs->pps->getPicWidthInLumaSamples() == refPic->cs->pps->getPicWidthInLumaSamples() && pu.cs->pps->getPicHeightInLumaSamples() == refPic->cs->pps->getPicHeightInLumaSamples();

  if (compID == COMPONENT_Y)
  {
    m_applyPROF[m_iRefListIdx] = enablePROF;
  }

  bool isLast = enablePROF ? false : !bi;

  const int cuExtW = pu.blocks[compID].width + PROF_BORDER_EXT_W * 2;
  //const int cuExtW = calculateSum(pu.blocks[compID].width , PROF_BORDER_EXT_W * 2,8, 7);

  const int cuExtH = pu.blocks[compID].height + PROF_BORDER_EXT_H * 2;
  //const int cuExtH = calculateSum(pu.blocks[compID].height , PROF_BORDER_EXT_H * 2,8, 7);

  PelBuf gradXExt(m_gradBuf[m_iRefListIdx][0], cuExtW, cuExtH);
  PelBuf gradYExt(m_gradBuf[m_iRefListIdx][1], cuExtW, cuExtH);

  const int MAX_FILTER_SIZE = std::max<int>(NTAPS_LUMA, NTAPS_CHROMA);
  const int dstExtW = ((blockWidth + PROF_BORDER_EXT_W * 2 + 7) >> 3) << 3;
  //const int dstExtW = ((calculateSum(blockWidth , PROF_BORDER_EXT_W * 2,8, 7) + 7) >> 3) << 3;

  const int dstExtH = blockHeight + PROF_BORDER_EXT_H * 2;
  //const int dstExtH = calculateSum(blockHeight , PROF_BORDER_EXT_H * 2,8, 7);

  PelBuf dstExtBuf(m_filteredBlockTmp[1][compID], dstExtW, dstExtH);

  const int refExtH = dstExtH + MAX_FILTER_SIZE - 1;
  //const int refExtH = calculateSum(dstExtH , MAX_FILTER_SIZE,8, 7) - 1;

  PelBuf tmpBuf = PelBuf(m_filteredBlockTmp[0][compID], dstExtW, refExtH);

  PelBuf &dstBuf = dstPic.bufs[compID];

  int *dMvScaleHor = m_dMvBuf[m_iRefListIdx];
  int *dMvScaleVer = m_dMvBuf[m_iRefListIdx] + 16;

  if (enablePROF && !bi)
  {
    int* dMvH = dMvScaleHor;
    int* dMvV = dMvScaleVer;
    int quadHorX = iDMvHorX << 2;
    int quadHorY = iDMvHorY << 2;
    int quadVerX = iDMvVerX << 2;
    int quadVerY = iDMvVerY << 2;

    dMvH[0] = ((iDMvHorX + iDMvVerX) << 1) - ((quadHorX + quadVerX) << 1);
    //dMvH[0] = calculateSum(((iDMvHorX + iDMvVerX) << 1), - ((quadHorX + quadVerX) << 1), 8, 7);
    dMvV[0] = ((iDMvHorY + iDMvVerY) << 1) - ((quadHorY + quadVerY) << 1);
    //dMvV[0] = calculateSum(((iDMvHorY + iDMvVerY) << 1), - ((quadHorY + quadVerY) << 1), 8, 7);


    for (int w = 1; w < blockWidth; w++)
    {
      dMvH[w] = dMvH[w - 1] + quadHorX;
      //dMvH[w] = calculateSum(dMvH[w - 1] , quadHorX,8, 7);

      dMvV[w] = dMvV[w - 1] + quadHorY;
      //dMvV[w] = calculateSum(dMvV[w - 1] , quadHorY,8, 7);
    }
    

    dMvH += blockWidth;
    dMvV += blockWidth;
    for (int h = 1; h < blockHeight; h++)
    {
      for (int w = 0; w < blockWidth; w++)
      {
        
        dMvH[w] = dMvH[w - blockWidth] + quadVerX;
        //dMvH[w] = calculateSum(dMvH[w - blockWidth] , quadVerX,8, 7);

        dMvV[w] = dMvV[w - blockWidth] + quadVerY;
        //dMvV[w] = calculateSum(dMvV[w - blockWidth] , quadVerY,8, 7);
      }
      dMvH += blockWidth;
      dMvV += blockWidth;
    }
    const int bdlimit = std::max<int>(6, clpRng.bd - 6);
    const int dmvLimit = 1 << bdlimit;

    if (!g_pelBufOP.roundIntVector)
    {

      for (int idx = 0; idx < blockWidth * blockHeight; idx++)
      {
        roundAffineMv(dMvScaleHor[idx], dMvScaleVer[idx], shift);
        dMvScaleHor[idx] = Clip3(-dmvLimit, dmvLimit - 1, dMvScaleHor[idx]);
        dMvScaleVer[idx] = Clip3(-dmvLimit, dmvLimit - 1, dMvScaleVer[idx]);
      }
 
    }
    else
    {
      int sz = blockWidth * blockHeight;
      g_pelBufOP.roundIntVector(dMvScaleHor, sz, shift, dmvLimit);
      g_pelBufOP.roundIntVector(dMvScaleVer, sz, shift, dmvLimit);
    }
  }
  // get prediction block by block
 
  for ( int h = 0; h < cxHeight; h += blockHeight )
  {
    for ( int w = 0; w < cxWidth; w += blockWidth )
    {
      int iMvScaleTmpHor, iMvScaleTmpVer;
      if (compID == COMPONENT_Y || pu.chromaFormat == CHROMA_444) // time of this section is 17%
      { 
        // time of START (6%) ------------ 
        if ( !subblkMVSpreadOverLimit )
        { 
          // dont use calculateSum
          iMvScaleTmpHor = iMvScaleHor + iDMvHorX * (iHalfBW + w) + iDMvVerX * (iHalfBH + h);
          iMvScaleTmpVer = iMvScaleVer + iDMvHorY * (iHalfBW + w) + iDMvVerY * (iHalfBH + h);
          //iMvScaleTmpVer = calculateSum(calculateSum(iMvScaleVer, iDMvHorY * (calculateSum(iHalfBW, w,8, 7)),8, 7), iDMvVerY * (calculateSum(iHalfBH, h,8, 7)),8, 7);
        }
        else
        {
          // dont use calculateSum
          iMvScaleTmpHor = iMvScaleHor + iDMvHorX * ( cxWidth >> 1 ) + iDMvVerX * ( cxHeight >> 1 );
          iMvScaleTmpVer = iMvScaleVer + iDMvHorY * ( cxWidth >> 1 ) + iDMvVerY * ( cxHeight >> 1 );
          //iMvScaleTmpVer = calculateSum(calculateSum(iMvScaleVer, iDMvHorY * (cxWidth >> 1),8, 7), iDMvVerY * (cxHeight >> 1),8, 7);
        }
        // ------------ time of END (6%)
        roundAffineMv(iMvScaleTmpHor, iMvScaleTmpVer, shift);
        Mv tmpMv(iMvScaleTmpHor, iMvScaleTmpVer);
        tmpMv.clipToStorageBitDepth();
        iMvScaleTmpHor = tmpMv.getHor();
        iMvScaleTmpVer = tmpMv.getVer();
        // time of START (6%) ------------ 
        // clip and scale
        if (sps.getWrapAroundEnabledFlag())
        {
          m_storedMv[h / AFFINE_MIN_BLOCK_SIZE * MVBUFFER_SIZE + w / AFFINE_MIN_BLOCK_SIZE].set(iMvScaleTmpHor, iMvScaleTmpVer);
          //m_storedMv[h / AFFINE_MIN_BLOCK_SIZE * calculateSum(MVBUFFER_SIZE , w, 8, 7) / AFFINE_MIN_BLOCK_SIZE].set(iMvScaleTmpHor, iMvScaleTmpVer);
          Mv tmpMv(iMvScaleTmpHor, iMvScaleTmpVer);
          wrapRef = wrapClipMv( tmpMv, Position( pu.Y().x + w, pu.Y().y + h ), Size( blockWidth, blockHeight ), &sps, pu.cs->pps );
          iMvScaleTmpHor = tmpMv.getHor();
          iMvScaleTmpVer = tmpMv.getVer();
        }
        else
        {
          wrapRef = false;
          m_storedMv[h / AFFINE_MIN_BLOCK_SIZE * MVBUFFER_SIZE + w / AFFINE_MIN_BLOCK_SIZE].set(iMvScaleTmpHor, iMvScaleTmpVer);
          //m_storedMv[h / AFFINE_MIN_BLOCK_SIZE * calculateSum(MVBUFFER_SIZE, w, 8, 7) / AFFINE_MIN_BLOCK_SIZE].set(iMvScaleTmpHor, iMvScaleTmpVer);
          iMvScaleTmpHor = std::min<int>(iHorMax, std::max<int>(iHorMin, iMvScaleTmpHor));
          iMvScaleTmpVer = std::min<int>(iVerMax, std::max<int>(iVerMin, iMvScaleTmpVer));
        }
        // ------------ time of END (6%) 
      }
      else // time of this section is 0
      {
        // dont use calculateSum
        Mv curMv = m_storedMv[((h << iScaleY) / AFFINE_MIN_BLOCK_SIZE) * MVBUFFER_SIZE + ((w << iScaleX) / AFFINE_MIN_BLOCK_SIZE)] + m_storedMv[((h << iScaleY) / AFFINE_MIN_BLOCK_SIZE + iScaleY)* MVBUFFER_SIZE + ((w << iScaleX) / AFFINE_MIN_BLOCK_SIZE + iScaleX)];
        if (sps.getWrapAroundEnabledFlag())
        {
          wrapRef = wrapClipMv( curMv, Position( pu.Y().x + ( w << iScaleX ), pu.Y().y + ( h << iScaleY ) ), Size( blockWidth << iScaleX, blockHeight << iScaleY ), &sps, pu.cs->pps );
        }
        else
        {
          wrapRef = false;
          curMv.hor = std::min<int>(iHorMax, std::max<int>(iHorMin, curMv.hor));
          curMv.ver = std::min<int>(iVerMax, std::max<int>(iVerMin, curMv.ver));
        }
        iMvScaleTmpHor = curMv.hor;
        iMvScaleTmpVer = curMv.ver;
      }

      if( xPredInterBlkRPR( scalingRatio, *pu.cs->pps, CompArea( compID, chFmt, pu.blocks[compID].offset( w, h ), Size( blockWidth, blockHeight ) ), refPic, Mv( iMvScaleTmpHor, iMvScaleTmpVer ), dstBuf.buf + w + h * dstBuf.stride, dstBuf.stride, bi, wrapRef, clpRng, 2 ) )
      {
        CHECK( enablePROF, "PROF should be disabled with RPR" );
      }
      else // time of this section is 66%
      {
      // get the MV in high precision
      // time of START (2%) ------------ 
      int xFrac, yFrac, xInt, yInt;

      if (!iScaleX)
      {
        xInt  = iMvScaleTmpHor >> 4;
        xFrac = iMvScaleTmpHor & 15;
      }
      else
      {
        xInt  = iMvScaleTmpHor >> 5;
        xFrac = iMvScaleTmpHor & 31;
      }
      if (!iScaleY)
      {
        yInt  = iMvScaleTmpVer >> 4;
        yFrac = iMvScaleTmpVer & 15;
      }
      else
      {
        yInt  = iMvScaleTmpVer >> 5;
        yFrac = iMvScaleTmpVer & 31;
      }
      // dont use approximate
      const CPelBuf refBuf = refPic->getRecoBuf( CompArea( compID, chFmt, pu.blocks[compID].offset(xInt + w, yInt + h), pu.blocks[compID] ), wrapRef );
      //const CPelBuf refBuf = refPic->getRecoBuf( CompArea( compID, chFmt, pu.blocks[compID].offset(calculateSum(xInt , w, 8, 7), calculateSum(yInt , h, 8, 7)), pu.blocks[compID] ), wrapRef );

      Pel* ref = (Pel*) refBuf.buf;
      // dont use calculateSum
      Pel* dst = dstBuf.buf + w + h * dstBuf.stride;

      int refStride = refBuf.stride;
      int dstStride = dstBuf.stride;

      int bw = blockWidth;
      int bh = blockHeight;

      if (enablePROF)
      {
        dst = dstExtBuf.bufAt(PROF_BORDER_EXT_W, PROF_BORDER_EXT_H);
        dstStride = dstExtBuf.stride;
      }
      // ------------ time of END (2%)
      if ( yFrac == 0 ) // time of this section is 3%
      {
        m_if.filterHor( compID, (Pel*) ref, refStride, dst, dstStride, bw, bh, xFrac, isLast, chFmt, clpRng);
      }
      else if ( xFrac == 0 ) // time of this section is 3%
      {
        m_if.filterVer( compID, (Pel*) ref, refStride, dst, dstStride, bw, bh, yFrac, true, isLast, chFmt, clpRng);
      }
      else // time of this section is 59%
      {
        m_if.filterHor( compID, (Pel*)ref - ((vFilterSize>>1) -1)*refStride, refStride, tmpBuf.buf, tmpBuf.stride, bw, bh+vFilterSize-1, xFrac, false,      chFmt, clpRng);
        JVET_J0090_SET_CACHE_ENABLE( false );
        m_if.filterVer( compID, tmpBuf.buf + ((vFilterSize>>1) -1)*tmpBuf.stride, tmpBuf.stride, dst, dstStride, bw, bh, yFrac, false, isLast, chFmt, clpRng);
        JVET_J0090_SET_CACHE_ENABLE( true );
      }
      if (enablePROF) // time of this section 5%
      {
        const int shift = std::max<int>(2, (IF_INTERNAL_PREC - clpRng.bd));
        const int xOffset = xFrac >> 3;
        const int yOffset = yFrac >> 3;

        const int refOffset = (blockHeight + 1) * refStride;
        const int dstOffset = (blockHeight + 1)* dstStride;
        // dont use calculateSum
        const Pel* refPel = ref - (1 - yOffset) * refStride + xOffset - 1;
        Pel* dstPel = dst - dstStride - 1;
        for (int pw = 0; pw < blockWidth + 2; pw++)
        {
          dstPel[pw] = leftShift_round(refPel[pw], shift) - (Pel)IF_INTERNAL_OFFS;
          dstPel[pw+dstOffset] = leftShift_round(refPel[pw+refOffset], shift) - (Pel)IF_INTERNAL_OFFS;
        }

        refPel = ref + yOffset * refBuf.stride + xOffset;
        //refPel = ref + calculateSum(yOffset * refBuf.stride , xOffset, 8, 7);
        dstPel = dst;
        for (int ph = 0; ph < blockHeight; ph++, refPel += refStride, dstPel += dstStride)
        {
          dstPel[-1] = leftShift_round(refPel[-1], shift) - (Pel)IF_INTERNAL_OFFS;
          dstPel[blockWidth] = leftShift_round(refPel[blockWidth], shift) - (Pel)IF_INTERNAL_OFFS;
        }

        PelBuf gradXBuf = gradXExt.subBuf(w, h, blockWidth + 2, blockHeight + 2);
        PelBuf gradYBuf = gradYExt.subBuf(w, h, blockWidth + 2, blockHeight + 2);
        g_pelBufOP.profGradFilter(dstExtBuf.buf, dstExtBuf.stride, blockWidth + 2, blockHeight + 2, gradXBuf.stride, gradXBuf.buf, gradYBuf.buf, clpRng.bd);

        const int shiftNum = std::max<int>(2, (IF_INTERNAL_PREC - clpRng.bd));
        const Pel offset = (1 << (shiftNum - 1)) + IF_INTERNAL_OFFS;
        Pel* src = dstExtBuf.bufAt(PROF_BORDER_EXT_W, PROF_BORDER_EXT_H);
        Pel* gX = gradXBuf.bufAt(PROF_BORDER_EXT_W, PROF_BORDER_EXT_H);
        Pel* gY = gradYBuf.bufAt(PROF_BORDER_EXT_W, PROF_BORDER_EXT_H);

        Pel * dstY = dstBuf.bufAt(w, h);

        if (!bi)
        {
          g_pelBufOP.applyPROF(dstY, dstBuf.stride, src, dstExtBuf.stride, blockWidth, blockHeight, gX, gY, gradXBuf.stride, dMvScaleHor, dMvScaleVer, blockWidth, shiftNum, offset, clpRng);
        }
        else
        {
          PelBuf srcExtBuf(src, dstExtBuf.stride, Size(blockWidth, blockHeight));
          PelBuf destBuf(dstY, dstBuf.stride, Size(blockWidth, blockHeight));
          destBuf.copyFrom(srcExtBuf);
        }
      }
      }
    }
  }
  
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<nanoseconds>(stop - start);
  long long int count    = duration.count();
  timeOfXPredAffineBlk   = timeOfXPredAffineBlk + duration.count();
   
}

void InterPrediction::applyBiOptFlow(const PredictionUnit &pu, const CPelUnitBuf &yuvSrc0, const CPelUnitBuf &yuvSrc1, const int &refIdx0, const int &refIdx1, PelUnitBuf &yuvDst, const BitDepths &clipBitDepths)
{
  auto start = high_resolution_clock::now();

  const int     height = yuvDst.Y().height;
  const int     width = yuvDst.Y().width;
  int           heightG = height + 2 * BIO_EXTEND_SIZE;
  //int           heightG   = calculateSum(height , 2 * BIO_EXTEND_SIZE, 8 ,7);
  int           widthG = width + 2 * BIO_EXTEND_SIZE;
  //int widthG    = calculateSum(width , 2 * BIO_EXTEND_SIZE, 8 ,7);
  int           offsetPos = widthG*BIO_EXTEND_SIZE + BIO_EXTEND_SIZE;
  //int offsetPos = calculateSum(widthG * BIO_EXTEND_SIZE , BIO_EXTEND_SIZE, 8 ,7);

  Pel*          gradX0 = m_gradX0;
  Pel*          gradX1 = m_gradX1;
  Pel*          gradY0 = m_gradY0;
  Pel*          gradY1 = m_gradY1;

  int           stridePredMC = widthG + 2;
  const Pel*    srcY0 = m_filteredBlockTmp[2][COMPONENT_Y] + stridePredMC + 1;
  const Pel*    srcY1 = m_filteredBlockTmp[3][COMPONENT_Y] + stridePredMC + 1;
  const int     src0Stride = stridePredMC;
  const int     src1Stride = stridePredMC;

  Pel*          dstY = yuvDst.Y().buf;
  const int     dstStride = yuvDst.Y().stride;
  const Pel*    srcY0Temp = srcY0;
  const Pel*    srcY1Temp = srcY1;

  for (int refList = 0; refList < NUM_REF_PIC_LIST_01; refList++)
  {
    Pel* dstTempPtr = m_filteredBlockTmp[2 + refList][COMPONENT_Y] + stridePredMC + 1;
    Pel* gradY = (refList == 0) ? m_gradY0 : m_gradY1;
    Pel* gradX = (refList == 0) ? m_gradX0 : m_gradX1;

    xBioGradFilter(dstTempPtr, stridePredMC, widthG, heightG, widthG, gradX, gradY, clipBitDepths.recon[toChannelType(COMPONENT_Y)]);
    Pel* padStr = m_filteredBlockTmp[2 + refList][COMPONENT_Y] + 2 * stridePredMC + 2;
    for (int y = 0; y< height; y++)
    {
      padStr[-1] = padStr[0];
      padStr[width] = padStr[width - 1];
      padStr += stridePredMC;
    }

    padStr = m_filteredBlockTmp[2 + refList][COMPONENT_Y] + 2 * stridePredMC + 1;
    ::memcpy(padStr - stridePredMC, padStr, sizeof(Pel)*(widthG));
    ::memcpy(padStr + height*stridePredMC, padStr + (height - 1)*stridePredMC, sizeof(Pel)*(widthG));
  }

  const ClpRng& clpRng = pu.cu->cs->slice->clpRng(COMPONENT_Y);
  const int   bitDepth = clipBitDepths.recon[toChannelType(COMPONENT_Y)];
  const int   shiftNum = IF_INTERNAL_PREC + 1 - bitDepth;
  //const int   shiftNum = calculateSum(IF_INTERNAL_PREC + 1 , - bitDepth, 8 ,7);
  const int   offset = (1 << (shiftNum - 1)) + 2 * IF_INTERNAL_OFFS;
  //const int offset = calculateSum((1 << (shiftNum - 1)) , 2 * IF_INTERNAL_OFFS, 8 ,7);
  const int   limit = (1<<(std::max<int>(5, bitDepth - 7)));

  int xUnit = (width >> 2);
  int yUnit = (height >> 2);

  Pel *dstY0 = dstY;
  gradX0 = m_gradX0; gradX1 = m_gradX1;
  gradY0 = m_gradY0; gradY1 = m_gradY1;

  for (int yu = 0; yu < yUnit; yu++)
  {
    for (int xu = 0; xu < xUnit; xu++)
    {
      int tmpx = 0, tmpy = 0;
      int sumAbsGX = 0, sumAbsGY = 0, sumDIX = 0, sumDIY = 0;
      int sumSignGY_GX = 0;

      Pel* pGradX0Tmp = m_gradX0 + (xu << 2) + (yu << 2) * widthG;
      Pel* pGradX1Tmp = m_gradX1 + (xu << 2) + (yu << 2) * widthG;
      Pel* pGradY0Tmp = m_gradY0 + (xu << 2) + (yu << 2) * widthG;
      Pel* pGradY1Tmp = m_gradY1 + (xu << 2) + (yu << 2) * widthG;
      const Pel* SrcY1Tmp = srcY1 + (xu << 2) + (yu << 2) * src1Stride;
      const Pel* SrcY0Tmp = srcY0 + (xu << 2) + (yu << 2) * src0Stride;

      g_pelBufOP.calcBIOSums(SrcY0Tmp, SrcY1Tmp, pGradX0Tmp, pGradX1Tmp, pGradY0Tmp, pGradY1Tmp, xu, yu, src0Stride, src1Stride, widthG, bitDepth, &sumAbsGX, &sumAbsGY, &sumDIX, &sumDIY, &sumSignGY_GX);
      tmpx = (sumAbsGX == 0 ? 0 : rightShiftMSB(sumDIX << 3, sumAbsGX));
      tmpx = Clip3(-limit, limit, tmpx);

      int     mainsGxGy = sumSignGY_GX >> 12;
      int     secsGxGy = sumSignGY_GX & ((1 << 12) - 1);
      int     tmpData = tmpx * mainsGxGy;
      tmpData = ((tmpData << 12) + tmpx*secsGxGy) >> 1;
      //tmpData = calculateSum((tmpData << 12) , tmpx*secsGxGy, 8 ,7) >> 1;
      tmpy = (sumAbsGY == 0 ? 0 : rightShiftMSB(((sumDIY << 3) - tmpData), sumAbsGY));
      //tmpy      = (sumAbsGY == 0 ? 0 : rightShiftMSB(calculateSum((sumDIY << 3) , - tmpData, 8 ,7), sumAbsGY));
      tmpy = Clip3(-limit, limit, tmpy);
      srcY0Temp = srcY0 + (stridePredMC + 1) + ((yu*src0Stride + xu) << 2);
      srcY1Temp = srcY1 + (stridePredMC + 1) + ((yu*src0Stride + xu) << 2);
      gradX0 = m_gradX0 + offsetPos + ((yu*widthG + xu) << 2);
      gradX1 = m_gradX1 + offsetPos + ((yu*widthG + xu) << 2);
      gradY0 = m_gradY0 + offsetPos + ((yu*widthG + xu) << 2);
      gradY1 = m_gradY1 + offsetPos + ((yu*widthG + xu) << 2);

      dstY0 = dstY + ((yu*dstStride + xu) << 2);
      xAddBIOAvg4(srcY0Temp, src0Stride, srcY1Temp, src1Stride, dstY0, dstStride, gradX0, gradX1, gradY0, gradY1, widthG, (1 << 2), (1 << 2), (int)tmpx, (int)tmpy, shiftNum, offset, clpRng);
    }  // xu
  }  // yu
  auto stop                = high_resolution_clock::now();
  auto duration            = duration_cast<nanoseconds>(stop - start);
  timeOfApplyBiOptFlow     = timeOfApplyBiOptFlow + duration.count();
}



void InterPrediction::xAddBIOAvg4(const Pel* src0, int src0Stride, const Pel* src1, int src1Stride, Pel *dst, int dstStride, const Pel *gradX0, const Pel *gradX1, const Pel *gradY0, const Pel*gradY1, int gradStride, int width, int height, int tmpx, int tmpy, int shift, int offset, const ClpRng& clpRng)
{
  auto start = high_resolution_clock::now();

  g_pelBufOP.addBIOAvg4(src0, src0Stride, src1, src1Stride, dst, dstStride, gradX0, gradX1, gradY0, gradY1, gradStride, width, height, tmpx, tmpy, shift, offset, clpRng);
  auto stop                = high_resolution_clock::now();
  auto duration            = duration_cast<nanoseconds>(stop - start);
  timeOfXAddBIOAvg4        = timeOfXAddBIOAvg4 + duration.count();
}

void InterPrediction::xBioGradFilter(Pel* pSrc, int srcStride, int width, int height, int gradStride, Pel* gradX, Pel* gradY, int bitDepth)
{
  auto start = high_resolution_clock::now();
  g_pelBufOP.bioGradFilter(pSrc, srcStride, width, height, gradStride, gradX, gradY, bitDepth);
  auto stop                = high_resolution_clock::now();
  auto duration            = duration_cast<nanoseconds>(stop - start);
  timeOfXBioGradFilter     = timeOfXBioGradFilter + duration.count();
}

void InterPrediction::xCalcBIOPar(const Pel* srcY0Temp, const Pel* srcY1Temp, const Pel* gradX0, const Pel* gradX1, const Pel* gradY0, const Pel* gradY1, int* dotProductTemp1, int* dotProductTemp2, int* dotProductTemp3, int* dotProductTemp5, int* dotProductTemp6, const int src0Stride, const int src1Stride, const int gradStride, const int widthG, const int heightG, int bitDepth)
{
  g_pelBufOP.calcBIOPar(srcY0Temp, srcY1Temp, gradX0, gradX1, gradY0, gradY1, dotProductTemp1, dotProductTemp2, dotProductTemp3, dotProductTemp5, dotProductTemp6, src0Stride, src1Stride, gradStride, widthG, heightG, bitDepth);
}

void InterPrediction::xCalcBlkGradient(int sx, int sy, int    *arraysGx2, int     *arraysGxGy, int     *arraysGxdI, int     *arraysGy2, int     *arraysGydI, int     &sGx2, int     &sGy2, int     &sGxGy, int     &sGxdI, int     &sGydI, int width, int height, int unitSize)
{
  g_pelBufOP.calcBlkGradient(sx, sy, arraysGx2, arraysGxGy, arraysGxdI, arraysGy2, arraysGydI, sGx2, sGy2, sGxGy, sGxdI, sGydI, width, height, unitSize);
}

void InterPrediction::xWeightedAverage(const PredictionUnit& pu, const CPelUnitBuf& pcYuvSrc0, const CPelUnitBuf& pcYuvSrc1, PelUnitBuf& pcYuvDst, const BitDepths& clipBitDepths, const ClpRngs& clpRngs, const bool& bioApplied, PelUnitBuf* yuvDstTmp /*= NULL*/)
{
  const int iRefIdx0 = pu.refIdx[0];
  const int iRefIdx1 = pu.refIdx[1];

  if( iRefIdx0 >= 0 && iRefIdx1 >= 0 )
  {
    if (pu.cu->affine && (m_applyPROF[0] || m_applyPROF[1]))
    {
      xApplyBiPROF(pu, pcYuvSrc0.bufs[COMPONENT_Y], pcYuvSrc1.bufs[COMPONENT_Y], pcYuvDst.bufs[COMPONENT_Y], clpRngs.comp[COMPONENT_Y]);
      pcYuvDst.addWeightedAvg(pcYuvSrc0, pcYuvSrc1, clpRngs, pu.cu->GBiIdx, true);
      CHECK(yuvDstTmp, "yuvDstTmp is disallowed with PROF");
      return;
    }
    if( pu.cu->GBiIdx != GBI_DEFAULT && (yuvDstTmp || !pu.mhIntraFlag) )
    {
      CHECK(bioApplied, "GBi is disallowed with BIO");
      pcYuvDst.addWeightedAvg(pcYuvSrc0, pcYuvSrc1, clpRngs, pu.cu->GBiIdx);
      if (yuvDstTmp)
        yuvDstTmp->addAvg(pcYuvSrc0, pcYuvSrc1, clpRngs, false);
      return;
    }
    if (bioApplied)
    {
      const int  src0Stride = pu.lwidth() + 2 * BIO_EXTEND_SIZE + 2;
      const int  src1Stride = pu.lwidth() + 2 * BIO_EXTEND_SIZE + 2;
      const Pel* pSrcY0 = m_filteredBlockTmp[2][COMPONENT_Y] + 2 * src0Stride + 2;
      const Pel* pSrcY1 = m_filteredBlockTmp[3][COMPONENT_Y] + 2 * src1Stride + 2;

      bool bioEnabled = true;
      if (bioEnabled)
      {
        applyBiOptFlow(pu, pcYuvSrc0, pcYuvSrc1, iRefIdx0, iRefIdx1, pcYuvDst, clipBitDepths);
        if (yuvDstTmp)
          yuvDstTmp->bufs[0].addAvg(CPelBuf(pSrcY0, src0Stride, pu.lumaSize()), CPelBuf(pSrcY1, src1Stride, pu.lumaSize()), clpRngs.comp[0]);
      }
      else
      {
        pcYuvDst.bufs[0].addAvg(CPelBuf(pSrcY0, src0Stride, pu.lumaSize()), CPelBuf(pSrcY1, src1Stride, pu.lumaSize()), clpRngs.comp[0]);
        if (yuvDstTmp)
          yuvDstTmp->bufs[0].copyFrom(pcYuvDst.bufs[0]);
      }
    }
    if (pu.cs->pps->getWPBiPred())
    {
      const int iRefIdx0 = pu.refIdx[0];
      const int iRefIdx1 = pu.refIdx[1];
      WPScalingParam  *pwp0;
      WPScalingParam  *pwp1;
      getWpScaling(pu.cu->slice, iRefIdx0, iRefIdx1, pwp0, pwp1);
      if (!bioApplied)
      {
        addWeightBiComponent(pcYuvSrc0, pcYuvSrc1, pu.cu->slice->clpRngs(), pwp0, pwp1, pcYuvDst, true, COMPONENT_Y);
      }
      addWeightBiComponent(pcYuvSrc0, pcYuvSrc1, pu.cu->slice->clpRngs(), pwp0, pwp1, pcYuvDst, true, COMPONENT_Cb);
      addWeightBiComponent(pcYuvSrc0, pcYuvSrc1, pu.cu->slice->clpRngs(), pwp0, pwp1, pcYuvDst, true, COMPONENT_Cr);
    }
    else
    {
      pcYuvDst.addAvg(pcYuvSrc0, pcYuvSrc1, clpRngs, bioApplied);
    }
    if (yuvDstTmp)
    {
      if (bioApplied)
      {
        yuvDstTmp->bufs[1].copyFrom(pcYuvDst.bufs[1]);
        yuvDstTmp->bufs[2].copyFrom(pcYuvDst.bufs[2]);
      }
      else
        yuvDstTmp->copyFrom(pcYuvDst);
    }
  }
  else if( iRefIdx0 >= 0 && iRefIdx1 < 0 )
  {
    if( pu.cu->triangle )
    {
      pcYuvDst.copyFrom( pcYuvSrc0 );
    }
    else
    pcYuvDst.copyClip( pcYuvSrc0, clpRngs );
    if (yuvDstTmp)
      yuvDstTmp->copyFrom(pcYuvDst);
  }
  else if( iRefIdx0 < 0 && iRefIdx1 >= 0 )
  {
    if( pu.cu->triangle )
    {
      pcYuvDst.copyFrom( pcYuvSrc1 );
    }
    else
    pcYuvDst.copyClip( pcYuvSrc1, clpRngs );
    if (yuvDstTmp)
      yuvDstTmp->copyFrom(pcYuvDst);
  }
}

void InterPrediction::xApplyBiPROF(const PredictionUnit &pu, const CPelBuf& pcYuvSrc0, const CPelBuf& pcYuvSrc1, PelBuf& pcYuvDst, const ClpRng& clpRng)
{
  int blockWidth = AFFINE_MIN_BLOCK_SIZE;
  int blockHeight = AFFINE_MIN_BLOCK_SIZE;

  CHECK(!m_applyPROF[0] && !m_applyPROF[1], "xApplyBiPROF() applies PROF for at least one list.");
  const int width = pu.Y().width;
  const int height = pu.Y().height;

  const int bit = MAX_CU_DEPTH;
  const int shift = bit - 4 + MV_FRACTIONAL_BITS_INTERNAL;
  const int bdlimit = std::max<int>(6, clpRng.bd - 6);
  const int dmvLimit = 1 << bdlimit;

  for (int list = 0; list < 2; list++)
  {
    if (m_applyPROF[list])
    {
      Mv mvLT = pu.mvAffi[list][0];
      Mv mvRT = pu.mvAffi[list][1];
      Mv mvLB = pu.mvAffi[list][2];

      int dMvHorX, dMvHorY, dMvVerX, dMvVerY;
      dMvHorX = (mvRT - mvLT).getHor() << (bit - floorLog2(width));
      dMvHorY = (mvRT - mvLT).getVer() << (bit - floorLog2(width));
      if (pu.cu->affineType == AFFINEMODEL_6PARAM)
      {
        dMvVerX = (mvLB - mvLT).getHor() << (bit - floorLog2(height));
        dMvVerY = (mvLB - mvLT).getVer() << (bit - floorLog2(height));
      }
      else
      {
        dMvVerX = -dMvHorY;
        dMvVerY = dMvHorX;
      }

      int *dMvScaleHor = m_dMvBuf[list];
      int *dMvScaleVer = m_dMvBuf[list] + 16;

      int* dMvH = dMvScaleHor;
      int* dMvV = dMvScaleVer;
      int  quadHorX = dMvHorX << 2;
      int  quadHorY = dMvHorY << 2;
      int  quadVerX = dMvVerX << 2;
      int  quadVerY = dMvVerY << 2;

      dMvH[0] = ((dMvHorX + dMvVerX) << 1) - ((quadHorX + quadVerX) << 1);
      dMvV[0] = ((dMvHorY + dMvVerY) << 1) - ((quadHorY + quadVerY) << 1);

      for (int w = 1; w < blockWidth; w++)
      {
        dMvH[w] = dMvH[w - 1] + quadHorX;
        dMvV[w] = dMvV[w - 1] + quadHorY;
      }

      dMvH += blockWidth;
      dMvV += blockWidth;
      for (int h = 1; h < blockHeight; h++)
      {
        for (int w = 0; w < blockWidth; w++)
        {
          dMvH[w] = dMvH[w - blockWidth] + quadVerX;
          dMvV[w] = dMvV[w - blockWidth] + quadVerY;
        }
        dMvH += blockWidth;
        dMvV += blockWidth;
      }

      if (!g_pelBufOP.roundIntVector)
      {
        for (int idx = 0; idx < blockWidth * blockHeight; idx++)
        {
          roundAffineMv(dMvScaleHor[idx], dMvScaleVer[idx], shift);
          dMvScaleHor[idx] = Clip3(-dmvLimit, dmvLimit - 1, dMvScaleHor[idx]);
          dMvScaleVer[idx] = Clip3(-dmvLimit, dmvLimit - 1, dMvScaleVer[idx]);
        }
      }
      else
      {
        int sz = blockWidth * blockHeight;
        g_pelBufOP.roundIntVector(dMvScaleHor, sz, shift, dmvLimit);
        g_pelBufOP.roundIntVector(dMvScaleVer, sz, shift, dmvLimit);
      }
    }
  }

  const int cuExtW = width + PROF_BORDER_EXT_W * 2;
  const int cuExtH = height + PROF_BORDER_EXT_H * 2;

  PelBuf gradXExt0 = PelBuf(m_gradBuf[REF_PIC_LIST_0][0], cuExtW, cuExtH);
  PelBuf gradYExt0 = PelBuf(m_gradBuf[REF_PIC_LIST_0][1], cuExtW, cuExtH);
  PelBuf gradXExt1 = PelBuf(m_gradBuf[REF_PIC_LIST_1][0], cuExtW, cuExtH);
  PelBuf gradYExt1 = PelBuf(m_gradBuf[REF_PIC_LIST_1][1], cuExtW, cuExtH);

  Pel* gX0 = gradXExt0.bufAt(PROF_BORDER_EXT_W, PROF_BORDER_EXT_H);
  Pel* gY0 = gradYExt0.bufAt(PROF_BORDER_EXT_W, PROF_BORDER_EXT_H);
  Pel* gX1 = gradXExt1.bufAt(PROF_BORDER_EXT_W, PROF_BORDER_EXT_H);
  Pel* gY1 = gradYExt1.bufAt(PROF_BORDER_EXT_W, PROF_BORDER_EXT_H);

  int *dMvX0 = m_dMvBuf[REF_PIC_LIST_0];
  int *dMvY0 = m_dMvBuf[REF_PIC_LIST_0] + 16;
  int *dMvX1 = m_dMvBuf[REF_PIC_LIST_1];
  int *dMvY1 = m_dMvBuf[REF_PIC_LIST_1] + 16;

  const Pel* srcY0 = pcYuvSrc0.bufAt(0, 0);
  const Pel* srcY1 = pcYuvSrc1.bufAt(0, 0);
  Pel* dstY = pcYuvDst.bufAt(0, 0);

  if(m_applyPROF[0] && m_applyPROF[1])
    g_pelBufOP.applyBiPROF[1](dstY, pcYuvDst.stride, srcY0, srcY1, pcYuvSrc0.stride, width, height, gX0, gY0, gX1, gY1, gradXExt0.stride, dMvX0, dMvY0, dMvX1, dMvY1, blockWidth, getGbiWeight(pu.cu->GBiIdx, REF_PIC_LIST_0), clpRng);
  else if (m_applyPROF[0])
    g_pelBufOP.applyBiPROF[0](dstY, pcYuvDst.stride, srcY0, srcY1, pcYuvSrc0.stride, width, height, gX0, gY0, gX1, gY1, gradXExt0.stride, dMvX0, dMvY0, dMvX1, dMvY1, blockWidth, getGbiWeight(pu.cu->GBiIdx, REF_PIC_LIST_0), clpRng);
  else
    g_pelBufOP.applyBiPROF[0](dstY, pcYuvDst.stride, srcY1, srcY0, pcYuvSrc0.stride, width, height, gX1, gY1, gX0, gY0, gradXExt0.stride, dMvX1, dMvY1, dMvX0, dMvY0, blockWidth, getGbiWeight(pu.cu->GBiIdx, REF_PIC_LIST_1), clpRng);
}

void InterPrediction::motionCompensation( PredictionUnit &pu, PelUnitBuf &predBuf, const RefPicList &eRefPicList
  , const bool luma, const bool chroma
  , PelUnitBuf* predBufWOBIO /*= NULL*/
)
{
  CHECK(predBufWOBIO && pu.mhIntraFlag, "the case should not happen!");

  if (!pu.cs->pcv->isEncoder)
  {
    if (CU::isIBC(*pu.cu))
    {
      CHECK(!luma, "IBC only for Chroma is not allowed.");
      xIntraBlockCopy(pu, predBuf, COMPONENT_Y);
      if (chroma)
      {
        xIntraBlockCopy(pu, predBuf, COMPONENT_Cb);
        xIntraBlockCopy(pu, predBuf, COMPONENT_Cr);
      }
      return;
    }
  }
  // dual tree handling for IBC as the only ref
  if ((!luma || !chroma) && eRefPicList == REF_PIC_LIST_0)
  {
      xPredInterUni(pu, eRefPicList, predBuf, false
        , false
        , luma, chroma);
      return;
  }
  // else, go with regular MC below
        CodingStructure &cs = *pu.cs;
  const PPS &pps            = *cs.pps;
  const SliceType sliceType =  cs.slice->getSliceType();

  if( eRefPicList != REF_PIC_LIST_X )
  {
    CHECK(predBufWOBIO != NULL, "the case should not happen!");
    if( ( ( sliceType == P_SLICE && pps.getUseWP() ) || ( sliceType == B_SLICE && pps.getWPBiPred() ) ) )
    {
      xPredInterUni         ( pu,          eRefPicList, predBuf, true
        , false
        , true, true
      );
      xWeightedPredictionUni( pu, predBuf, eRefPicList, predBuf, -1, m_maxCompIDToPred );
    }
    else
    {
      xPredInterUni( pu, eRefPicList, predBuf, false
        , false
        , true, true
      );
    }
  }
  else
  {

    CHECK( !pu.cu->affine && pu.refIdx[0] >= 0 && pu.refIdx[1] >= 0 && ( pu.lwidth() + pu.lheight() == 12 ), "invalid 4x8/8x4 bi-predicted blocks" );
    WPScalingParam *wp0;
    WPScalingParam *wp1;
    int refIdx0 = pu.refIdx[REF_PIC_LIST_0];
    int refIdx1 = pu.refIdx[REF_PIC_LIST_1];
    pu.cs->slice->getWpScaling(REF_PIC_LIST_0, refIdx0, wp0);
    pu.cs->slice->getWpScaling(REF_PIC_LIST_1, refIdx1, wp1);
    bool bioApplied = false;
    const Slice &slice = *pu.cs->slice;
    if (pu.cs->sps->getBDOFEnabledFlag() && (!pu.cs->slice->getDisBdofDmvrFlag()))
    {

      if (pu.cu->affine || m_subPuMC)
      {
        bioApplied = false;
      }
      else
      {
        const bool biocheck0 = !((wp0[COMPONENT_Y].bPresentFlag || wp1[COMPONENT_Y].bPresentFlag) && slice.getSliceType() == B_SLICE);
        const bool biocheck1 = !(pps.getUseWP() && slice.getSliceType() == P_SLICE);
        if (biocheck0
          && biocheck1
          && PU::isBiPredFromDifferentDir(pu)
          && (pu.Y().height >= 8)
          && (pu.Y().width >= 8)
          && ((pu.Y().height * pu.Y().width) >= 128)
          )
        {
          bioApplied = true;
        }
      }

      if (bioApplied && pu.mhIntraFlag)
      {
        bioApplied = false;
      }

      if (bioApplied && pu.cu->smvdMode)
      {
        bioApplied = false;
      }
      if (pu.cu->cs->sps->getUseGBi() && bioApplied && pu.cu->GBiIdx != GBI_DEFAULT)
      {
        bioApplied = false;
      }
      if (pu.mmvdEncOptMode == 2 && pu.mmvdMergeFlag)
      {
        bioApplied = false;
      }
    }
    bool dmvrApplied = false;
    dmvrApplied = (pu.mvRefine) && PU::checkDMVRCondition(pu);
    if ((pu.lumaSize().width > MAX_BDOF_APPLICATION_REGION || pu.lumaSize().height > MAX_BDOF_APPLICATION_REGION) && pu.mergeType != MRG_TYPE_SUBPU_ATMVP && (bioApplied && !dmvrApplied))
    {
      xSubPuBio(pu, predBuf, eRefPicList, predBufWOBIO);
    }
    else
    if (pu.mergeType != MRG_TYPE_DEFAULT_N && pu.mergeType != MRG_TYPE_IBC)
    {
      CHECK(predBufWOBIO != NULL, "the case should not happen!");
      xSubPuMC( pu, predBuf, eRefPicList );
    }
    else if( xCheckIdenticalMotion( pu ) )
    {
      xPredInterUni( pu, REF_PIC_LIST_0, predBuf, false
        , false
        , true, true
      );
      if (predBufWOBIO)
        predBufWOBIO->copyFrom(predBuf);
    }
    else
    {
      xPredInterBi(pu, predBuf, predBufWOBIO);
    }
  }
  return;
}

void InterPrediction::motionCompensation( CodingUnit &cu, const RefPicList &eRefPicList
  , const bool luma, const bool chroma
)
{
  for( auto &pu : CU::traversePUs( cu ) )
  {
    PelUnitBuf predBuf = cu.cs->getPredBuf( pu );
    pu.mvRefine = true;
    motionCompensation( pu, predBuf, eRefPicList
      , luma, chroma
    );
    pu.mvRefine = false;
  }
}

void InterPrediction::motionCompensation( PredictionUnit &pu, const RefPicList &eRefPicList /*= REF_PIC_LIST_X*/
  , const bool luma, const bool chroma
)
{
  PelUnitBuf predBuf = pu.cs->getPredBuf( pu );
  motionCompensation( pu, predBuf, eRefPicList
    , luma, chroma
  );
}

int InterPrediction::rightShiftMSB(int numer, int denom)
{
  auto start            = high_resolution_clock::now();

  int output = numer >> floorLog2(denom);

  auto stop             = high_resolution_clock::now();
  auto duration         = duration_cast<nanoseconds>(stop - start);
  timeOfRightShiftMSB   = timeOfRightShiftMSB + duration.count();
  
  return output;
}

void InterPrediction::motionCompensation4Triangle( CodingUnit &cu, MergeCtx &triangleMrgCtx, const bool splitDir, const uint8_t candIdx0, const uint8_t candIdx1 )
{
  for( auto &pu : CU::traversePUs( cu ) )
  {
    const UnitArea localUnitArea( cu.cs->area.chromaFormat, Area( 0, 0, pu.lwidth(), pu.lheight() ) );
    PelUnitBuf tmpTriangleBuf = m_triangleBuf.getBuf( localUnitArea );
    PelUnitBuf predBuf        = cu.cs->getPredBuf( pu );

    triangleMrgCtx.setMergeInfo( pu, candIdx0 );
    PU::spanMotionInfo( pu );
    motionCompensation( pu, tmpTriangleBuf );

    {
      if( g_mctsDecCheckEnabled && !MCTSHelper::checkMvBufferForMCTSConstraint( pu, true ) )
      {
        printf( "DECODER_TRIANGLE_PU: pu motion vector across tile boundaries (%d,%d,%d,%d)\n", pu.lx(), pu.ly(), pu.lwidth(), pu.lheight() );
      }
    }

    triangleMrgCtx.setMergeInfo( pu, candIdx1 );
    PU::spanMotionInfo( pu );
    motionCompensation( pu, predBuf );

    {
      if( g_mctsDecCheckEnabled && !MCTSHelper::checkMvBufferForMCTSConstraint( pu, true ) )
      {
        printf( "DECODER_TRIANGLE_PU: pu motion vector across tile boundaries (%d,%d,%d,%d)\n", pu.lx(), pu.ly(), pu.lwidth(), pu.lheight() );
      }
    }
    weightedTriangleBlk( pu, splitDir, MAX_NUM_CHANNEL_TYPE, predBuf, tmpTriangleBuf, predBuf );
  }
}

void InterPrediction::weightedTriangleBlk( PredictionUnit &pu, const bool splitDir, int32_t channel, PelUnitBuf& predDst, PelUnitBuf& predSrc0, PelUnitBuf& predSrc1 )
{
  if( channel == CHANNEL_TYPE_LUMA )
  {
    m_if.weightedTriangleBlk( pu, pu.lumaSize().width, pu.lumaSize().height, COMPONENT_Y, splitDir, predDst, predSrc0, predSrc1 );
  }
  else if( channel == CHANNEL_TYPE_CHROMA )
  {
    m_if.weightedTriangleBlk( pu, pu.chromaSize().width, pu.chromaSize().height, COMPONENT_Cb, splitDir, predDst, predSrc0, predSrc1 );
    m_if.weightedTriangleBlk( pu, pu.chromaSize().width, pu.chromaSize().height, COMPONENT_Cr, splitDir, predDst, predSrc0, predSrc1 );
  }
  else
  {
    m_if.weightedTriangleBlk( pu, pu.lumaSize().width,   pu.lumaSize().height,   COMPONENT_Y,  splitDir, predDst, predSrc0, predSrc1 );
    m_if.weightedTriangleBlk( pu, pu.chromaSize().width, pu.chromaSize().height, COMPONENT_Cb, splitDir, predDst, predSrc0, predSrc1 );
    m_if.weightedTriangleBlk( pu, pu.chromaSize().width, pu.chromaSize().height, COMPONENT_Cr, splitDir, predDst, predSrc0, predSrc1 );
  }
}


void InterPrediction::xPrefetch(PredictionUnit& pu, PelUnitBuf &pcPad, RefPicList refId, bool forLuma)
{
  auto           startTimer = high_resolution_clock::now();

  int offset, width, height;
  Mv cMv;
  const Picture* refPic = pu.cu->slice->getRefPic( refId, pu.refIdx[refId] )->unscaledPic;
  int mvShift = (MV_FRACTIONAL_BITS_INTERNAL);

  int start = 0;
  int end = MAX_NUM_COMPONENT;

  start = forLuma ? 0 : 1;
  end = forLuma ? 1 : MAX_NUM_COMPONENT;

  for (int compID = start; compID < end; compID++)
  {
    cMv = Mv(pu.mv[refId].getHor(), pu.mv[refId].getVer());
    pcPad.bufs[compID].stride = (pcPad.bufs[compID].width + (2 * DMVR_NUM_ITERATION) + NTAPS_LUMA);
    int filtersize = (compID == (COMPONENT_Y)) ? NTAPS_LUMA : NTAPS_CHROMA;
    width = pcPad.bufs[compID].width;
    height = pcPad.bufs[compID].height;
    offset = (DMVR_NUM_ITERATION) * (pcPad.bufs[compID].stride + 1);
    int mvshiftTemp = mvShift + getComponentScaleX((ComponentID)compID, pu.chromaFormat);
    //int mvshiftTemp           = calculateSum(mvShift , getComponentScaleX((ComponentID) compID, pu.chromaFormat), 8, 7);
    width += (filtersize - 1);
    //width = calculateSum(width, (filtersize - 1), 8, 7);
    height += (filtersize - 1);
    //height = calculateSum(height, (filtersize - 1), 8, 7);
    cMv += Mv(-(((filtersize >> 1) - 1) << mvshiftTemp),
      -(((filtersize >> 1) - 1) << mvshiftTemp));
    bool wrapRef = false;
    if( pu.cs->sps->getWrapAroundEnabledFlag() )
    {
      wrapRef = wrapClipMv( cMv, pu.blocks[0].pos(), pu.blocks[0].size(), pu.cs->sps, pu.cs->pps );
    }
    else
    {
      clipMv( cMv, pu.lumaPos(), pu.lumaSize(), *pu.cs->sps, *pu.cs->pps );
    }
    /* Pre-fetch similar to HEVC*/
    {
      CPelBuf refBuf;
      Position Rec_offset = pu.blocks[compID].pos().offset(cMv.getHor() >> mvshiftTemp, cMv.getVer() >> mvshiftTemp);
      refBuf = refPic->getRecoBuf(CompArea((ComponentID)compID, pu.chromaFormat, Rec_offset, pu.blocks[compID].size()), wrapRef);
      PelBuf &dstBuf = pcPad.bufs[compID];
      g_pelBufOP.copyBuffer((Pel *)refBuf.buf, refBuf.stride, ((Pel *)dstBuf.buf) + offset, dstBuf.stride, width, height);
    }
  }

  auto stopTimer       = high_resolution_clock::now();
  auto duration   = duration_cast<nanoseconds>(stopTimer - startTimer);
  timeOfXPrefetch = timeOfXPrefetch + duration.count();
}
void InterPrediction::xPad(PredictionUnit& pu, PelUnitBuf &pcPad, RefPicList refId)
{
  auto start  = high_resolution_clock::now();
  int offset = 0, width, height;
  int padsize;
  Mv cMv;
  for (int compID = 0; compID < MAX_NUM_COMPONENT; compID++)
  {
    int filtersize = (compID == (COMPONENT_Y)) ? NTAPS_LUMA : NTAPS_CHROMA;
    width = pcPad.bufs[compID].width;
    height = pcPad.bufs[compID].height;
    offset = (DMVR_NUM_ITERATION) * (pcPad.bufs[compID].stride + 1);
    padsize = (DMVR_NUM_ITERATION) >> getComponentScaleX((ComponentID)compID, pu.chromaFormat);
    width += (filtersize - 1);
    //width = calculateSum(width, filtersize - 1, 8, 7);
    height += (filtersize - 1);
    //height += calculateSum(height, filtersize - 1, 8, 7);

    /*padding on all side of size DMVR_PAD_LENGTH*/
    {
      g_pelBufOP.padding(pcPad.bufs[compID].buf + offset, pcPad.bufs[compID].stride, width, height, padsize);
    }
  }
  auto stop        = high_resolution_clock::now();
  auto duration    = duration_cast<nanoseconds>(stop - start);
  timeOfXPad       = timeOfXPad + duration.count();
}
inline int32_t div_for_maxq7(int64_t N, int64_t D)
{
  auto    start = high_resolution_clock::now();

  int32_t sign, q;
  sign = 0;
  if (N < 0)
  {
    sign = 1;
    N = -N;
  }

  q = 0;
  D = (D << 3);
  if (N >= D)
  {
    N -= D;
    q++;
  }
  q = (q << 1);

  D = (D >> 1);
  if (N >= D)
  {
    N -= D;
    q++;
  }
  q = (q << 1);

  if (N >= (D >> 1))
    q++;

  auto stop        = high_resolution_clock::now();
  auto duration    = duration_cast<nanoseconds>(stop - start);
  timeOfDiv_for_maxq7 = timeOfDiv_for_maxq7 + duration.count();

  if (sign)
    return (-q);
  return(q);
}

void xSubPelErrorSrfc(uint64_t *sadBuffer, int32_t *deltaMv)
{
  auto    start = high_resolution_clock::now();
  int64_t numerator, denominator;
  int32_t mvDeltaSubPel;
  int32_t mvSubPelLvl = 4;/*1: half pel, 2: Qpel, 3:1/8, 4: 1/16*/
                                                        /*horizontal*/
    numerator = (int64_t)((sadBuffer[1] - sadBuffer[3]) << mvSubPelLvl);
    denominator = (int64_t)((sadBuffer[1] + sadBuffer[3] - (sadBuffer[0] << 1)));

    if (0 != denominator)
    {
      if ((sadBuffer[1] != sadBuffer[0]) && (sadBuffer[3] != sadBuffer[0]))
      {
        mvDeltaSubPel = div_for_maxq7(numerator, denominator);
        deltaMv[0] = (mvDeltaSubPel);
      }
      else
      {
        if (sadBuffer[1] == sadBuffer[0])
        {
          deltaMv[0] = -8;// half pel
        }
        else
        {
          deltaMv[0] = 8;// half pel
        }
      }
    }

    /*vertical*/
    numerator = (int64_t)((sadBuffer[2] - sadBuffer[4]) << mvSubPelLvl);
    denominator = (int64_t)((sadBuffer[2] + sadBuffer[4] - (sadBuffer[0] << 1)));
    if (0 != denominator)
    {
      if ((sadBuffer[2] != sadBuffer[0]) && (sadBuffer[4] != sadBuffer[0]))
      {
        mvDeltaSubPel = div_for_maxq7(numerator, denominator);
        deltaMv[1] = (mvDeltaSubPel);
      }
      else
      {
        if (sadBuffer[2] == sadBuffer[0])
        {
          deltaMv[1] = -8;// half pel
        }
        else
        {
          deltaMv[1] = 8;// half pel
        }
      }
    }

  auto stop        = high_resolution_clock::now();
  auto duration    = duration_cast<nanoseconds>(stop - start);
  timeOfXSubPelErrorSrfc = timeOfXSubPelErrorSrfc + duration.count();

  return;
}

void InterPrediction::xBIPMVRefine(int bd, Pel *pRefL0, Pel *pRefL1, uint64_t& minCost, int16_t *deltaMV, uint64_t *pSADsArray, int width, int height)
{
  auto          start       = high_resolution_clock::now();

  const int32_t refStrideL0 = m_biLinearBufStride;
  const int32_t refStrideL1 = m_biLinearBufStride;
  Pel *pRefL0Orig = pRefL0;
  Pel *pRefL1Orig = pRefL1;
  for (int nIdx = 0; (nIdx < 25); ++nIdx)
  {
    int32_t sadOffset = ((m_pSearchOffset[nIdx].getVer() * ((2 * DMVR_NUM_ITERATION) + 1)) + m_pSearchOffset[nIdx].getHor());
    //int32_t sadOffset = (calculateSum((m_pSearchOffset[nIdx].getVer() * ((2 * DMVR_NUM_ITERATION) + 1)) , m_pSearchOffset[nIdx].getHor(), 8, 7));
    pRefL0 = pRefL0Orig + m_pSearchOffset[nIdx].hor + (m_pSearchOffset[nIdx].ver * refStrideL0);
    //pRefL0 = pRefL0Orig + calculateSum(m_pSearchOffset[nIdx].hor , (m_pSearchOffset[nIdx].ver * refStrideL0), 8, 7);
    pRefL1 = pRefL1Orig - m_pSearchOffset[nIdx].hor - (m_pSearchOffset[nIdx].ver * refStrideL1);
    //pRefL1 = pRefL1Orig - calculateSum(m_pSearchOffset[nIdx].hor , - (m_pSearchOffset[nIdx].ver * refStrideL1), 8, 7);
    if (*(pSADsArray + sadOffset) == MAX_UINT64)
    {
      const uint64_t cost = xDMVRCost(bd, pRefL0, refStrideL0, pRefL1, refStrideL1, width, height);
      *(pSADsArray + sadOffset) = cost;
    }
    if (*(pSADsArray + sadOffset) < minCost)
    {
      minCost = *(pSADsArray + sadOffset);
      deltaMV[0] = m_pSearchOffset[nIdx].getHor();
      deltaMV[1] = m_pSearchOffset[nIdx].getVer();
    }
  }

  auto stop             = high_resolution_clock::now();
  auto duration         = duration_cast<nanoseconds>(stop - start);
  timeOfXBIPMVRefine    = timeOfXBIPMVRefine + duration.count();

}

void InterPrediction::xFinalPaddedMCForDMVR(PredictionUnit& pu, PelUnitBuf &pcYuvSrc0, PelUnitBuf &pcYuvSrc1, PelUnitBuf &pcPad0, PelUnitBuf &pcPad1, const bool bioApplied
  , const Mv mergeMV[NUM_REF_PIC_LIST_01]
  , bool blockMoved
)
{
  auto start = high_resolution_clock::now();
  int offset, deltaIntMvX, deltaIntMvY;

  PelUnitBuf pcYUVTemp = pcYuvSrc0;
  PelUnitBuf pcPadTemp = pcPad0;
  /*always high precision MVs are used*/
  int mvShift = MV_FRACTIONAL_BITS_INTERNAL;

  for (int k = 0; k < NUM_REF_PIC_LIST_01; k++)
  {
    RefPicList refId = (RefPicList)k;
    Mv cMv = pu.mv[refId];
    m_iRefListIdx = refId;
    const Picture* refPic = pu.cu->slice->getRefPic( refId, pu.refIdx[refId] )->unscaledPic;
    Mv cMvClipped = cMv;
    clipMv( cMvClipped, pu.lumaPos(), pu.lumaSize(), *pu.cs->sps, *pu.cs->pps );

    Mv startMv = mergeMV[refId];

    if( g_mctsDecCheckEnabled && !MCTSHelper::checkMvForMCTSConstraint( pu, startMv, MV_PRECISION_INTERNAL ) )
    {
      const Area& tileArea = pu.cs->picture->mctsInfo.getTileArea();
      printf( "Attempt an access over tile boundary at block %d,%d %d,%d with MV %d,%d (in Tile TL: %d,%d BR: %d,%d)\n",
        pu.lx(), pu.ly(), pu.lwidth(), pu.lheight(), startMv.getHor(), startMv.getVer(), tileArea.topLeft().x, tileArea.topLeft().y, tileArea.bottomRight().x, tileArea.bottomRight().y );
      THROW( "MCTS constraint failed!" );
    }
    for (int compID = 0; compID < MAX_NUM_COMPONENT; compID++)
    {
      Pel *srcBufPelPtr = NULL;
      int pcPadstride = 0;
      if (blockMoved || (compID == 0))
      {
        pcPadstride = pcPadTemp.bufs[compID].stride;
        int mvshiftTemp = mvShift + getComponentScaleX((ComponentID)compID, pu.chromaFormat);
        //int mvshiftTemp = calculateSum(mvShift ,(int) getComponentScaleX((ComponentID) compID, pu.chromaFormat), 8, 7);
        int leftPixelExtra;
        if (compID == COMPONENT_Y)
        {
          leftPixelExtra = (NTAPS_LUMA >> 1) - 1;
        }
        else
        {
          leftPixelExtra = (NTAPS_CHROMA >> 1) - 1;
        }
        PelBuf &srcBuf = pcPadTemp.bufs[compID];
        // dont use calculateSum
        deltaIntMvX = (cMv.getHor() >> mvshiftTemp) - (startMv.getHor() >> mvshiftTemp);
        // dont use calculateSum
        deltaIntMvY = (cMv.getVer() >> mvshiftTemp) - (startMv.getVer() >> mvshiftTemp);

        CHECK((abs(deltaIntMvX) > DMVR_NUM_ITERATION) || (abs(deltaIntMvY) > DMVR_NUM_ITERATION), "not expected DMVR movement");

        offset = (DMVR_NUM_ITERATION + leftPixelExtra) * (pcPadTemp.bufs[compID].stride + 1);
        offset += (deltaIntMvY)* pcPadTemp.bufs[compID].stride;
        //offset = calculateSum(offset, (deltaIntMvY) * pcPadTemp.bufs[compID].stride, 8, 7);
        offset += (deltaIntMvX);
        //offset       = (calculateSum(offset, deltaIntMvX, 8, 7));
        srcBufPelPtr = (srcBuf.buf + offset);
      }

      xPredInterBlk( (ComponentID)compID, pu, refPic, cMvClipped, pcYUVTemp, true, pu.cs->slice->getClpRngs().comp[compID],
        bioApplied, false, pu.cu->slice->getScalingRatio( refId, pu.refIdx[refId] ), 0, 0, 0, srcBufPelPtr, pcPadstride );
    }
    pcYUVTemp = pcYuvSrc1;
    pcPadTemp = pcPad1;
  }
  auto stop                    = high_resolution_clock::now();
  auto duration                = duration_cast<nanoseconds>(stop - start);
  timeOfXFinalPaddedMCForDMVR  = timeOfXFinalPaddedMCForDMVR + duration.count();
}

uint64_t InterPrediction::xDMVRCost(int bitDepth, Pel* pOrg, uint32_t refStride, const Pel* pRef, uint32_t orgStride, int width, int height)
{
  auto      start = high_resolution_clock::now();
  DistParam cDistParam;
  cDistParam.applyWeight = false;
  cDistParam.useMR = false;
  m_pcRdCost->setDistParam(cDistParam, pOrg, pRef, orgStride, refStride, bitDepth, COMPONENT_Y, width, height, 1);
  uint64_t uiCost = cDistParam.distFunc(cDistParam);
  auto     stop            = high_resolution_clock::now();
  auto     duration        = duration_cast<nanoseconds>(stop - start);
  timeOfXDMVRCost          = timeOfXDMVRCost + duration.count();

  return uiCost>>1;
}

void xDMVRSubPixelErrorSurface(bool notZeroCost, int16_t *totalDeltaMV, int16_t *deltaMV, uint64_t *pSADsArray)
{
  auto     start     = high_resolution_clock::now();

  int sadStride = (((2 * DMVR_NUM_ITERATION) + 1));
  uint64_t sadbuffer[5];
  if (notZeroCost && (abs(totalDeltaMV[0]) != (2 << MV_FRACTIONAL_BITS_INTERNAL))
    && (abs(totalDeltaMV[1]) != (2 << MV_FRACTIONAL_BITS_INTERNAL)))
  {
    int32_t tempDeltaMv[2] = { 0,0 };
    sadbuffer[0] = pSADsArray[0];
    sadbuffer[1] = pSADsArray[-1];
    sadbuffer[2] = pSADsArray[-sadStride];
    sadbuffer[3] = pSADsArray[1];
    sadbuffer[4] = pSADsArray[sadStride];
    xSubPelErrorSrfc(sadbuffer, tempDeltaMv);

    totalDeltaMV[0] += tempDeltaMv[0];
    //totalDeltaMV[0] = calculateSum(totalDeltaMV[0] , tempDeltaMv[0], 8, 7);
    
    totalDeltaMV[1] += tempDeltaMv[1];
    //totalDeltaMV[1] = calculateSum(totalDeltaMV[1] , tempDeltaMv[1], 8, 7);
  }

  auto stop        = high_resolution_clock::now();
  auto duration    = duration_cast<nanoseconds>(stop - start);
  timeOfXDMVRSubPixelErrorSurface = timeOfXDMVRSubPixelErrorSurface + duration.count();
}

void InterPrediction::xinitMC(PredictionUnit& pu, const ClpRngs &clpRngs)
{
  auto      start   = high_resolution_clock::now();
  const int refIdx0 = pu.refIdx[0];
  const int refIdx1 = pu.refIdx[1];
  /*use merge MV as starting MV*/
  Mv mergeMVL0(pu.mv[REF_PIC_LIST_0]);
  Mv mergeMVL1(pu.mv[REF_PIC_LIST_1]);

  /*Clip the starting MVs*/
  clipMv( mergeMVL0, pu.lumaPos(), pu.lumaSize(), *pu.cs->sps, *pu.cs->pps );
  clipMv( mergeMVL1, pu.lumaPos(), pu.lumaSize(), *pu.cs->sps, *pu.cs->pps );

  /*L0 MC for refinement*/
  {
    int offset;
    int leftPixelExtra = (NTAPS_LUMA >> 1) - 1;
    offset = (DMVR_NUM_ITERATION + leftPixelExtra) * (m_cYuvRefBuffDMVRL0.bufs[COMPONENT_Y].stride + 1);
    //offset = (calculateSum(DMVR_NUM_ITERATION , leftPixelExtra, 8 ,7)) * (m_cYuvRefBuffDMVRL0.bufs[COMPONENT_Y].stride + 1);
    offset += (-(int)DMVR_NUM_ITERATION)* (int)m_cYuvRefBuffDMVRL0.bufs[COMPONENT_Y].stride;
    //offset = calculateSum(offset , (-(int) DMVR_NUM_ITERATION) * (int) m_cYuvRefBuffDMVRL0.bufs[COMPONENT_Y].stride, 8 ,7);
    offset += (-(int)DMVR_NUM_ITERATION);
    //offset = calculateSum(offset , (-(int)DMVR_NUM_ITERATION), 8 ,7);
    PelBuf srcBuf = m_cYuvRefBuffDMVRL0.bufs[COMPONENT_Y];
    PelUnitBuf yuvPredTempL0 = PelUnitBuf(pu.chromaFormat, PelBuf(m_cYuvPredTempDMVRL0, m_biLinearBufStride, pu.lwidth() + (2 * DMVR_NUM_ITERATION), pu.lheight() + (2 * DMVR_NUM_ITERATION)));
    //PelUnitBuf yuvPredTempL0 = PelUnitBuf(pu.chromaFormat, PelBuf(m_cYuvPredTempDMVRL0, m_biLinearBufStride, calculateSum(pu.lwidth() , (2 * DMVR_NUM_ITERATION), 8 ,7), calculateSum(pu.lheight() , (2 * DMVR_NUM_ITERATION), 8 ,7)));

    xPredInterBlk( COMPONENT_Y, pu, pu.cu->slice->getRefPic( REF_PIC_LIST_0, refIdx0 )->unscaledPic, mergeMVL0, yuvPredTempL0, true, clpRngs.comp[COMPONENT_Y],false, false, pu.cu->slice->getScalingRatio( REF_PIC_LIST_0, refIdx0 ), pu.lwidth() + ( 2 * DMVR_NUM_ITERATION ), pu.lheight() + ( 2 * DMVR_NUM_ITERATION ), true, ( (Pel *)srcBuf.buf ) + offset, srcBuf.stride );
    //xPredInterBlk( COMPONENT_Y, pu, pu.cu->slice->getRefPic( REF_PIC_LIST_0, refIdx0 )->unscaledPic, mergeMVL0, yuvPredTempL0, true, clpRngs.comp[COMPONENT_Y],false, false, pu.cu->slice->getScalingRatio( REF_PIC_LIST_0, refIdx0 ), calculateSum( pu.lwidth() , ( 2 * DMVR_NUM_ITERATION ), 8 ,7), calculateSum(pu.lheight() , ( 2 * DMVR_NUM_ITERATION ), 8 ,7), true, ( (Pel *)srcBuf.buf ) + offset, srcBuf.stride );
  }

  /*L1 MC for refinement*/
  {
    int offset;
    int leftPixelExtra = (NTAPS_LUMA >> 1) - 1;
    offset = (DMVR_NUM_ITERATION + leftPixelExtra) * (m_cYuvRefBuffDMVRL1.bufs[COMPONENT_Y].stride + 1);
    //offset = (calculateSum(DMVR_NUM_ITERATION , leftPixelExtra, 8 ,7)) * (m_cYuvRefBuffDMVRL1.bufs[COMPONENT_Y].stride + 1);
    offset += (-(int)DMVR_NUM_ITERATION)* (int)m_cYuvRefBuffDMVRL1.bufs[COMPONENT_Y].stride;
    //offset = calculateSum(offset , (-(int) DMVR_NUM_ITERATION) * (int) m_cYuvRefBuffDMVRL1.bufs[COMPONENT_Y].stride, 8 ,7);
    offset += (-(int)DMVR_NUM_ITERATION);
    //offset = calculateSum(offset , (-(int)DMVR_NUM_ITERATION), 8 ,7);
    PelBuf srcBuf = m_cYuvRefBuffDMVRL1.bufs[COMPONENT_Y];
    PelUnitBuf yuvPredTempL1 = PelUnitBuf(pu.chromaFormat, PelBuf(m_cYuvPredTempDMVRL1, m_biLinearBufStride , pu.lwidth() + (2 * DMVR_NUM_ITERATION), pu.lheight() + (2 * DMVR_NUM_ITERATION)));
    //PelUnitBuf yuvPredTempL1 = PelUnitBuf(pu.chromaFormat, PelBuf(m_cYuvPredTempDMVRL1, m_biLinearBufStride , calculateSum(pu.lwidth() , (2 * DMVR_NUM_ITERATION), 8 ,7), calculateSum(pu.lheight() , (2 * DMVR_NUM_ITERATION), 8 ,7)));

    xPredInterBlk( COMPONENT_Y, pu, pu.cu->slice->getRefPic( REF_PIC_LIST_1, refIdx1 )->unscaledPic, mergeMVL1, yuvPredTempL1, true, clpRngs.comp[COMPONENT_Y], false, false, pu.cu->slice->getScalingRatio( REF_PIC_LIST_1, refIdx1 ), pu.lwidth() + ( 2 * DMVR_NUM_ITERATION ), pu.lheight() + ( 2 * DMVR_NUM_ITERATION ), true, ( (Pel *)srcBuf.buf ) + offset, srcBuf.stride );
    //xPredInterBlk( COMPONENT_Y, pu, pu.cu->slice->getRefPic( REF_PIC_LIST_1, refIdx1 )->unscaledPic, mergeMVL1, yuvPredTempL1, true, clpRngs.comp[COMPONENT_Y], false, false, pu.cu->slice->getScalingRatio( REF_PIC_LIST_1, refIdx1 ), calculateSum(pu.lwidth() , ( 2 * DMVR_NUM_ITERATION ), 8 ,7), calculateSum(pu.lheight() , ( 2 * DMVR_NUM_ITERATION ), 8 ,7), true, ( (Pel *)srcBuf.buf ) + offset, srcBuf.stride );
  }
  auto stop       = high_resolution_clock::now();
  auto duration   = duration_cast<nanoseconds>(stop - start);
  timeOfXinitMC   = timeOfXinitMC + duration.count();
}

void InterPrediction::xProcessDMVR(PredictionUnit& pu, PelUnitBuf &pcYuvDst, const ClpRngs &clpRngs, const bool bioApplied)
{
  auto start          = high_resolution_clock::now();
  int iterationCount = 1;
  /*Always High Precision*/
  int mvShift = MV_FRACTIONAL_BITS_INTERNAL;

  /*use merge MV as starting MV*/
  Mv mergeMv[] = { pu.mv[REF_PIC_LIST_0] , pu.mv[REF_PIC_LIST_1] };

  // dont use calculateSum
  m_biLinearBufStride = (MAX_CU_SIZE + (2 * DMVR_NUM_ITERATION));

  int dy = std::min<int>(pu.lumaSize().height, DMVR_SUBCU_HEIGHT);
  int dx = std::min<int>(pu.lumaSize().width,  DMVR_SUBCU_WIDTH);
  Position puPos = pu.lumaPos();

  int bd = pu.cs->slice->getClpRngs().comp[COMPONENT_Y].bd;

  int            bioEnabledThres = 2 * dy * dx;
  bool           bioAppliedType[MAX_NUM_SUBCU_DMVR];
  {
    int num = 0;

    int scaleX = getComponentScaleX(COMPONENT_Cb, pu.chromaFormat);
    int scaleY = getComponentScaleY(COMPONENT_Cb, pu.chromaFormat);

    // dont use calculateSum
    m_biLinearBufStride = (dx + (2 * DMVR_NUM_ITERATION));

    // point mc buffer to cetre point to avoid multiplication to reach each iteration to the begining
    Pel *biLinearPredL0 = m_cYuvPredTempDMVRL0 + (DMVR_NUM_ITERATION * m_biLinearBufStride) + DMVR_NUM_ITERATION;
    //Pel *biLinearPredL0 = m_cYuvPredTempDMVRL0 + calculateSum((DMVR_NUM_ITERATION * m_biLinearBufStride) , DMVR_NUM_ITERATION, 8, 7);
    Pel *biLinearPredL1 = m_cYuvPredTempDMVRL1 + (DMVR_NUM_ITERATION * m_biLinearBufStride) + DMVR_NUM_ITERATION;
    //Pel *biLinearPredL1 = m_cYuvPredTempDMVRL1 + calculateSum((DMVR_NUM_ITERATION * m_biLinearBufStride) , DMVR_NUM_ITERATION, 8, 7);

    PredictionUnit subPu = pu;
    subPu.UnitArea::operator=(UnitArea(pu.chromaFormat, Area(puPos.x, puPos.y, dx, dy)));
    m_cYuvRefBuffDMVRL0 = (pu.chromaFormat == CHROMA_400 ?
      PelUnitBuf(pu.chromaFormat, PelBuf(m_cRefSamplesDMVRL0[0], pcYuvDst.Y())) :
      PelUnitBuf(pu.chromaFormat, PelBuf(m_cRefSamplesDMVRL0[0], pcYuvDst.Y()),
        PelBuf(m_cRefSamplesDMVRL0[1], pcYuvDst.Cb()), PelBuf(m_cRefSamplesDMVRL0[2], pcYuvDst.Cr())));
    m_cYuvRefBuffDMVRL0 = m_cYuvRefBuffDMVRL0.subBuf(UnitAreaRelative(pu, subPu));

    m_cYuvRefBuffDMVRL1 = (pu.chromaFormat == CHROMA_400 ?
      PelUnitBuf(pu.chromaFormat, PelBuf(m_cRefSamplesDMVRL1[0], pcYuvDst.Y())) :
      PelUnitBuf(pu.chromaFormat, PelBuf(m_cRefSamplesDMVRL1[0], pcYuvDst.Y()), PelBuf(m_cRefSamplesDMVRL1[1], pcYuvDst.Cb()),
        PelBuf(m_cRefSamplesDMVRL1[2], pcYuvDst.Cr())));
    m_cYuvRefBuffDMVRL1 = m_cYuvRefBuffDMVRL1.subBuf(UnitAreaRelative(pu, subPu));

    PelUnitBuf srcPred0 = (pu.chromaFormat == CHROMA_400 ?
      PelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[0][0], pcYuvDst.Y())) :
      PelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[0][0], pcYuvDst.Y()), PelBuf(m_acYuvPred[0][1], pcYuvDst.Cb()), PelBuf(m_acYuvPred[0][2], pcYuvDst.Cr())));
    PelUnitBuf srcPred1 = (pu.chromaFormat == CHROMA_400 ?
      PelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[1][0], pcYuvDst.Y())) :
      PelUnitBuf(pu.chromaFormat, PelBuf(m_acYuvPred[1][0], pcYuvDst.Y()), PelBuf(m_acYuvPred[1][1], pcYuvDst.Cb()), PelBuf(m_acYuvPred[1][2], pcYuvDst.Cr())));

    srcPred0 = srcPred0.subBuf(UnitAreaRelative(pu, subPu));
    srcPred1 = srcPred1.subBuf(UnitAreaRelative(pu, subPu));

    int yStart = 0;
    // dont use calculateSum
    for (int y = puPos.y; y < (puPos.y + pu.lumaSize().height); y = y + dy, yStart = yStart + dy)
    {
      // dont use calculateSum
      for (int x = puPos.x, xStart = 0; x < (puPos.x + pu.lumaSize().width); x = x + dx, xStart = xStart + dx)
      {
        PredictionUnit subPu = pu;
        subPu.UnitArea::operator=(UnitArea(pu.chromaFormat, Area(x, y, dx, dy)));
        xPrefetch(subPu, m_cYuvRefBuffDMVRL0, REF_PIC_LIST_0, 1);
        xPrefetch(subPu, m_cYuvRefBuffDMVRL1, REF_PIC_LIST_1, 1);

        xinitMC(subPu, clpRngs);

        uint64_t minCost = MAX_UINT64;
        bool notZeroCost = true;
        int16_t totalDeltaMV[2] = { 0,0 };
        int16_t deltaMV[2] = { 0, 0 };
        uint64_t  *pSADsArray;
        for (int i = 0; i < (((2 * DMVR_NUM_ITERATION) + 1) * ((2 * DMVR_NUM_ITERATION) + 1)); i++)
        {
          m_SADsArray[i] = MAX_UINT64;
        }
        pSADsArray = &m_SADsArray[(((2 * DMVR_NUM_ITERATION) + 1) * ((2 * DMVR_NUM_ITERATION) + 1)) >> 1];
        for (int i = 0; i < iterationCount; i++)
        {
          deltaMV[0] = 0;
          deltaMV[1] = 0;
          Pel *addrL0 = biLinearPredL0 + totalDeltaMV[0] + (totalDeltaMV[1] * m_biLinearBufStride);
          //Pel *addrL0 = biLinearPredL0 + calculateSum((int) totalDeltaMV[0] , (int) (totalDeltaMV[1] * m_biLinearBufStride), 8, 7);
          Pel *addrL1 = biLinearPredL1 - totalDeltaMV[0] - (totalDeltaMV[1] * m_biLinearBufStride);
          //Pel *addrL1 = biLinearPredL1 - calculateSum((int) totalDeltaMV[0] , -(int) (totalDeltaMV[1] * m_biLinearBufStride), 8, 7);
          if (i == 0)
          {
            minCost = xDMVRCost(clpRngs.comp[COMPONENT_Y].bd, addrL0, m_biLinearBufStride, addrL1, m_biLinearBufStride, dx, dy);
            minCost -= (minCost >>2);
            if (minCost < (dx * dy))
            {
              notZeroCost = false;
              break;
            }
            pSADsArray[0] = minCost;
          }
          if (!minCost)
          {
            notZeroCost = false;
            break;
          }

          xBIPMVRefine(bd, addrL0, addrL1, minCost, deltaMV, pSADsArray, dx, dy);

          if (deltaMV[0] == 0 && deltaMV[1] == 0)
          {
            break;
          }
          totalDeltaMV[0] += deltaMV[0];
          //totalDeltaMV[0] = calculateSum((int) totalDeltaMV[0] ,(int) deltaMV[0], 8, 7);
          totalDeltaMV[1] += deltaMV[1];
          //totalDeltaMV[1] = calculateSum(totalDeltaMV[1] , deltaMV[1], 8, 7);
          pSADsArray += ((deltaMV[1] * (((2 * DMVR_NUM_ITERATION) + 1))) + deltaMV[0]);
          //pSADsArray += (calculateSum((deltaMV[1] * (((2 * DMVR_NUM_ITERATION) + 1))), deltaMV[0], 8, 7));
          pSADsArray += ((deltaMV[1] * (((2 * DMVR_NUM_ITERATION) + 1))) + deltaMV[0]);
          //pSADsArray += (calculateSum((deltaMV[1] * (((2 * DMVR_NUM_ITERATION) + 1))) , deltaMV[0], 8, 7));
        }

        bioAppliedType[num] = (minCost < bioEnabledThres) ? false : bioApplied;
        totalDeltaMV[0] = (totalDeltaMV[0] << mvShift);
        totalDeltaMV[1] = (totalDeltaMV[1] << mvShift);
        xDMVRSubPixelErrorSurface(notZeroCost, totalDeltaMV, deltaMV, pSADsArray);

        pu.mvdL0SubPu[num] = Mv(totalDeltaMV[0], totalDeltaMV[1]);
        PelUnitBuf subPredBuf = pcYuvDst.subBuf(UnitAreaRelative(pu, subPu));

        bool blockMoved = false;
        if (pu.mvdL0SubPu[num] != Mv(0, 0))
        {
          blockMoved = true;
          xPrefetch(subPu, m_cYuvRefBuffDMVRL0, REF_PIC_LIST_0, 0);
          xPrefetch(subPu, m_cYuvRefBuffDMVRL1, REF_PIC_LIST_1, 0);
          xPad(subPu, m_cYuvRefBuffDMVRL0, REF_PIC_LIST_0);
          xPad(subPu, m_cYuvRefBuffDMVRL1, REF_PIC_LIST_1);
        }

        int dstStride[MAX_NUM_COMPONENT] = { pcYuvDst.bufs[COMPONENT_Y].stride, pcYuvDst.bufs[COMPONENT_Cb].stride, pcYuvDst.bufs[COMPONENT_Cr].stride };
        subPu.mv[0] = mergeMv[REF_PIC_LIST_0] + pu.mvdL0SubPu[num];
        subPu.mv[1] = mergeMv[REF_PIC_LIST_1] - pu.mvdL0SubPu[num];

        subPu.mv[0].clipToStorageBitDepth();
        subPu.mv[1].clipToStorageBitDepth();

        xFinalPaddedMCForDMVR(subPu, srcPred0, srcPred1, m_cYuvRefBuffDMVRL0, m_cYuvRefBuffDMVRL1, bioAppliedType[num], mergeMv
          , blockMoved
        );

       subPredBuf.bufs[COMPONENT_Y].buf = pcYuvDst.bufs[COMPONENT_Y].buf + xStart + yStart * dstStride[COMPONENT_Y];
       //subPredBuf.bufs[COMPONENT_Y].buf = pcYuvDst.bufs[COMPONENT_Y].buf + calculateSum(xStart , yStart * dstStride[COMPONENT_Y], 8 ,7);

        subPredBuf.bufs[COMPONENT_Cb].buf = pcYuvDst.bufs[COMPONENT_Cb].buf + (xStart >> scaleX) + ((yStart >> scaleY) * dstStride[COMPONENT_Cb]);
        //subPredBuf.bufs[COMPONENT_Cb].buf = pcYuvDst.bufs[COMPONENT_Cb].buf + calculateSum((xStart >> scaleX) , ((yStart >> scaleY) * dstStride[COMPONENT_Cb]), 8 ,7);

        subPredBuf.bufs[COMPONENT_Cr].buf = pcYuvDst.bufs[COMPONENT_Cr].buf + (xStart >> scaleX) + ((yStart >> scaleY) * dstStride[COMPONENT_Cr]);
        //subPredBuf.bufs[COMPONENT_Cr].buf = pcYuvDst.bufs[COMPONENT_Cr].buf + calculateSum((xStart >> scaleX) , ((yStart >> scaleY) * dstStride[COMPONENT_Cr]), 8 ,7);

        xWeightedAverage(subPu, srcPred0, srcPred1, subPredBuf, subPu.cu->slice->getSPS()->getBitDepths(), subPu.cu->slice->clpRngs(), bioAppliedType[num]);
        num++;
      }
    }
  }
  JVET_J0090_SET_CACHE_ENABLE(true);
  auto stop                    = high_resolution_clock::now();
  auto duration                = duration_cast<nanoseconds>(stop - start);
  timeOfXProcessDMVR           = timeOfXProcessDMVR + duration.count();
}
#if JVET_J0090_MEMORY_BANDWITH_MEASURE
void InterPrediction::cacheAssign( CacheModel *cache )
{
  m_cacheModel = cache;
  m_if.cacheAssign( cache );
  m_if.initInterpolationFilter( !cache->isCacheEnable() );
}
#endif

void InterPrediction::xFillIBCBuffer(CodingUnit &cu)
{
  for (auto &currPU : CU::traverseTUs(cu))
  {
    for (const CompArea &area : currPU.blocks)
    {
      if (!area.valid())
        continue;

      const unsigned int lcuWidth = cu.cs->slice->getSPS()->getMaxCUWidth();
      const int shiftSample = ::getComponentScaleX(area.compID, cu.chromaFormat);
      const int ctuSizeLog2 = floorLog2(lcuWidth) - shiftSample;
      const int pux = area.x & ((m_IBCBufferWidth >> shiftSample) - 1);
      const int puy = area.y & (( 1 << ctuSizeLog2 ) - 1);
      const CompArea dstArea = CompArea(area.compID, cu.chromaFormat, Position(pux, puy), Size(area.width, area.height));
      CPelBuf srcBuf = cu.cs->getRecoBuf(area);
      PelBuf dstBuf = m_IBCBuffer.getBuf(dstArea);

      dstBuf.copyFrom(srcBuf);
    }
  }
}

void InterPrediction::xIntraBlockCopy(PredictionUnit &pu, PelUnitBuf &predBuf, const ComponentID compID)
{
  const unsigned int lcuWidth = pu.cs->slice->getSPS()->getMaxCUWidth();
  int shiftSample = ::getComponentScaleX(compID, pu.chromaFormat);
  const int ctuSizeLog2 = floorLog2(lcuWidth) - shiftSample;
  pu.bv = pu.mv[REF_PIC_LIST_0];
  pu.bv.changePrecision(MV_PRECISION_INTERNAL, MV_PRECISION_INT);
  int refx, refy;
  if (compID == COMPONENT_Y)
  {
    refx = pu.Y().x + pu.bv.hor;
    refy = pu.Y().y + pu.bv.ver;
  }
  else
  {//Cb or Cr
    refx = pu.Cb().x + (pu.bv.hor >> shiftSample);
    refy = pu.Cb().y + (pu.bv.ver >> shiftSample);
  }
  refx &= ((m_IBCBufferWidth >> shiftSample) - 1);
  refy &= ((1 << ctuSizeLog2) - 1);

  if (refx + predBuf.bufs[compID].width <= (m_IBCBufferWidth >> shiftSample))
  {
    const CompArea srcArea = CompArea(compID, pu.chromaFormat, Position(refx, refy), Size(predBuf.bufs[compID].width, predBuf.bufs[compID].height));
    const CPelBuf refBuf = m_IBCBuffer.getBuf(srcArea);
    predBuf.bufs[compID].copyFrom(refBuf);
  }
  else
  {//wrap around
    int width = (m_IBCBufferWidth >> shiftSample) - refx;
    CompArea srcArea = CompArea(compID, pu.chromaFormat, Position(refx, refy), Size(width, predBuf.bufs[compID].height));
    CPelBuf srcBuf = m_IBCBuffer.getBuf(srcArea);
    PelBuf dstBuf = PelBuf(predBuf.bufs[compID].bufAt(Position(0, 0)), predBuf.bufs[compID].stride, Size(width, predBuf.bufs[compID].height));
    dstBuf.copyFrom(srcBuf);

    width = refx + predBuf.bufs[compID].width - (m_IBCBufferWidth >> shiftSample);
    srcArea = CompArea(compID, pu.chromaFormat, Position(0, refy), Size(width, predBuf.bufs[compID].height));
    srcBuf = m_IBCBuffer.getBuf(srcArea);
    dstBuf = PelBuf(predBuf.bufs[compID].bufAt(Position((m_IBCBufferWidth >> shiftSample) - refx, 0)), predBuf.bufs[compID].stride, Size(width, predBuf.bufs[compID].height));
    dstBuf.copyFrom(srcBuf);
  }
}

void InterPrediction::resetIBCBuffer(const ChromaFormat chromaFormatIDC, const int ctuSize)
{
  const UnitArea area = UnitArea(chromaFormatIDC, Area(0, 0, m_IBCBufferWidth, ctuSize));
  m_IBCBuffer.getBuf(area).fill(-1);
}

void InterPrediction::resetVPDUforIBC(const ChromaFormat chromaFormatIDC, const int ctuSize, const int vSize, const int xPos, const int yPos)
{
  const UnitArea area = UnitArea(chromaFormatIDC, Area(xPos & (m_IBCBufferWidth - 1), yPos & (ctuSize - 1), vSize, vSize));
  m_IBCBuffer.getBuf(area).fill(-1);
}

bool InterPrediction::isLumaBvValid(const int ctuSize, const int xCb, const int yCb, const int width, const int height, const int xBv, const int yBv)
{
  if(((yCb + yBv) & (ctuSize - 1)) + height > ctuSize)
  {
    return false;
  }
  int refTLx = xCb + xBv;
  int refTLy = (yCb + yBv) & (ctuSize - 1);
  PelBuf buf = m_IBCBuffer.Y();
  for(int x = 0; x < width; x += 4)
  {
    for(int y = 0; y < height; y += 4)
    {
      if(buf.at((x + refTLx) & (m_IBCBufferWidth - 1), y + refTLy) == -1) return false;
      if(buf.at((x + 3 + refTLx) & (m_IBCBufferWidth - 1), y + refTLy) == -1) return false;
      if(buf.at((x + refTLx) & (m_IBCBufferWidth - 1), y + 3 + refTLy) == -1) return false;
      if(buf.at((x + 3 + refTLx) & (m_IBCBufferWidth - 1), y + 3 + refTLy) == -1) return false;
    }
  }
  return true;
}

bool InterPrediction::xPredInterBlkRPR( const std::pair<int, int>& scalingRatio, const PPS& pps, const CompArea &blk, const Picture* refPic, const Mv& mv, Pel* dst, const int dstStride, const bool bi, const bool wrapRef, const ClpRng& clpRng, const int filterIndex, const bool useAltHpelIf )
{
  const ChromaFormat  chFmt = blk.chromaFormat;
  const ComponentID compID = blk.compID;
  const bool          rndRes = !bi;

  int shiftHor = MV_FRACTIONAL_BITS_INTERNAL + ::getComponentScaleX( compID, chFmt );
  int shiftVer = MV_FRACTIONAL_BITS_INTERNAL + ::getComponentScaleY( compID, chFmt );

  int width = blk.width;
  int height = blk.height;
  CPelBuf refBuf;

  const bool scaled = scalingRatio != SCALE_1X;

  if( scaled )
  {
    int row, col;
    int refPicWidth = refPic->cs->pps->getPicWidthInLumaSamples();
    int refPicHeight = refPic->cs->pps->getPicHeightInLumaSamples();

    const int posShift = SCALE_RATIO_BITS - 4;
    int stepX = ( scalingRatio.first + 8 ) >> 4;
    int stepY = ( scalingRatio.second + 8 ) >> 4;
    int64_t x0Int;
    int64_t y0Int;
    int offX = 1 << ( posShift - shiftHor - 1 );
    int offY = 1 << ( posShift - shiftVer - 1 );

    x0Int = ( ( blk.pos().x << ( 4 + ::getComponentScaleX( compID, chFmt ) ) ) + mv.getHor() )* scalingRatio.first;
    x0Int = SIGN( x0Int ) * ( ( llabs( x0Int ) + ( (long long)1 << ( 7 + ::getComponentScaleX( compID, chFmt ) ) ) ) >> ( 8 + ::getComponentScaleX( compID, chFmt ) ) );

    y0Int = ( ( blk.pos().y << ( 4 + ::getComponentScaleY( compID, chFmt ) ) ) + mv.getVer() )* scalingRatio.second;
    y0Int = SIGN( y0Int ) * ( ( llabs( y0Int ) + ( (long long)1 << ( 7 + ::getComponentScaleY( compID, chFmt ) ) ) ) >> ( 8 + ::getComponentScaleY( compID, chFmt ) ) );

    const int extSize = isLuma( compID ) ? 1 : 2;

    int vFilterSize = isLuma( compID ) ? NTAPS_LUMA : NTAPS_CHROMA;

    int refHeight = height * scalingRatio.second >> SCALE_RATIO_BITS;
    refHeight = std::max<int>( 1, refHeight );

    CHECK( MAX_CU_SIZE * MAX_SCALING_RATIO < refHeight + vFilterSize - 1 + extSize, "Buffer size is not enough, increase MAX_SCALING_RATIO" );

    Pel buffer[( MAX_CU_SIZE + 16 ) * ( MAX_CU_SIZE * MAX_SCALING_RATIO + 16 )];

    int tmpStride = width;

    int yInt0 = ( (int32_t)y0Int + offY ) >> posShift;
    yInt0 = std::min( std::max( 0, yInt0 ), ( refPicHeight >> ::getComponentScaleY( compID, chFmt ) ) );

    int xInt0 = ( (int32_t)x0Int + offX ) >> posShift;
    xInt0 = std::min( std::max( 0, xInt0 ), ( refPicWidth >> ::getComponentScaleX( compID, chFmt ) ) );

    int xInt = 0, yInt = 0;

    for( col = 0; col < width; col++ )
    {
      int posX = (int32_t)x0Int + col * stepX;
      xInt = ( posX + offX ) >> posShift;
      xInt = std::min( std::max( 0, xInt ), ( refPicWidth >> ::getComponentScaleX( compID, chFmt ) ) );
      int xFrac = ( ( posX + offX ) >> ( posShift - shiftHor ) ) & ( ( 1 << shiftHor ) - 1 );

      CHECK( xInt0 > xInt, "Wrong horizontal starting point" );

      Position offset = Position( xInt, yInt0 );
      refBuf = refPic->getRecoBuf( CompArea( compID, chFmt, offset, Size( 1, refHeight ) ), wrapRef );
      Pel* tempBuf = buffer + col;

      m_if.filterHor( compID, (Pel*)refBuf.buf - ( ( vFilterSize >> 1 ) - 1 ) * refBuf.stride, refBuf.stride, tempBuf, tmpStride, 1, refHeight + vFilterSize - 1 + extSize, xFrac, false, chFmt, clpRng, filterIndex, false, useAltHpelIf );
    }

    for( row = 0; row < height; row++ )
    {
      int posY = (int32_t)y0Int + row * stepY;
      yInt = ( posY + offY ) >> posShift;
      yInt = std::min( std::max( 0, yInt ), ( refPicHeight >> ::getComponentScaleY( compID, chFmt ) ) );
      int yFrac = ( ( posY + offY ) >> ( posShift - shiftVer ) ) & ( ( 1 << shiftVer ) - 1 );

      CHECK( yInt0 > yInt, "Wrong vertical starting point" );

      Pel* tempBuf = buffer + ( yInt - yInt0 ) * tmpStride;

      JVET_J0090_SET_CACHE_ENABLE( false );
      m_if.filterVer( compID, tempBuf + ( ( vFilterSize >> 1 ) - 1 ) * tmpStride, tmpStride, dst + row * dstStride, dstStride, width, 1, yFrac, false, rndRes, chFmt, clpRng, filterIndex, false, useAltHpelIf );
      JVET_J0090_SET_CACHE_ENABLE( true );
    }

    Position offset = Position( xInt, yInt );
    refBuf = refPic->getRecoBuf( CompArea( compID, chFmt, offset, Size( 1, 1 ) ), wrapRef );
  }

  return scaled;
}
