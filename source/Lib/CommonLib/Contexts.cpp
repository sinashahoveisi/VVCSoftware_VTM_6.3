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

/** \file     Contexts.cpp
 *  \brief    Classes providing probability descriptions and contexts (also contains context initialization values)
 */

#include "Contexts.h"
#include "ApproximatAdderSubtraction.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <chrono>

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::nanoseconds;

long long int timeOfBinProbModel_Std = 0;


const uint8_t ProbModelTables::m_RenormTable_32[32] =
{
  6,  5,  4,  4,
  3,  3,  3,  3,
  2,  2,  2,  2,
  2,  2,  2,  2,
  1,  1,  1,  1,
  1,  1,  1,  1,
  1,  1,  1,  1,
  1,  1,  1,  1
};

const BinFracBits ProbModelTables::m_binFracBits[256] = {
  { { 0x0005c, 0x48000 } }, { { 0x00116, 0x3b520 } }, { { 0x001d0, 0x356cb } }, { { 0x0028b, 0x318a9 } },
  { { 0x00346, 0x2ea40 } }, { { 0x00403, 0x2c531 } }, { { 0x004c0, 0x2a658 } }, { { 0x0057e, 0x28beb } },
  { { 0x0063c, 0x274ce } }, { { 0x006fc, 0x26044 } }, { { 0x007bc, 0x24dc9 } }, { { 0x0087d, 0x23cfc } },
  { { 0x0093f, 0x22d96 } }, { { 0x00a01, 0x21f60 } }, { { 0x00ac4, 0x2122e } }, { { 0x00b89, 0x205dd } },
  { { 0x00c4e, 0x1fa51 } }, { { 0x00d13, 0x1ef74 } }, { { 0x00dda, 0x1e531 } }, { { 0x00ea2, 0x1db78 } },
  { { 0x00f6a, 0x1d23c } }, { { 0x01033, 0x1c970 } }, { { 0x010fd, 0x1c10b } }, { { 0x011c8, 0x1b903 } },
  { { 0x01294, 0x1b151 } }, { { 0x01360, 0x1a9ee } }, { { 0x0142e, 0x1a2d4 } }, { { 0x014fc, 0x19bfc } },
  { { 0x015cc, 0x19564 } }, { { 0x0169c, 0x18f06 } }, { { 0x0176d, 0x188de } }, { { 0x0183f, 0x182e8 } },
  { { 0x01912, 0x17d23 } }, { { 0x019e6, 0x1778a } }, { { 0x01abb, 0x1721c } }, { { 0x01b91, 0x16cd5 } },
  { { 0x01c68, 0x167b4 } }, { { 0x01d40, 0x162b6 } }, { { 0x01e19, 0x15dda } }, { { 0x01ef3, 0x1591e } },
  { { 0x01fcd, 0x15480 } }, { { 0x020a9, 0x14fff } }, { { 0x02186, 0x14b99 } }, { { 0x02264, 0x1474e } },
  { { 0x02343, 0x1431b } }, { { 0x02423, 0x13f01 } }, { { 0x02504, 0x13afd } }, { { 0x025e6, 0x1370f } },
  { { 0x026ca, 0x13336 } }, { { 0x027ae, 0x12f71 } }, { { 0x02894, 0x12bc0 } }, { { 0x0297a, 0x12821 } },
  { { 0x02a62, 0x12494 } }, { { 0x02b4b, 0x12118 } }, { { 0x02c35, 0x11dac } }, { { 0x02d20, 0x11a51 } },
  { { 0x02e0c, 0x11704 } }, { { 0x02efa, 0x113c7 } }, { { 0x02fe9, 0x11098 } }, { { 0x030d9, 0x10d77 } },
  { { 0x031ca, 0x10a63 } }, { { 0x032bc, 0x1075c } }, { { 0x033b0, 0x10461 } }, { { 0x034a5, 0x10173 } },
  { { 0x0359b, 0x0fe90 } }, { { 0x03693, 0x0fbb9 } }, { { 0x0378c, 0x0f8ed } }, { { 0x03886, 0x0f62b } },
  { { 0x03981, 0x0f374 } }, { { 0x03a7e, 0x0f0c7 } }, { { 0x03b7c, 0x0ee23 } }, { { 0x03c7c, 0x0eb89 } },
  { { 0x03d7d, 0x0e8f9 } }, { { 0x03e7f, 0x0e671 } }, { { 0x03f83, 0x0e3f2 } }, { { 0x04088, 0x0e17c } },
  { { 0x0418e, 0x0df0e } }, { { 0x04297, 0x0dca8 } }, { { 0x043a0, 0x0da4a } }, { { 0x044ab, 0x0d7f3 } },
  { { 0x045b8, 0x0d5a5 } }, { { 0x046c6, 0x0d35d } }, { { 0x047d6, 0x0d11c } }, { { 0x048e7, 0x0cee3 } },
  { { 0x049fa, 0x0ccb0 } }, { { 0x04b0e, 0x0ca84 } }, { { 0x04c24, 0x0c85e } }, { { 0x04d3c, 0x0c63f } },
  { { 0x04e55, 0x0c426 } }, { { 0x04f71, 0x0c212 } }, { { 0x0508d, 0x0c005 } }, { { 0x051ac, 0x0bdfe } },
  { { 0x052cc, 0x0bbfc } }, { { 0x053ee, 0x0b9ff } }, { { 0x05512, 0x0b808 } }, { { 0x05638, 0x0b617 } },
  { { 0x0575f, 0x0b42a } }, { { 0x05888, 0x0b243 } }, { { 0x059b4, 0x0b061 } }, { { 0x05ae1, 0x0ae83 } },
  { { 0x05c10, 0x0acaa } }, { { 0x05d41, 0x0aad6 } }, { { 0x05e74, 0x0a907 } }, { { 0x05fa9, 0x0a73c } },
  { { 0x060e0, 0x0a575 } }, { { 0x06219, 0x0a3b3 } }, { { 0x06354, 0x0a1f5 } }, { { 0x06491, 0x0a03b } },
  { { 0x065d1, 0x09e85 } }, { { 0x06712, 0x09cd4 } }, { { 0x06856, 0x09b26 } }, { { 0x0699c, 0x0997c } },
  { { 0x06ae4, 0x097d6 } }, { { 0x06c2f, 0x09634 } }, { { 0x06d7c, 0x09495 } }, { { 0x06ecb, 0x092fa } },
  { { 0x0701d, 0x09162 } }, { { 0x07171, 0x08fce } }, { { 0x072c7, 0x08e3e } }, { { 0x07421, 0x08cb0 } },
  { { 0x0757c, 0x08b26 } }, { { 0x076da, 0x089a0 } }, { { 0x0783b, 0x0881c } }, { { 0x0799f, 0x0869c } },
  { { 0x07b05, 0x0851f } }, { { 0x07c6e, 0x083a4 } }, { { 0x07dd9, 0x0822d } }, { { 0x07f48, 0x080b9 } },
  { { 0x080b9, 0x07f48 } }, { { 0x0822d, 0x07dd9 } }, { { 0x083a4, 0x07c6e } }, { { 0x0851f, 0x07b05 } },
  { { 0x0869c, 0x0799f } }, { { 0x0881c, 0x0783b } }, { { 0x089a0, 0x076da } }, { { 0x08b26, 0x0757c } },
  { { 0x08cb0, 0x07421 } }, { { 0x08e3e, 0x072c7 } }, { { 0x08fce, 0x07171 } }, { { 0x09162, 0x0701d } },
  { { 0x092fa, 0x06ecb } }, { { 0x09495, 0x06d7c } }, { { 0x09634, 0x06c2f } }, { { 0x097d6, 0x06ae4 } },
  { { 0x0997c, 0x0699c } }, { { 0x09b26, 0x06856 } }, { { 0x09cd4, 0x06712 } }, { { 0x09e85, 0x065d1 } },
  { { 0x0a03b, 0x06491 } }, { { 0x0a1f5, 0x06354 } }, { { 0x0a3b3, 0x06219 } }, { { 0x0a575, 0x060e0 } },
  { { 0x0a73c, 0x05fa9 } }, { { 0x0a907, 0x05e74 } }, { { 0x0aad6, 0x05d41 } }, { { 0x0acaa, 0x05c10 } },
  { { 0x0ae83, 0x05ae1 } }, { { 0x0b061, 0x059b4 } }, { { 0x0b243, 0x05888 } }, { { 0x0b42a, 0x0575f } },
  { { 0x0b617, 0x05638 } }, { { 0x0b808, 0x05512 } }, { { 0x0b9ff, 0x053ee } }, { { 0x0bbfc, 0x052cc } },
  { { 0x0bdfe, 0x051ac } }, { { 0x0c005, 0x0508d } }, { { 0x0c212, 0x04f71 } }, { { 0x0c426, 0x04e55 } },
  { { 0x0c63f, 0x04d3c } }, { { 0x0c85e, 0x04c24 } }, { { 0x0ca84, 0x04b0e } }, { { 0x0ccb0, 0x049fa } },
  { { 0x0cee3, 0x048e7 } }, { { 0x0d11c, 0x047d6 } }, { { 0x0d35d, 0x046c6 } }, { { 0x0d5a5, 0x045b8 } },
  { { 0x0d7f3, 0x044ab } }, { { 0x0da4a, 0x043a0 } }, { { 0x0dca8, 0x04297 } }, { { 0x0df0e, 0x0418e } },
  { { 0x0e17c, 0x04088 } }, { { 0x0e3f2, 0x03f83 } }, { { 0x0e671, 0x03e7f } }, { { 0x0e8f9, 0x03d7d } },
  { { 0x0eb89, 0x03c7c } }, { { 0x0ee23, 0x03b7c } }, { { 0x0f0c7, 0x03a7e } }, { { 0x0f374, 0x03981 } },
  { { 0x0f62b, 0x03886 } }, { { 0x0f8ed, 0x0378c } }, { { 0x0fbb9, 0x03693 } }, { { 0x0fe90, 0x0359b } },
  { { 0x10173, 0x034a5 } }, { { 0x10461, 0x033b0 } }, { { 0x1075c, 0x032bc } }, { { 0x10a63, 0x031ca } },
  { { 0x10d77, 0x030d9 } }, { { 0x11098, 0x02fe9 } }, { { 0x113c7, 0x02efa } }, { { 0x11704, 0x02e0c } },
  { { 0x11a51, 0x02d20 } }, { { 0x11dac, 0x02c35 } }, { { 0x12118, 0x02b4b } }, { { 0x12494, 0x02a62 } },
  { { 0x12821, 0x0297a } }, { { 0x12bc0, 0x02894 } }, { { 0x12f71, 0x027ae } }, { { 0x13336, 0x026ca } },
  { { 0x1370f, 0x025e6 } }, { { 0x13afd, 0x02504 } }, { { 0x13f01, 0x02423 } }, { { 0x1431b, 0x02343 } },
  { { 0x1474e, 0x02264 } }, { { 0x14b99, 0x02186 } }, { { 0x14fff, 0x020a9 } }, { { 0x15480, 0x01fcd } },
  { { 0x1591e, 0x01ef3 } }, { { 0x15dda, 0x01e19 } }, { { 0x162b6, 0x01d40 } }, { { 0x167b4, 0x01c68 } },
  { { 0x16cd5, 0x01b91 } }, { { 0x1721c, 0x01abb } }, { { 0x1778a, 0x019e6 } }, { { 0x17d23, 0x01912 } },
  { { 0x182e8, 0x0183f } }, { { 0x188de, 0x0176d } }, { { 0x18f06, 0x0169c } }, { { 0x19564, 0x015cc } },
  { { 0x19bfc, 0x014fc } }, { { 0x1a2d4, 0x0142e } }, { { 0x1a9ee, 0x01360 } }, { { 0x1b151, 0x01294 } },
  { { 0x1b903, 0x011c8 } }, { { 0x1c10b, 0x010fd } }, { { 0x1c970, 0x01033 } }, { { 0x1d23c, 0x00f6a } },
  { { 0x1db78, 0x00ea2 } }, { { 0x1e531, 0x00dda } }, { { 0x1ef74, 0x00d13 } }, { { 0x1fa51, 0x00c4e } },
  { { 0x205dd, 0x00b89 } }, { { 0x2122e, 0x00ac4 } }, { { 0x21f60, 0x00a01 } }, { { 0x22d96, 0x0093f } },
  { { 0x23cfc, 0x0087d } }, { { 0x24dc9, 0x007bc } }, { { 0x26044, 0x006fc } }, { { 0x274ce, 0x0063c } },
  { { 0x28beb, 0x0057e } }, { { 0x2a658, 0x004c0 } }, { { 0x2c531, 0x00403 } }, { { 0x2ea40, 0x00346 } },
  { { 0x318a9, 0x0028b } }, { { 0x356cb, 0x001d0 } }, { { 0x3b520, 0x00116 } }, { { 0x48000, 0x0005c } },
};
void BinProbModel_Std::init( int qp, int initId )
{
  auto      start      = high_resolution_clock::now();

  int slope = (initId >> 3) - 4;
  int offset = ((initId & 7) * 18) + 1;
  int inistate = ((slope   * (qp - 16)) >> 1) + offset;
  //int       inistate   = calculateSum(((slope * (qp - 16)) >> 1) , offset, 8, 7);
  int state_clip = inistate < 1 ? 1 : inistate > 127 ? 127 : inistate;
  const int p1 = (state_clip << 8);
  m_state[0]   = p1 & MASK_0;
  m_state[1]   = p1 & MASK_1;

  auto stop        = high_resolution_clock::now();
  auto duration    = duration_cast<nanoseconds>(stop - start);
  timeOfBinProbModel_Std = timeOfBinProbModel_Std + duration.count();
}




CtxSet::CtxSet( std::initializer_list<CtxSet> ctxSets )
{
  uint16_t  minOffset = std::numeric_limits<uint16_t>::max();
  uint16_t  maxOffset = 0;
  for( auto iter = ctxSets.begin(); iter != ctxSets.end(); iter++ )
  {
    minOffset = std::min<uint16_t>( minOffset, (*iter).Offset              );
    maxOffset = std::max<uint16_t>( maxOffset, (*iter).Offset+(*iter).Size );
  }
  Offset  = minOffset;
  Size    = maxOffset - minOffset;
}





const std::vector<uint8_t>& ContextSetCfg::getInitTable( unsigned initId )
{
  CHECK( initId >= (unsigned)sm_InitTables.size(),
         "Invalid initId (" << initId << "), only " << sm_InitTables.size() << " tables defined." );
  return sm_InitTables[initId];
}


CtxSet ContextSetCfg::addCtxSet( std::initializer_list<std::initializer_list<uint8_t>> initSet2d )
{
  const std::size_t startIdx  = sm_InitTables[0].size();
  const std::size_t numValues = ( *initSet2d.begin() ).size();
        std::size_t setId     = 0;
  for( auto setIter = initSet2d.begin(); setIter != initSet2d.end() && setId < sm_InitTables.size(); setIter++, setId++ )
  {
    const std::initializer_list<uint8_t>& initSet   = *setIter;
    std::vector<uint8_t>&           initTable = sm_InitTables[setId];
    CHECK( initSet.size() != numValues,
           "Number of init values do not match for all sets (" << initSet.size() << " != " << numValues << ")." );
    initTable.resize( startIdx + numValues );
    std::size_t elemId = startIdx;
    for( auto elemIter = ( *setIter ).begin(); elemIter != ( *setIter ).end(); elemIter++, elemId++ )
    {
      initTable[elemId] = *elemIter;
    }
  }
  return CtxSet( (uint16_t)startIdx, (uint16_t)numValues );
}


#define CNU 35
std::vector<std::vector<uint8_t>> ContextSetCfg::sm_InitTables(NUMBER_OF_SLICE_TYPES + 1);

// clang-format off
const CtxSet ContextSetCfg::SplitFlag = ContextSetCfg::addCtxSet
({
  {  18,  27,  15,  11,  28,  30,  19,  22,  23, },
  {  18,  27,  53,  12,   6,  30,  13,  15,  31, },
  {  19,  28,  38,  12,  29,  38,  28,  38,  31, },
  {  12,  13,   8,   8,  13,  12,   5,  10,   9, },
});

const CtxSet ContextSetCfg::SplitQtFlag = ContextSetCfg::addCtxSet
({
  {  26,  36,  38,  33,  34,  21, },
  {  20,   7,  23,  18,  19,   6, },
  {  12,   6,  15,  33,  27,  22, },
  {   0,   8,   8,  12,  12,  12, },
});

const CtxSet ContextSetCfg::SplitHvFlag = ContextSetCfg::addCtxSet
({
  {  43,  42,  37,  35,  44, },
  {  36,  35,  37,  27,  52, },
  {  43,  42,  29,  27,  44, },
  {   9,   9,   9,   8,   8, },
});

const CtxSet ContextSetCfg::Split12Flag = ContextSetCfg::addCtxSet
({
  {  28,  29,  28,  29, },
  {  36,  37,  28,  22, },
  {  51,  37,  51,  37, },
  {  12,  12,  12,  13, },
});

const CtxSet ContextSetCfg::ModeConsFlag = ContextSetCfg::addCtxSet
({
  {  40,  28, },
  {  25,  12, },
  { CNU, CNU, },
  {   1,   0, },
});

const CtxSet ContextSetCfg::SkipFlag = ContextSetCfg::addCtxSet
({
  {  57,  60,  53, },
  {  57,  59,  45, },
  {   0,  34,  36, },
  {   5,   4,   8, },
});

const CtxSet ContextSetCfg::MergeFlag = ContextSetCfg::addCtxSet
({
  {   6, },
  {   6, },
  {  19, },
  {   5, },
});

const CtxSet ContextSetCfg::RegularMergeFlag = ContextSetCfg::addCtxSet
({
  {  31,  15, },
  {  38,   7, },
  { CNU, CNU, },
  {   5,   5, },
});

const CtxSet ContextSetCfg::MergeIdx = ContextSetCfg::addCtxSet
({
  {  41, },
  {  43, },
  {  34, },
  {   4, },
});

const CtxSet ContextSetCfg::MmvdFlag = ContextSetCfg::addCtxSet
({
  {  48, },
  {  26, },
  { CNU, },
  {   4, },
});

const CtxSet ContextSetCfg::MmvdMergeIdx = ContextSetCfg::addCtxSet
({
  {  43, },
  {  43, },
  { CNU, },
  {  10, },
});

const CtxSet ContextSetCfg::MmvdStepMvpIdx = ContextSetCfg::addCtxSet
({
  {  51, },
  {  60, },
  { CNU, },
  {   0, },
});

const CtxSet ContextSetCfg::PredMode = ContextSetCfg::addCtxSet
({
  {  40,  50, },
  {  40,  35, },
  { CNU, CNU, },
  {   5,   2, },
});

const CtxSet ContextSetCfg::MultiRefLineIdx = ContextSetCfg::addCtxSet
({
  {  25,  50, },
  {  25,  57, },
  {  25,  51, },
  {   6,   8, },
});

const CtxSet ContextSetCfg::IntraLumaMpmFlag = ContextSetCfg::addCtxSet
({
  {  36, },
  {  36, },
  {  45, },
  {   6, },
});

const CtxSet ContextSetCfg::IntraLumaPlanarFlag = ContextSetCfg::addCtxSet
({
  {  13,  21, },
  {  12,  13, },
  {  13,  28, },
  {   4,   5, },
});

const CtxSet ContextSetCfg::CclmModeFlag = ContextSetCfg::addCtxSet
({
  {  19, },
  {  42, },
  {  59, },
  {   4, },
});

const CtxSet ContextSetCfg::IntraChromaPredMode = ContextSetCfg::addCtxSet
({
  {  25, },
  {  33, },
  {  19, },
  {   6, },
});

const CtxSet ContextSetCfg::MipFlag = ContextSetCfg::addCtxSet
({
  {  41,  49,  50,  26, },
  {  41,  57,  58,  26, },
  {  33,  41,  42,  25, },
  {   9,  10,  10,   5, },
});


const CtxSet ContextSetCfg::DeltaQP = ContextSetCfg::addCtxSet
({
  { CNU, CNU, },
  { CNU, CNU, },
  { CNU, CNU, },
  { DWS, DWS, },
});

const CtxSet ContextSetCfg::InterDir = ContextSetCfg::addCtxSet
({
  {   6,  13,   5,   4,  25, },
  {   7,   6,   5,   4,  33, },
  { CNU, CNU, CNU, CNU, CNU, },
  {   0,   0,   1,   4,   0, },
});

const CtxSet ContextSetCfg::RefPic = ContextSetCfg::addCtxSet
({
  {  13,  20, },
  {  27,  35, },
  { CNU, CNU, },
  {   0,   4, },
});

const CtxSet ContextSetCfg::SubblockMergeFlag = ContextSetCfg::addCtxSet
({
  {  56,  59,  60, },
  {  56,  50,  37, },
  { CNU, CNU, CNU, },
  {   4,   4,   4, },
});

const CtxSet ContextSetCfg::AffineFlag = ContextSetCfg::addCtxSet
({
  {  27,  28,  29, },
  {  12,  20,   6, },
  { CNU, CNU, CNU, },
  {   4,   1,   0, },
});

const CtxSet ContextSetCfg::AffineType = ContextSetCfg::addCtxSet
({
  {  35, },
  {  35, },
  { CNU, },
  {   4, },
});

const CtxSet ContextSetCfg::AffMergeIdx = ContextSetCfg::addCtxSet
({
  {   4, },
  {   5, },
  { CNU, },
  {   0, },
});

const CtxSet ContextSetCfg::GBiIdx = ContextSetCfg::addCtxSet
({
  {  20, },
  {   5, },
  { CNU, },
  {   0, },
});

const CtxSet ContextSetCfg::Mvd = ContextSetCfg::addCtxSet
({
  {  51,  58, },
  {  44,  43, },
  {  14,  45, },
  {   9,   5, },
});

const CtxSet ContextSetCfg::BDPCMMode = ContextSetCfg::addCtxSet
({
  { CNU, CNU, },
  { CNU, CNU, },
  { CNU, CNU, },
  { DWS, DWS, },
});

const CtxSet ContextSetCfg::QtRootCbf = ContextSetCfg::addCtxSet
({
  {  12, },
  {   5, },
  {   6, },
  {   4, },
});

const CtxSet ContextSetCfg::QtCbf[] =
{
  ContextSetCfg::addCtxSet
  ({
    {  15, CNU,   5,  14, },
    {  15, CNU,  20,   7, },
    {   7, CNU,   5,   7, },
    {   5, DWS,   8,   8, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {  25, },
    {  25, },
    {  12, },
    {   5, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {   9,  44, },
    {  25,  29, },
    {  33,  21, },
    {   2,   1, },
  })
};

const CtxSet ContextSetCfg::SigCoeffGroup[] =
{
  ContextSetCfg::addCtxSet
  ({
    {  25,  37, },
    {  25,  30, },
    {  18,  31, },
    {   8,   5, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {  25,  37, },
    {  25,  52, },
    {  25,   7, },
    {   5,   8, },
  })
};

const CtxSet ContextSetCfg::SigFlag[] =
{
  ContextSetCfg::addCtxSet
  ({
    {  17,  41,  49,  51,   1,  49,  50,  37,  48,  51,  58,  45, },
    {  17,  41,  42,  29,  25,  49,  43,  37,  33,  51,  51,  30, },
    {  25,  19,  28,  14,  25,  20,  29,  30,  19,  52,  30,  38, },
    {  12,   9,   9,  10,   9,   9,   9,  10,   8,   8,   8,  10, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {   9,  49,  42,  21,  48,  59,  59,  53, },
    {  17,  19,  20,  29,  41,  59,  60,  38, },
    {  25,  27,  28,  37,  49,  53,  53,  46, },
    {   9,   9,   9,  13,   5,   5,   8,   9, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {  26,  45,  53,  46,  49,  54,  61,  39,  42,  39,  39,  39, },
    {  19,  38,  38,  46,  34,  54,  54,  39,   6,  39,  39,  39, },
    {  11,  38,  46,  54,  27,  39,  39,  39,  28,  39,  39,  39, },
    {   9,  12,   8,   8,   8,   8,   8,   5,   8,   0,   0,   0, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {  34,  45,  38,  31,  58,  39,  39,  39, },
    {  35,  45,  53,  54,  51,  39,  39,  39, },
    {  19,  46,  38,  39,  52,  39,  39,  39, },
    {   8,  12,   8,   8,   4,   0,   0,   0, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {  19,  54,  39,  39,  50,  39,  39,  39,   0,  39,  39,  39, },
    {  19,  39,  54,  39,  19,  39,  39,  39,  56,  39,  39,  39, },
    {  18,  39,  39,  39,  11,  39,  39,  39,   0,  39,  39,  39, },
    {   8,   8,   8,   8,   8,   0,   4,   4,   0,   0,   0,   0, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {  34,  38,  54,  39,  41,  39,  39,  39, },
    {  34,  38,  62,  39,  26,  39,  39,  39, },
    {  26,  39,  39,  39,  19,  39,  39,  39, },
    {   8,   8,   8,   8,   4,   0,   0,   0, },
  })
};

const CtxSet ContextSetCfg::ParFlag[] =
{
  ContextSetCfg::addCtxSet
  ({
    {  33,  40,  25,  41,  26,  42,  25,  33,  26,  34,  27,  25,  41,  42,  42,  35,  33,  27,  35,  42,  43, },
    {  18,  17,  33,  18,  34,  42,  25,  33,  26,  42,  27,  25,  34,  42,  42,  20,  26,  27,  42,  20,  20, },
    {  33,  25,  18,  26,  34,  27,  25,  26,  19,  42,  35,  33,  19,  27,  35,  20,  34,  42,  20,  43,  20, },
    {   8,   9,  12,  13,  13,  13,  10,  13,  13,  13,  13,  13,  13,  13,  13,  13,  10,  13,  13,  13,  13, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {  33,  25,  26,  19,  19,  27,  33,  42,  43,  27,  43, },
    {  25,  25,  26,  11,  19,  27,  33,  42,  50,  20,  43, },
    {  33,  25,  26,  42,  19,  27,  26,  50,  43,  20,  43, },
    {   9,  13,  12,  12,  13,  13,  13,  13,  13,  13,  13, },
  })
};

const CtxSet ContextSetCfg::GtxFlag[] =
{
  ContextSetCfg::addCtxSet
  ({
    {  25,   0,   0,  17,  25,  18,   0,   9,  25,  33,  19,   0,  25,  33,  26,  20,  25,  33,  34,  42,  29, },
    {  17,   0,   1,  17,  25,  18,   0,   9,  25,  33,  34,   9,  25,  18,  26,  20,  25,  18,  19,  27,  21, },
    {  25,   1,  40,  25,  33,  11,  17,  25,  25,  18,   4,  17,  33,  11,   4,   5,  33,  19,  20,  28,  22, },
    {   1,   5,   9,   9,   9,   6,   5,   9,  10,  10,   9,   9,   9,   9,   9,   9,   6,   8,   9,   8,   9, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {  25,   1,  40,  33,  18,   4,  25,  33,  27,  36,  37, },
    {  17,   9,  25,  10,   3,   4,  17,  33,  19,  28,  29, },
    {  48,   9,  25,  18,  26,  27,  25,  26,  35,  28,  37, },
    {   1,   5,   8,   8,   8,   6,   6,   9,   8,   8,   9, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {   0,   0,  33,  34,  35,  36,  25,  49,  35,  28,  29,  40,  42,  43,  36,  37,  56,  58,  59,  45,  38, },
    {   0,  17,  26,  19,  20,  21,  25,  34,  20,  28,  29,  33,  27,  28,  29,  37,  34,  28,  44,  37,  38, },
    {  25,  25,  11,  27,  20,  21,  18,  12,  28,  21,  22,  34,  28,  29,  29,  30,  28,  29,  45,  30,  23, },
    {   9,   5,  10,  13,  13,  10,   9,  10,  13,  13,  13,   9,  10,  10,  10,  10,   8,   9,   8,  10,  13, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {   0,  40,  42,  20,  21,  29,  49,  52,  53,  38,  46, },
    {   0,  25,  27,  20,  13,   6,  57,  52,  30,  38,  31, },
    {  40,  33,  27,  28,  21,  37,  51,  37,  53,  38,  46, },
    {   9,   9,  10,  12,  12,  10,   5,   9,   9,   9,   9, },
  })
};

const CtxSet ContextSetCfg::LastX[] =
{
  ContextSetCfg::addCtxSet
  ({
    {  14,   6,   5,   7,   7,  12,   7,   7,   6,  12,  22,   7,   6,  14,  20,  28,   7,  13,  13,  20, },
    {   6,  13,  12,   6,   6,   4,  14,  14,   5,  12,  29,  14,  13,   5,  36,  28,  14,  13,  20,  19, },
    {   6,  13,  12,   6,  14,  12,  14,  14,  29,   4,  14,   7,  14,  29,   4,  29,  30,  37,  29,  58, },
    {   8,   5,   4,   5,   4,   4,   5,   4,   1,   0,   5,   1,   0,   0,   0,   1,   1,   0,   0,   0, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {  11,   5,   3, },
    {  19,   4,  18, },
    {  12,  11,   3, },
    {   2,   1,   1, },
  })
};

const CtxSet ContextSetCfg::LastY[] =
{
  ContextSetCfg::addCtxSet
  ({
    {  13,  13,  20,   6,   6,  12,  14,  14,   5,  13,  14,   7,   5,  12,  21,  13,   7,  13,  12,  41, },
    {   5,   5,  12,   6,   6,  19,   6,  14,   5,  19,  29,   7,  13,   5,  36,  21,   7,  13,   5,  27, },
    {  13,   5,   4,   6,   6,  11,  14,  14,   5,  11,  14,   7,  14,   5,   3,  21,  45,  45,  21,  34, },
    {   8,   5,   8,   5,   5,   4,   5,   5,   4,   0,   5,   5,   1,   0,   0,   1,   4,   0,   0,   0, },
  }),
  ContextSetCfg::addCtxSet
  ({
    {  11,   5,  19, },
    {  11,   4,  18, },
    {  12,   4,   3, },
    {   6,   2,   2, },
  })
};

const CtxSet ContextSetCfg::MVPIdx = ContextSetCfg::addCtxSet
({
  {  34, },
  {  49, },
  {  42, },
  {  12, },
});

const CtxSet ContextSetCfg::SmvdFlag = ContextSetCfg::addCtxSet
({
  {  50, },
  {  28, },
  { CNU, },
  {   5, },
});

const CtxSet ContextSetCfg::SaoMergeFlag = ContextSetCfg::addCtxSet
({
  {   2, },
  {  60, },
  {  59, },
  {   0, },
});

const CtxSet ContextSetCfg::SaoTypeIdx = ContextSetCfg::addCtxSet
({
  {  10, },
  {   5, },
  {   5, },
  {   0, },
});

const CtxSet ContextSetCfg::TransquantBypassFlag = ContextSetCfg::addCtxSet
({
  { CNU, },
  { CNU, },
  { CNU, },
  { DWS, },
});

const CtxSet ContextSetCfg::LFNSTIdx = ContextSetCfg::addCtxSet
({
  {  45,  37, },
  {  38,  45, },
  { CNU,  45, },
  {   8,   8, },
});

const CtxSet ContextSetCfg::PLTFlag = ContextSetCfg::addCtxSet
({
  { CNU, },
  { CNU, },
  { CNU, },
  { DWS, },
});

const CtxSet ContextSetCfg::RotationFlag = ContextSetCfg::addCtxSet
({
  { CNU, },
  { CNU, },
  { CNU, },
  { DWS, },
});

const CtxSet ContextSetCfg::RunTypeFlag = ContextSetCfg::addCtxSet
({
  { CNU, },
  { CNU, },
  { CNU, },
  { DWS, },
});

const CtxSet ContextSetCfg::IdxRunModel = ContextSetCfg::addCtxSet
({
  { CNU, CNU, CNU, CNU, CNU, },
  { CNU, CNU, CNU, CNU, CNU, },
  { CNU, CNU, CNU, CNU, CNU, },
  { DWS, DWS, DWS, DWS, DWS, },
});

const CtxSet ContextSetCfg::CopyRunModel = ContextSetCfg::addCtxSet
({
  { CNU, CNU, CNU, },
  { CNU, CNU, CNU, },
  { CNU, CNU, CNU, },
  { DWS, DWS, DWS, },
});

const CtxSet ContextSetCfg::RdpcmFlag = ContextSetCfg::addCtxSet
({
  { CNU, CNU, },
  { CNU, CNU, },
  { CNU, CNU, },
  { DWS, DWS, },
});

const CtxSet ContextSetCfg::RdpcmDir = ContextSetCfg::addCtxSet
({
  { CNU, CNU, },
  { CNU, CNU, },
  { CNU, CNU, },
  { DWS, DWS, },
});

const CtxSet ContextSetCfg::MTSIndex = ContextSetCfg::addCtxSet
({
  {  29, CNU, CNU, CNU, CNU, CNU,  33,  18,  27,   0, CNU, },
  {  29, CNU, CNU, CNU, CNU, CNU,  18,  33,  27,   0, CNU, },
  {  20, CNU, CNU, CNU, CNU, CNU,  33,   0,  42,   0, CNU, },
  {   8, DWS, DWS, DWS, DWS, DWS,   1,   0,   9,   0, DWS, },
});

const CtxSet ContextSetCfg::ISPMode = ContextSetCfg::addCtxSet
({
  {  48,  43, },
  {  33,  43, },
  {  33,  43, },
  {   9,   2, },
});

const CtxSet ContextSetCfg::SbtFlag = ContextSetCfg::addCtxSet
({
  {  57,  58, },
  {  57,  58, },
  { CNU, CNU, },
  {   1,   5, },
});

const CtxSet ContextSetCfg::SbtQuadFlag = ContextSetCfg::addCtxSet
({
  {  42, },
  {  42, },
  { CNU, },
  {  10, },
});

const CtxSet ContextSetCfg::SbtHorFlag = ContextSetCfg::addCtxSet
({
  {  35,  51,  20, },
  {  20,  43,  12, },
  { CNU, CNU, CNU, },
  {   8,   4,   4, },
});

const CtxSet ContextSetCfg::SbtPosFlag = ContextSetCfg::addCtxSet
({
  {  28, },
  {  28, },
  { CNU, },
  {  13, },
});

const CtxSet ContextSetCfg::CrossCompPred = ContextSetCfg::addCtxSet
({
  { CNU, CNU, CNU, CNU, CNU, CNU, CNU, CNU, CNU, CNU, },
  { CNU, CNU, CNU, CNU, CNU, CNU, CNU, CNU, CNU, CNU, },
  { CNU, CNU, CNU, CNU, CNU, CNU, CNU, CNU, CNU, CNU, },
  { DWS, DWS, DWS, DWS, DWS, DWS, DWS, DWS, DWS, DWS, },
});

const CtxSet ContextSetCfg::ChromaQpAdjFlag = ContextSetCfg::addCtxSet
({
  { CNU, },
  { CNU, },
  { CNU, },
  { DWS, },
});

const CtxSet ContextSetCfg::ChromaQpAdjIdc = ContextSetCfg::addCtxSet
({
  { CNU, },
  { CNU, },
  { CNU, },
  { DWS, },
});

const CtxSet ContextSetCfg::ImvFlag = ContextSetCfg::addCtxSet
({
  {  58,  33,  50,  59,  52, },
  {  59,  48,  58,  60,  60, },
  { CNU,  34, CNU, CNU, CNU, },
  {   0,   5,   1,   0,   1, },
});

const CtxSet ContextSetCfg::ctbAlfFlag = ContextSetCfg::addCtxSet
({
  {  26,  45,  46,  33,  61,  54,  33,  61,  54, },
  {   6,  23,  46,  27,  61,  54,  20,  46,  54, },
  {  39,  39,  39,  54,  39,  39,  31,  62,  39, },
  {   0,   0,   0,   0,   0,   0,   0,   0,   0, },
});

const CtxSet ContextSetCfg::ctbAlfAlternative = ContextSetCfg::addCtxSet
({
  {  18,  18, },
  {  20,  12, },
  {  44,  44, },
  {   0,   0, },
});

const CtxSet ContextSetCfg::AlfUseLatestFilt = ContextSetCfg::addCtxSet
({
  {  58, },
  {  50, },
  {  31, },
  {   0, },
});

const CtxSet ContextSetCfg::AlfUseTemporalFilt = ContextSetCfg::addCtxSet
({
  {  53, },
  {  53, },
  { CNU, },
  {   0, },
});

const CtxSet ContextSetCfg::MHIntraFlag = ContextSetCfg::addCtxSet
({
  {  58, },
  {  58, },
  { CNU, },
  {   1, },
});

const CtxSet ContextSetCfg::IBCFlag = ContextSetCfg::addCtxSet
({
  {   0,  43,  30, },
  {   0,  42,  37, },
  {  17,  27,  36, },
  {   1,   5,   8, },
});

const CtxSet ContextSetCfg::JointCbCrFlag = ContextSetCfg::addCtxSet
({
  {  51,  44,  45, },
  {  36,  44,  45, },
  {  43,  29,  51, },
  {   1,   1,   0, },
});

const CtxSet ContextSetCfg::TsSigCoeffGroup = ContextSetCfg::addCtxSet
({
  {  18,  35,  37, },
  {  18,  12,  29, },
  {  11,   5,  38, },
  {   5,   5,   5, },
});

const CtxSet ContextSetCfg::TsSigFlag = ContextSetCfg::addCtxSet
({
  {  25,  50,  37, },
  {  40,  35,  44, },
  {  25,  28,  38, },
  {  13,  13,   8, },
});

const CtxSet ContextSetCfg::TsParFlag = ContextSetCfg::addCtxSet
({
  {  11, },
  {   3, },
  {  11, },
  {   6, },
});

const CtxSet ContextSetCfg::TsGtxFlag = ContextSetCfg::addCtxSet
({
  { CNU,  10,   4,   4,   5, },
  { CNU,   2,   3,   3,   4, },
  { CNU,   3,   3,   3,   3, },
  { DWS,   1,   1,   1,   1, },
});

const CtxSet ContextSetCfg::TsLrg1Flag = ContextSetCfg::addCtxSet
({
  {  19,  11,  12, CNU, },
  {  18,  11,  12, CNU, },
  {  11,   5,  13, CNU, },
  {   4,   2,   1, DWS, },
});

const CtxSet ContextSetCfg::TsResidualSign = ContextSetCfg::addCtxSet
({
  {  28,  25,  53, CNU, CNU, CNU, },
  {   5,  10,  53, CNU, CNU, CNU, },
  {  20,   2,  46, CNU, CNU, CNU, },
  {   1,   4,   4, DWS, DWS, DWS, },
});
// clang-format on

const unsigned ContextSetCfg::NumberOfContexts = (unsigned)ContextSetCfg::sm_InitTables[0].size();


// combined sets
const CtxSet ContextSetCfg::Palette = { ContextSetCfg::RotationFlag, ContextSetCfg::RunTypeFlag, ContextSetCfg::IdxRunModel, ContextSetCfg::CopyRunModel };
const CtxSet ContextSetCfg::Sao = { ContextSetCfg::SaoMergeFlag, ContextSetCfg::SaoTypeIdx };

const CtxSet ContextSetCfg::Alf = { ContextSetCfg::ctbAlfFlag, ContextSetCfg::ctbAlfAlternative, ContextSetCfg::AlfUseLatestFilt, ContextSetCfg::AlfUseTemporalFilt };

template <class BinProbModel>
CtxStore<BinProbModel>::CtxStore()
  : m_CtxBuffer ()
  , m_Ctx       ( nullptr )
{}

template <class BinProbModel>
CtxStore<BinProbModel>::CtxStore( bool dummy )
  : m_CtxBuffer ( ContextSetCfg::NumberOfContexts )
  , m_Ctx       ( m_CtxBuffer.data() )
{}

template <class BinProbModel>
CtxStore<BinProbModel>::CtxStore( const CtxStore<BinProbModel>& ctxStore )
  : m_CtxBuffer ( ctxStore.m_CtxBuffer )
  , m_Ctx       ( m_CtxBuffer.data() )
{}

template <class BinProbModel>
void CtxStore<BinProbModel>::init( int qp, int initId )
{
  const std::vector<uint8_t>& initTable = ContextSetCfg::getInitTable( initId );
  CHECK( m_CtxBuffer.size() != initTable.size(),
        "Size of init table (" << initTable.size() << ") does not match size of context buffer (" << m_CtxBuffer.size() << ")." );
  const std::vector<uint8_t> &rateInitTable = ContextSetCfg::getInitTable(NUMBER_OF_SLICE_TYPES);
  CHECK(m_CtxBuffer.size() != rateInitTable.size(),
        "Size of rate init table (" << rateInitTable.size() << ") does not match size of context buffer ("
                                    << m_CtxBuffer.size() << ").");
  int clippedQP = Clip3( 0, MAX_QP, qp );
  for( std::size_t k = 0; k < m_CtxBuffer.size(); k++ )
  {
    m_CtxBuffer[k].init( clippedQP, initTable[k] );
    m_CtxBuffer[k].setLog2WindowSize(rateInitTable[k]);
  }
}

template <class BinProbModel>
void CtxStore<BinProbModel>::setWinSizes( const std::vector<uint8_t>& log2WindowSizes )
{
  CHECK( m_CtxBuffer.size() != log2WindowSizes.size(),
        "Size of window size table (" << log2WindowSizes.size() << ") does not match size of context buffer (" << m_CtxBuffer.size() << ")." );
  for( std::size_t k = 0; k < m_CtxBuffer.size(); k++ )
  {
    m_CtxBuffer[k].setLog2WindowSize( log2WindowSizes[k] );
  }
}

template <class BinProbModel>
void CtxStore<BinProbModel>::loadPStates( const std::vector<uint16_t>& probStates )
{
  CHECK( m_CtxBuffer.size() != probStates.size(),
        "Size of prob states table (" << probStates.size() << ") does not match size of context buffer (" << m_CtxBuffer.size() << ")." );
  for( std::size_t k = 0; k < m_CtxBuffer.size(); k++ )
  {
    m_CtxBuffer[k].setState( probStates[k] );
  }
}

template <class BinProbModel>
void CtxStore<BinProbModel>::savePStates( std::vector<uint16_t>& probStates ) const
{
  probStates.resize( m_CtxBuffer.size(), uint16_t(0) );
  for( std::size_t k = 0; k < m_CtxBuffer.size(); k++ )
  {
    probStates[k] = m_CtxBuffer[k].getState();
  }
}





template class CtxStore<BinProbModel_Std>;





Ctx::Ctx()                                  : m_BPMType( BPM_Undefined )                        {}
Ctx::Ctx( const BinProbModel_Std*   dummy ) : m_BPMType( BPM_Std   ), m_CtxStore_Std  ( true )  {}

Ctx::Ctx( const Ctx& ctx )
  : m_BPMType         ( ctx.m_BPMType )
  , m_CtxStore_Std    ( ctx.m_CtxStore_Std    )
{
  ::memcpy( m_GRAdaptStats, ctx.m_GRAdaptStats, sizeof( unsigned ) * RExt__GOLOMB_RICE_ADAPTATION_STATISTICS_SETS );
}

