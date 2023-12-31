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

/** \file     MatrixIntraPrediction.cpp
\brief    matrix-based intra prediction class
*/

#include "MatrixIntraPrediction.h"
#include "dtrace_next.h"

#include "UnitTools.h"
#include "MipData.h"


MatrixIntraPrediction::MatrixIntraPrediction():
  m_reducedBoundary          (MIP_MAX_INPUT_SIZE),
  m_reducedBoundaryTransposed(MIP_MAX_INPUT_SIZE),
  m_inputOffset      ( 0 ),
  m_inputOffsetTransp( 0 ),
  m_refSamplesTop (MIP_MAX_WIDTH),
  m_refSamplesLeft(MIP_MAX_HEIGHT),
  m_blockSize( 0, 0 ),
  m_numModes( 0 ),
  m_reducedBoundarySize( 0, 0 ),
  m_reducedPredictionSize( 0, 0 ),
  m_upsmpFactorHor( 0 ),
  m_upsmpFactorVer( 0 )
{
}


void MatrixIntraPrediction::prepareInputForPred(const CPelBuf &pSrc, const Area& block, const int bitDepth)
{
  // Step 1: Save block size and calculate dependent values
  initPredBlockParams(block);

  // Step 2: Get the input data (left and top reference samples)
  m_refSamplesTop.resize(block.width);
  for (int x = 0; x < block.width; x++)
  {
    m_refSamplesTop[x] = pSrc.at(x + 1, 0);
  }

  m_refSamplesLeft.resize(block.height);
  for (int y = 0; y < block.height; y++)
  {
    m_refSamplesLeft[y] = pSrc.at(y + 1, 1);
  }

  // Step 3: Compute the reduced boundary via Haar-downsampling (input for the prediction)
  m_reducedBoundary          .resize( m_reducedBoundarySize.width + m_reducedBoundarySize.height );
  m_reducedBoundaryTransposed.resize( m_reducedBoundarySize.width + m_reducedBoundarySize.height );

  int* const topReduced = m_reducedBoundary.data();
  boundaryDownsampling1D(topReduced, m_refSamplesTop.data(), block.width, m_reducedBoundarySize.width);

  int* const leftReduced = m_reducedBoundary.data() + m_reducedBoundarySize.width;
  boundaryDownsampling1D(leftReduced, m_refSamplesLeft.data(), block.height, m_reducedBoundarySize.height);

  int* const leftReducedTransposed = m_reducedBoundaryTransposed.data();
  int* const topReducedTransposed  = m_reducedBoundaryTransposed.data() + m_reducedBoundarySize.height;
  for( int x = 0; x < m_reducedBoundarySize.width; x++ )
  {
    topReducedTransposed[ x ] = topReduced[ x ];
  }
  for( int y = 0; y < m_reducedBoundarySize.height; y++ )
  {
    leftReducedTransposed[ y ] = leftReduced[ y ];
  }

  // Step 4: Rebase the reduced boundary
  const int inputSize = m_reducedBoundarySize.width + m_reducedBoundarySize.height;

  m_inputOffset       = m_reducedBoundary[0];
  m_inputOffsetTransp = m_reducedBoundaryTransposed[0];

  const bool hasFirstCol = (m_blockSize.width <= 8 && m_blockSize.height <= 8);
  m_reducedBoundary          [0] = hasFirstCol ? (m_inputOffset       - (1 << (bitDepth - 1))) : 0; // first column of matrix not needed for large blocks
  m_reducedBoundaryTransposed[0] = hasFirstCol ? (m_inputOffsetTransp - (1 << (bitDepth - 1))) : 0;
  for (int i = 1; i < inputSize; i++)
  {
    m_reducedBoundary          [i] -= m_inputOffset;
    m_reducedBoundaryTransposed[i] -= m_inputOffsetTransp;
  }
}

void MatrixIntraPrediction::predBlock(int* const result, const int modeIdx, const int bitDepth)
{
  const bool transpose = isTransposed( modeIdx );
  const bool needUpsampling = ( m_upsmpFactorHor > 1 ) || ( m_upsmpFactorVer > 1 );

  const uint8_t* matrix;
  int shiftMatrix = 0, offsetMatrix = 0;
  getMatrixData(matrix, shiftMatrix, offsetMatrix, modeIdx);

  bool leaveHorOut = ( m_blockSize.width == 4 && m_blockSize.height >= 16 );
  bool leaveVerOut = ( m_blockSize.height == 4 && m_blockSize.width >= 16 );
  if (transpose)
  {
    std::swap(leaveHorOut, leaveVerOut);
  }
  static_vector<int, MIP_MAX_REDUCED_OUTPUT_SAMPLES> bufReducedPred( m_reducedPredictionSize.area() );
  int* const       reducedPred     = needUpsampling ? bufReducedPred.data() : result;
  const int* const reducedBoundary = transpose ? m_reducedBoundaryTransposed.data() : m_reducedBoundary.data();
  computeReducedPred( reducedPred, reducedBoundary, matrix, leaveHorOut, leaveVerOut, shiftMatrix, offsetMatrix,
                      transpose, needUpsampling, bitDepth );
  // Reduced prediction is transposed if ( transpose && needUpsampling ).

  if( needUpsampling )
  {
    predictionUpsampling( result, reducedPred, transpose );
  }
}

bool MatrixIntraPrediction::isTransposed(const int modeIdx) const
{
  return ( modeIdx > ( m_numModes / 2 ) );
}

int MatrixIntraPrediction::getWeightIdx(const int modeIdx) const
{
  if( modeIdx > m_numModes / 2 )
  {
    return modeIdx - m_numModes / 2;
  }
  else
  {
    return modeIdx;
  }
}

void MatrixIntraPrediction::initPredBlockParams(const Size& block)
{
  m_blockSize = block;
  m_numModes  = getNumModesMip(m_blockSize);

  // init reduced boundary size
  if (m_blockSize.width > 4 || m_blockSize.height > 4)
  {
    m_reducedBoundarySize = Size(4, 4);
  }
  else
  {
    m_reducedBoundarySize = Size(2, 2);
  }

  // init reduced prediction size
  if (m_blockSize.width <= 8 && m_blockSize.height <= 8)
  {
    m_reducedPredictionSize = Size(4, 4);
  }
  else
  {
    m_reducedPredictionSize = Size(std::min<SizeType>(m_blockSize.width, 8), std::min<SizeType>(m_blockSize.height, 8));
  }


  // init upsampling factors
  m_upsmpFactorHor = m_blockSize.width / m_reducedPredictionSize.width;
  CHECKD(m_reducedPredictionSize.width * m_upsmpFactorHor != m_blockSize.width, "Need integer horizontal upsampling factor.");
  CHECKD((m_upsmpFactorHor < 1) || ((m_upsmpFactorHor & (m_upsmpFactorHor - 1)) != 0), "Need power of two horizontal upsampling factor.");

  m_upsmpFactorVer = m_blockSize.height / m_reducedPredictionSize.height;
  CHECKD(m_reducedPredictionSize.height * m_upsmpFactorVer != m_blockSize.height, "Need integer vertical upsampling factor.");
  CHECKD((m_upsmpFactorVer < 1) || ((m_upsmpFactorVer & (m_upsmpFactorVer - 1)) != 0), "Need power of two vertical upsampling factor.");
}

void MatrixIntraPrediction::doDownsampling(int* dst, const int* src, const SizeType srcLen, const SizeType dstLen)
{
  const SizeType downsmpFactor = srcLen / dstLen;
  CHECKD( srcLen != dstLen * downsmpFactor, "Need integer downsampling factor." );
  CHECKD( ( downsmpFactor & ( downsmpFactor - 1 ) ) != 0, "Need power of two downsampling factor." );
  const int log2DownsmpFactor = floorLog2( downsmpFactor );
  const int roundingOffset = ( 1 << ( log2DownsmpFactor - 1 ) );

  for( SizeType srcIdx = 0, dstIdx = 0; dstIdx < dstLen; ++dstIdx )
  {
    int sum = 0;
    for( SizeType blockIdx = 0; blockIdx < downsmpFactor; ++blockIdx, ++srcIdx )
    {
      sum += src[ srcIdx ];
    }
    dst[ dstIdx ] = ( sum + roundingOffset ) >> log2DownsmpFactor;
  }
}


void MatrixIntraPrediction::boundaryDownsampling1D(int* reducedDst, const int* const fullSrc, const SizeType srcLen, const SizeType dstLen)
{
  if (dstLen < srcLen)
  {
    // Create reduced boundary by downsampling
    doDownsampling(reducedDst, fullSrc, srcLen, dstLen);
  }
  else
  {
    // Copy boundary if no downsampling is needed
    for (SizeType i = 0; i < dstLen; ++i)
    {
      reducedDst[i] = fullSrc[i];
    }
  }
}


void MatrixIntraPrediction::predictionUpsampling1D(int* const dst, const int* const src, const int* const bndry,
                                                   const SizeType srcSizeUpsmpDim, const SizeType srcSizeOrthDim,
                                                   const SizeType srcStep, const SizeType srcStride,
                                                   const SizeType dstStep, const SizeType dstStride,
                                                   const SizeType bndryStep,
                                                   const unsigned int upsmpFactor )
{
  const int log2UpsmpFactor = floorLog2( upsmpFactor );
  CHECKD( upsmpFactor <= 1, "Upsampling factor must be at least 2." );
  const int roundingOffset = 1 << (log2UpsmpFactor - 1);

  SizeType idxOrthDim = 0;
  const int* srcLine = src;
  int* dstLine = dst;
  const int* bndryLine = bndry + bndryStep - 1;
  while( idxOrthDim < srcSizeOrthDim )
  {
    SizeType idxUpsmpDim = 0;
    const int* before = bndryLine;
    const int* behind = srcLine;
    int* currDst = dstLine;
    while( idxUpsmpDim < srcSizeUpsmpDim )
    {
      SizeType pos = 1;
      int scaledBefore = ( *before ) << log2UpsmpFactor;
      int scaledBehind = 0;
      while( pos <= upsmpFactor )
      {
        scaledBefore -= *before;
        scaledBehind += *behind;
        *currDst = (scaledBefore + scaledBehind + roundingOffset) >> log2UpsmpFactor;

        pos++;
        currDst += dstStep;
      }

      idxUpsmpDim++;
      before = behind;
      behind += srcStep;
    }

    idxOrthDim++;
    srcLine += srcStride;
    dstLine += dstStride;
    bndryLine += bndryStep;
  }
}


void MatrixIntraPrediction::predictionUpsampling(int* const dst, const int* const src, const bool transpose) const
{
  // shorter side is upsampled first
  if( m_blockSize.height > m_blockSize.width )
  {
    const int* verSrc       = nullptr;
    SizeType   verSrcStep   = 0;
    SizeType   verSrcStride = 0;
    if( m_upsmpFactorHor > 1 )
    {
      const SizeType horSrcStep   = transpose ? m_reducedPredictionSize.height : 1;
      const SizeType horSrcStride = transpose ? 1 : m_reducedPredictionSize.width;

      int* const     horDst       = dst + ( m_upsmpFactorVer - 1 ) * m_blockSize.width;
      const SizeType horDstStride = m_upsmpFactorVer * m_blockSize.width;

     predictionUpsampling1D( horDst, src, m_refSamplesLeft.data(),
                             m_reducedPredictionSize.width, m_reducedPredictionSize.height,
                             horSrcStep, horSrcStride, 1, horDstStride,
                             m_upsmpFactorVer, m_upsmpFactorHor );

      verSrc       = horDst;
      verSrcStep   = horDstStride;
      verSrcStride = 1;
    }
    else
    {
      verSrc       = src;
      verSrcStep   = transpose ? 1 : m_blockSize.width;
      verSrcStride = transpose ? m_reducedPredictionSize.height : 1;
    }
    predictionUpsampling1D( dst, verSrc, m_refSamplesTop.data(),
                            m_reducedPredictionSize.height, m_blockSize.width,
                            verSrcStep, verSrcStride, m_blockSize.width, 1,
                            1, m_upsmpFactorVer );
  }
  else
  {
    const int* horSrc = nullptr;
    SizeType   horSrcStep = 0;
    SizeType   horSrcStride = 0;
    if( m_upsmpFactorVer > 1 )
    {
      const SizeType verSrcStep   = transpose ? 1 : m_reducedPredictionSize.width;
      const SizeType verSrcStride = transpose ? m_reducedPredictionSize.height : 1;

      int* const     verDst       = dst + ( m_upsmpFactorHor - 1 );
      const SizeType verDstStep   = m_blockSize.width;
      const SizeType verDstStride = m_upsmpFactorHor;

      predictionUpsampling1D( verDst, src, m_refSamplesTop.data(),
                              m_reducedPredictionSize.height, m_reducedPredictionSize.width,
                              verSrcStep, verSrcStride, verDstStep, verDstStride,
                              m_upsmpFactorHor, m_upsmpFactorVer );

      horSrc = verDst;
      horSrcStep = verDstStride;
      horSrcStride = verDstStep;
    }
    else
    {
      horSrc       = src;
      horSrcStep   = transpose ? m_blockSize.height : 1;
      horSrcStride = transpose ? 1 : m_reducedPredictionSize.width;
    }
    predictionUpsampling1D( dst, horSrc, m_refSamplesLeft.data(),
                            m_reducedPredictionSize.width, m_blockSize.height,
                            horSrcStep, horSrcStride, 1, m_blockSize.width,
                            1, m_upsmpFactorHor );
  }
}

void MatrixIntraPrediction::getMatrixData(const uint8_t*& matrix, int &shiftMatrix, int &offsetMatrix, const int modeIdx) const
{
  const int idx = getWeightIdx( modeIdx );
  if( m_blockSize.width == 4 && m_blockSize.height == 4 )
  {
    matrix       = &mipMatrix4x4      [idx][0][0];
    shiftMatrix  =  mipShiftMatrix4x4 [idx];
    offsetMatrix =  mipOffsetMatrix4x4[idx];
  }
  else if( m_blockSize.width <= 8 && m_blockSize.height <= 8 )
  {
    matrix       = &mipMatrix8x8      [idx][0][0];
    shiftMatrix  =  mipShiftMatrix8x8 [idx];
    offsetMatrix =  mipOffsetMatrix8x8[idx];
  }
  else
  {
    matrix       = &mipMatrix16x16      [idx][0][0];
    shiftMatrix  =  mipShiftMatrix16x16 [idx];
    offsetMatrix =  mipOffsetMatrix16x16[idx];
  }
}

void MatrixIntraPrediction::computeReducedPred(int*const result, const int* const input, const uint8_t*matrix,
                                               const bool leaveHorOut, const bool leaveVerOut,
                                               const int shiftMatrix, const int offsetMatrix,
                                               const bool transpose, const bool needUpsampling, const int bitDepth )
{
  const int inputSize = m_reducedBoundarySize.width + m_reducedBoundarySize.height;

  // Use local buffer for transposed result if no upsampling will be done.
  static_vector<int, MIP_MAX_REDUCED_OUTPUT_SAMPLES> resBufTransposed( m_reducedPredictionSize.area() );
  int*const resPtr = (transpose && !needUpsampling) ? resBufTransposed.data() : result;

  int sum = 0;
  for (int i = 0; i < inputSize; i++) { sum += input[i]; }
  const int offset = (1 << (shiftMatrix - 1)) - offsetMatrix * sum;
  CHECK(inputSize != 4 * (inputSize >> 2), "Error, input size not divisible by four");

  const uint8_t *weight = matrix;
  const int   inputOffset = transpose ? m_inputOffsetTransp : m_inputOffset;

  const int intermediateWidth  = transpose ? m_reducedPredictionSize.height : m_reducedPredictionSize.width;
  const int intermediateHeight = transpose ? m_reducedPredictionSize.width : m_reducedPredictionSize.height;
  const int xStep = leaveHorOut ? 2 : 1;
  const int yStep = leaveVerOut ? intermediateWidth : 0;

  const int redSize = (m_blockSize.width <= 8 && m_blockSize.height <= 8) ? 0 : 1;
  if ( redSize ) weight += xStep-1;
  int posRes  = 0;
  for (int y = 0; y < intermediateHeight; y++)
  {
    for (int x = 0; x < intermediateWidth; x++)
    {
      if(redSize) weight -= xStep;
      int tmp0 = redSize ? 0 : (input[0] * weight[0]);
      int tmp1 = input[1] * weight[1];
      int tmp2 = input[2] * weight[2];
      int tmp3 = input[3] * weight[3];
      for (int i = 4; i < inputSize; i += 4)
      {
        tmp0 += input[i]     * weight[i];
        tmp1 += input[i + 1] * weight[i + 1];
        tmp2 += input[i + 2] * weight[i + 2];
        tmp3 += input[i + 3] * weight[i + 3];
      }
      resPtr[posRes++] = ClipBD<int>( ((tmp0 + tmp1 + tmp2 + tmp3 + offset) >> shiftMatrix) + inputOffset, bitDepth );

      weight  += xStep * inputSize;
    }
    weight  += yStep * (inputSize - redSize);
  }

  // Re-transpose if no upsampling will be done.
  if( transpose && !needUpsampling )
  {
    for( int y = 0; y < m_reducedPredictionSize.height; y++ )
    {
      for( int x = 0; x < m_reducedPredictionSize.width; x++ )
      {
        CHECKD( x * m_reducedPredictionSize.height + y >= m_reducedPredictionSize.area(), "error" );
        result[ y * m_reducedPredictionSize.width + x ] = resPtr[ x * m_reducedPredictionSize.height + y ];
      }
    }
  }
}

