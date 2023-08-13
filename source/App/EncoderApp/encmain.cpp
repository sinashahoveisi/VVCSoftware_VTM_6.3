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

/** \file     encmain.cpp
    \brief    Encoder application main
*/

#include <time.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>

#include "EncApp.h"
#include "Utilities/program_options_lite.h"
#include "../../source/Lib/EncoderLib/InterSearch.h"
#include "../../source/Lib/EncoderLib/CABACWriter.h"
#include "../../source/Lib/EncoderLib/EncModeCtrl.h"
#include "../../source/Lib/EncoderLib/EncGOP.h"
#include "../../source/App/EncoderApp/EncApp.h"
#include "../../source/Lib/CommonLib/InterPrediction.h"
#include "../../source/Lib/CommonLib/IntraPrediction.h"
#include "../../source/Lib/CommonLib/WeightPrediction.h"
#include "../../source/Lib/CommonLib/CodingStructure.h"
#include "../../source/Lib/CommonLib/Rom.h"
#include "../../source/Lib/CommonLib/Contexts.h"
#include "../../source/Lib/CommonLib/Buffer.h"
#include "../../source/Lib/CommonLib/UnitTools.h"
#include "../../source/Lib/CommonLib/Slice.h"
#include "../../source/Lib/CommonLib/DepQuant.h"
#include "../../source/Lib/CommonLib/ApproximatAdderSubtraction.h"
#include "../../source/Lib/CommonLib/SampleAdaptiveOffset.h"
#include "../../source/Lib/CommonLib/Mv.h"
#include "../../source/Lib/CommonLib/ContextModelling.h"
#include "../../source/Lib/CommonLib/LoopFilter.h"
#include "../../source/Lib/CommonLib/Unit.h"
#include "../../source/Lib/EncoderLib/EncCu.h"
#include "../../source/Lib/DecoderLib/DecCu.h"
#include "../../source/Lib/DecoderLib/CABACReader.h"

//! \ingroup EncoderApp
//! \{

static const uint32_t settingNameWidth = 66;
static const uint32_t settingHelpWidth = 84;
static const uint32_t settingValueWidth = 3;
// --------------------------------------------------------------------------------------------------------------------- //

//macro value printing function

#define PRINT_CONSTANT(NAME, NAME_WIDTH, VALUE_WIDTH) std::cout << std::setw(NAME_WIDTH) << #NAME << " = " << std::setw(VALUE_WIDTH) << NAME << std::endl;

static void printMacroSettings()
{
  if( g_verbosity >= DETAILS )
  {
    std::cout << "Non-environment-variable-controlled macros set as follows: \n" << std::endl;

    //------------------------------------------------

    //setting macros

    PRINT_CONSTANT( RExt__DECODER_DEBUG_BIT_STATISTICS,                         settingNameWidth, settingValueWidth );
    PRINT_CONSTANT( RExt__HIGH_BIT_DEPTH_SUPPORT,                               settingNameWidth, settingValueWidth );
    PRINT_CONSTANT( RExt__HIGH_PRECISION_FORWARD_TRANSFORM,                     settingNameWidth, settingValueWidth );
    PRINT_CONSTANT( ME_ENABLE_ROUNDING_OF_MVS,                                  settingNameWidth, settingValueWidth );

    //------------------------------------------------

    std::cout << std::endl;
  }
}

// ====================================================================================================================
// Main function
// ====================================================================================================================

int main(int argc, char* argv[])
{
  // print information
  fprintf( stdout, "\n" );
  fprintf( stdout, "VVCSoftware: VTM Encoder Version %s ", VTM_VERSION );
  fprintf( stdout, NVM_ONOS );
  fprintf( stdout, NVM_COMPILEDBY );
  fprintf( stdout, NVM_BITS );
#if ENABLE_SIMD_OPT
  std::string SIMD;
  df::program_options_lite::Options opts;
  opts.addOptions()
    ( "SIMD", SIMD, string( "" ), "" )
    ( "c", df::program_options_lite::parseConfigFile, "" );
  df::program_options_lite::SilentReporter err;
  df::program_options_lite::scanArgv( opts, argc, ( const char** ) argv, err );
  fprintf( stdout, "[SIMD=%s] ", read_x86_extension( SIMD ) );
#endif
#if ENABLE_TRACING
  fprintf( stdout, "[ENABLE_TRACING] " );
#endif
#if ENABLE_SPLIT_PARALLELISM
  fprintf( stdout, "[SPLIT_PARALLEL (%d jobs)]", PARL_SPLIT_MAX_NUM_JOBS );
#endif
#if ENABLE_WPP_PARALLELISM
  fprintf( stdout, "[WPP_PARALLEL]" );
#endif
#if ENABLE_WPP_PARALLELISM || ENABLE_SPLIT_PARALLELISM
  const char* waitPolicy = getenv( "OMP_WAIT_POLICY" );
  const char* maxThLim   = getenv( "OMP_THREAD_LIMIT" );
  fprintf( stdout, waitPolicy ? "[OMP: WAIT_POLICY=%s," : "[OMP: WAIT_POLICY=,", waitPolicy );
  fprintf( stdout, maxThLim   ? "THREAD_LIMIT=%s" : "THREAD_LIMIT=", maxThLim );
  fprintf( stdout, "]" );
#endif
  fprintf( stdout, "\n" );

  EncApp* pcEncApp = new EncApp;
  // create application encoder class
  pcEncApp->create();

  // parse configuration
  try
  {
    if(!pcEncApp->parseCfg( argc, argv ))
    {
      pcEncApp->destroy();
      return 1;
    }
  }
  catch (df::program_options_lite::ParseFailure &e)
  {
    std::cerr << "Error parsing option \""<< e.arg <<"\" with argument \""<< e.val <<"\"." << std::endl;
    return 1;
  }

#if PRINT_MACRO_VALUES
  printMacroSettings();
#endif

  // starting time
  auto startTime  = std::chrono::steady_clock::now();
  std::time_t startTime2 = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  printf("developed by sinasho \n");
  fprintf(stdout, " started @ %s", std::ctime(&startTime2) );
  clock_t startClock = clock();

  // call encoding function
#ifndef _DEBUG
  try
  {
#endif
    pcEncApp->encode();
#ifndef _DEBUG
  }
  catch( Exception &e )
  {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  catch (const std::bad_alloc &e)
  {
    std::cout << "Memory allocation failed: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
#endif
  // ending time
  clock_t endClock = clock();
  auto endTime = std::chrono::steady_clock::now();
  std::time_t endTime2 = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#if JVET_O0756_CALCULATE_HDRMETRICS
  auto metricTime     = pcEncApp->getMetricTime();
  auto totalTime      = std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count();
  auto encTime        = std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime - metricTime ).count();
  auto metricTimeuser = std::chrono::duration_cast<std::chrono::milliseconds>( metricTime ).count();
#else
  auto encTime = std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime).count();
#endif
  // destroy application encoder class
  pcEncApp->destroy();

  delete pcEncApp;

  printf( "\n finished @ %s", std::ctime(&endTime2) );

  double totalProcessTime = (endClock - startClock) * 1.0 / CLOCKS_PER_SEC;

  ofstream MyExcelFile;
  MyExcelFile.open("Result.csv", ios::app);
  MyExcelFile << "function,time(nanoSecond),percentage" << endl;


  // ---------------------- AFFINE ----------------------------------//
  cout << "\ntimeOfSimdEqualCoeffComputer: " << timeOfSimdEqualCoeffComputer << " nanoSecond and persent is:" << timeOfSimdEqualCoeffComputer / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSimdEqualCoeffComputer," << timeOfSimdEqualCoeffComputer << "," << timeOfSimdEqualCoeffComputer / (10000000 * totalProcessTime) << endl;
  
  cout << "timeOfXAffineMotionEstimation: " << timeOfXAffineMotionEstimation << " nanoSecond and persent is:" << timeOfXAffineMotionEstimation / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXAffineMotionEstimation," << timeOfXAffineMotionEstimation << ","  << timeOfXAffineMotionEstimation / (10000000 * totalProcessTime) << endl;
  
  cout << "timeOfSolveEqual: " << timeOfSolveEqual << " nanoSecond and persent is:" << timeOfSolveEqual / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSolveEqual," << timeOfSolveEqual << ","  << timeOfSolveEqual / (10000000 * totalProcessTime) << endl;
  

  cout << "timeOfXInitLibCfg: " << timeOfXInitLibCfg << " nanoSecond and persent is:" << timeOfXInitLibCfg / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXInitLibCfg," << timeOfXInitLibCfg << ","  << timeOfXInitLibCfg / (10000000 * totalProcessTime) << endl;
  

  cout << "timeOfXInitScanArrays: " << timeOfXInitScanArrays << " nanoSecond and persent is:" << timeOfXInitScanArrays / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXInitScanArrays," << timeOfXInitScanArrays << ","  << timeOfXInitScanArrays / (10000000 * totalProcessTime) << endl;
  

  cout << "timeOfDeriveLoopFilterBoundaryAvailibility: " << timeOfDeriveLoopFilterBoundaryAvailibility << " nanoSecond and persent is:" << timeOfDeriveLoopFilterBoundaryAvailibility / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfDeriveLoopFilterBoundaryAvailibility," << timeOfDeriveLoopFilterBoundaryAvailibility << ","  << timeOfDeriveLoopFilterBoundaryAvailibility / (10000000 * totalProcessTime) << endl;
  

  cout << "timeOfXPredInterBlk: " << timeOfXPredInterBlk << " nanoSecond and persent is:" << timeOfXPredInterBlk / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXPredInterBlk," << timeOfXPredInterBlk << ","  << timeOfXPredInterBlk / (10000000 * totalProcessTime) << endl;
  
  cout << "timeIsSubblockVectorSpreadOverLimit: " << timeIsSubblockVectorSpreadOverLimit << " nanoSecond and persent is:" << timeIsSubblockVectorSpreadOverLimit / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeIsSubblockVectorSpreadOverLimit," << timeIsSubblockVectorSpreadOverLimit << ","  << timeIsSubblockVectorSpreadOverLimit / (10000000 * totalProcessTime) << endl;
  

  cout << "timeOfXGetCodedLevelTSPred: " << timeOfXGetCodedLevelTSPred << " nanoSecond and persent is:" << timeOfXGetCodedLevelTSPred / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXGetCodedLevelTSPred," << timeOfXGetCodedLevelTSPred << ","  << timeOfXGetCodedLevelTSPred / (10000000 * totalProcessTime) << endl;
  

  cout << "timeOfRoundAffineMv: " << timeOfRoundAffineMv << " nanoSecond and persent is:" << timeOfRoundAffineMv / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfRoundAffineMv," << timeOfRoundAffineMv << "," << timeOfRoundAffineMv / (10000000 * totalProcessTime) << endl;
  
  cout << "timeOfSetAllAffineMv: " << timeOfSetAllAffineMv << " nanoSecond and persent is:" << timeOfSetAllAffineMv / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSetAllAffineMv," << timeOfSetAllAffineMv << "," << timeOfSetAllAffineMv / (10000000 * totalProcessTime) << endl;

  cout << "timeOfxCalcAffineMVBits: " << timeOfxCalcAffineMVBits << " nanoSecond and persent is:" << timeOfxCalcAffineMVBits / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfxCalcAffineMVBits," << timeOfxCalcAffineMVBits << "," << timeOfxCalcAffineMVBits / (10000000 * totalProcessTime) << endl;

  cout << "timeOfContains: " << timeOfContains << " nanoSecond and persent is:" << timeOfContains / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfContains," << timeOfContains << "," << timeOfContains / (10000000 * totalProcessTime) << endl;


  cout << "timeOfXCopyAffineAMVPInfo: " << timeOfXCopyAffineAMVPInfo << " nanoSecond and persent is:" << timeOfXCopyAffineAMVPInfo / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXCopyAffineAMVPInfo," << timeOfXCopyAffineAMVPInfo << "," << timeOfXCopyAffineAMVPInfo / (10000000 * totalProcessTime) << endl;

  cout << "timeOfCopyAffineMvFrom: " << timeOfCopyAffineMvFrom << " nanoSecond and persent is:" << timeOfCopyAffineMvFrom / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfCopyAffineMvFrom," << timeOfCopyAffineMvFrom << "," << timeOfCopyAffineMvFrom / (10000000 * totalProcessTime) << endl;

  cout << "timeOfChangeAffinePrecInternal2Amvr: " << timeOfChangeAffinePrecInternal2Amvr << " nanoSecond and persent is:" << timeOfChangeAffinePrecInternal2Amvr / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfChangeAffinePrecInternal2Amvr," << timeOfChangeAffinePrecInternal2Amvr << "," << timeOfChangeAffinePrecInternal2Amvr / (10000000 * totalProcessTime) << endl;

  cout << "timeOfSimdVerticalSobelFilter: " << timeOfSimdVerticalSobelFilter << " nanoSecond and persent is:" << timeOfSimdVerticalSobelFilter / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSimdVerticalSobelFilter," << timeOfSimdVerticalSobelFilter << "," << timeOfSimdVerticalSobelFilter / (10000000 * totalProcessTime) << endl;

   cout << "timeOfSimdHorizontalSobelFilter: " << timeOfSimdHorizontalSobelFilter << " nanoSecond and persent is:" << timeOfSimdHorizontalSobelFilter / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSimdHorizontalSobelFilter," << timeOfSimdHorizontalSobelFilter << "," << timeOfSimdHorizontalSobelFilter / (10000000 * totalProcessTime) << endl;


  cout << "timeOfResetSavedAffineMotion: " << timeOfResetSavedAffineMotion << " nanoSecond and persent is:" << timeOfResetSavedAffineMotion / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfResetSavedAffineMotion," << timeOfResetSavedAffineMotion << "," << timeOfResetSavedAffineMotion / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXPredAffineInterSearch: " << timeOfXPredAffineInterSearch << " nanoSecond and persent is:" << timeOfXPredAffineInterSearch / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXPredAffineInterSearch," << timeOfXPredAffineInterSearch << "," << timeOfXPredAffineInterSearch / (10000000 * totalProcessTime) << endl;

  cout << "timeOfCreateExplicitReferencePictureSetFromReference: " << timeOfCreateExplicitReferencePictureSetFromReference << " nanoSecond and persent is:" << timeOfCreateExplicitReferencePictureSetFromReference / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfCreateExplicitReferencePictureSetFromReference," << timeOfCreateExplicitReferencePictureSetFromReference << "," << timeOfCreateExplicitReferencePictureSetFromReference / (10000000 * totalProcessTime) << endl;


  cout << "timeOfWeightPredAnalysis: " << timeOfWeightPredAnalysis << " nanoSecond and persent is:" << timeOfWeightPredAnalysis / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfWeightPredAnalysis," << timeOfWeightPredAnalysis << "," << timeOfWeightPredAnalysis / (10000000 * totalProcessTime) << endl;


  cout << "timeOfGetAffineControlPointCand: " << timeOfGetAffineControlPointCand << " nanoSecond and persent is:" << timeOfGetAffineControlPointCand / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfGetAffineControlPointCand," << timeOfGetAffineControlPointCand << "," << timeOfGetAffineControlPointCand / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXPredAffineBlk: " << timeOfXPredAffineBlk  << " nanoSecond and persent is:" << timeOfXPredAffineBlk / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXPredAffineBlk," << timeOfXPredAffineBlk << "," << timeOfXPredAffineBlk / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXCheckRDCostAffineMerge2Nx2N: " << timeOfXCheckRDCostAffineMerge2Nx2N   << " nanoSecond and persent is:" << timeOfXCheckRDCostAffineMerge2Nx2N / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXCheckRDCostAffineMerge2Nx2N," << timeOfXCheckRDCostAffineMerge2Nx2N << "," << timeOfXCheckRDCostAffineMerge2Nx2N / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXCheckRDCostInterIMV: " << timeOfXCheckRDCostInterIMV   << " nanoSecond and persent is:" << timeOfXCheckRDCostInterIMV / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXCheckRDCostInterIMV," << timeOfXCheckRDCostInterIMV << "," << timeOfXCheckRDCostInterIMV / (10000000 * totalProcessTime) << endl;


  cout << "timeOfRoundAffinePrecInternal2Amvr: " << timeOfRoundAffinePrecInternal2Amvr  << " nanoSecond and persent is:" << timeOfRoundAffinePrecInternal2Amvr / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfRoundAffinePrecInternal2Amvr," << timeOfRoundAffinePrecInternal2Amvr << "," << timeOfRoundAffinePrecInternal2Amvr / (10000000 * totalProcessTime) << endl;

  cout << "timeOfAddAffineMVPCandUnscaled: " << timeOfAddAffineMVPCandUnscaled  << " nanoSecond and persent is:" << timeOfAddAffineMVPCandUnscaled / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfAddAffineMVPCandUnscaled," << timeOfAddAffineMVPCandUnscaled << "," << timeOfAddAffineMVPCandUnscaled / (10000000 * totalProcessTime) << endl;


  cout << "timeOfXGetAffineTemplateCost: " << timeOfXGetAffineTemplateCost  << " nanoSecond and persent is:" << timeOfXGetAffineTemplateCost / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXGetAffineTemplateCost," << timeOfXGetAffineTemplateCost << "," << timeOfXGetAffineTemplateCost / (10000000 * totalProcessTime) << endl;

  cout << "timeOfFillAffineMvpCand: " << timeOfFillAffineMvpCand  << " nanoSecond and persent is:" << timeOfFillAffineMvpCand / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfFillAffineMvpCand," << timeOfFillAffineMvpCand << "," << timeOfFillAffineMvpCand / (10000000 * totalProcessTime) << endl;

  cout << "timeOfGetAffineMergeCand: " << timeOfGetAffineMergeCand  << " nanoSecond and persent is:" << timeOfGetAffineMergeCand / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfGetAffineMergeCand," << timeOfGetAffineMergeCand << "," << timeOfGetAffineMergeCand / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXCheckBestAffineMVP: " << timeOfXCheckBestAffineMVP  << " nanoSecond and persent is:" << timeOfXCheckBestAffineMVP / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXCheckBestAffineMVP," << timeOfXCheckBestAffineMVP << "," << timeOfXCheckBestAffineMVP / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXEstimateAffineAMVP: " << timeOfXEstimateAffineAMVP  << " nanoSecond and persent is:" << timeOfXEstimateAffineAMVP / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXEstimateAffineAMVP," << timeOfXEstimateAffineAMVP << "," << timeOfXEstimateAffineAMVP / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXInheritedAffineMv: " << timeOfXInheritedAffineMv  << " nanoSecond and persent is:" << timeOfXInheritedAffineMv / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXInheritedAffineMv," << timeOfXInheritedAffineMv << "," << timeOfXInheritedAffineMv / (10000000 * totalProcessTime) << endl;

  cout << "timeOfGetAvailableAffineNeighboursForLeftPredictor: " << timeOfGetAvailableAffineNeighboursForLeftPredictor  << " nanoSecond and persent is:" << timeOfGetAvailableAffineNeighboursForLeftPredictor / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfGetAvailableAffineNeighboursForLeftPredictor," << timeOfGetAvailableAffineNeighboursForLeftPredictor << "," << timeOfGetAvailableAffineNeighboursForLeftPredictor / (10000000 * totalProcessTime) << endl;

  cout << "timeOfAddAffMVInfo: " << timeOfAddAffMVInfo  << " nanoSecond and persent is:" << timeOfAddAffMVInfo / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfAddAffMVInfo," << timeOfAddAffMVInfo << "," << timeOfAddAffMVInfo / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXDetermineBestMvp: " << timeOfXDetermineBestMvp  << " nanoSecond and persent is:" << timeOfXDetermineBestMvp / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXDetermineBestMvp," << timeOfXDetermineBestMvp << "," << timeOfXDetermineBestMvp / (10000000 * totalProcessTime) << endl;

  cout << "timeOfGetAvailableAffineNeighboursForAbovePredictor: " << timeOfGetAvailableAffineNeighboursForAbovePredictor  << " nanoSecond and persent is:" << timeOfGetAvailableAffineNeighboursForAbovePredictor / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfGetAvailableAffineNeighboursForAbovePredictor," << timeOfGetAvailableAffineNeighboursForAbovePredictor << "," << timeOfGetAvailableAffineNeighboursForAbovePredictor / (10000000 * totalProcessTime) << endl;

  cout << "timeOfAffine_amvr_mode: " << timeOfAffine_amvr_mode  << " nanoSecond and persent is:" << timeOfAffine_amvr_mode / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfAffine_amvr_mode," << timeOfAffine_amvr_mode << "," << timeOfAffine_amvr_mode / (10000000 * totalProcessTime) << endl;

  cout << "timeOfSetAllAffineMvField: " << timeOfSetAllAffineMvField  << " nanoSecond and persent is:" << timeOfSetAllAffineMvField / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSetAllAffineMvField," << timeOfSetAllAffineMvField << "," << timeOfSetAllAffineMvField / (10000000 * totalProcessTime) << endl;

  cout << "timeOfStoreAffineMotion: " << timeOfStoreAffineMotion  << " nanoSecond and persent is:" << timeOfStoreAffineMotion / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfStoreAffineMotion," << timeOfStoreAffineMotion << "," << timeOfStoreAffineMotion / (10000000 * totalProcessTime) << endl;

  cout << "timeOfCtxAffineFlag: " << timeOfCtxAffineFlag  << " nanoSecond and persent is:" << timeOfCtxAffineFlag / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfCtxAffineFlag," << timeOfCtxAffineFlag << "," << timeOfCtxAffineFlag / (10000000 * totalProcessTime) << endl;

  cout << "timeOfPredictionUnit: " << timeOfPredictionUnit  << " nanoSecond and persent is:" << timeOfPredictionUnit / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfPredictionUnit," << timeOfPredictionUnit << "," << timeOfPredictionUnit / (10000000 * totalProcessTime) << endl;

  cout << "timeOfOffsetBlock: " << timeOfOffsetBlock  << " nanoSecond and persent is:" << timeOfOffsetBlock / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfOffsetBlock," << timeOfOffsetBlock << "," << timeOfOffsetBlock / (10000000 * totalProcessTime) << endl;


   // ---------------------- DMVR ----------------------------------//
  cout << "timeOfCheckDMVRCondition: " << timeOfCheckDMVRCondition   << " nanoSecond and persent is:" << timeOfCheckDMVRCondition / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfCheckDMVRCondition," << timeOfCheckDMVRCondition << "," << timeOfCheckDMVRCondition / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXFinalPaddedMCForDMVR: " << timeOfXFinalPaddedMCForDMVR  << " nanoSecond and persent is:" << timeOfXFinalPaddedMCForDMVR / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXFinalPaddedMCForDMVR," << timeOfXFinalPaddedMCForDMVR << "," << timeOfXFinalPaddedMCForDMVR / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXProcessDMVR: " << timeOfXProcessDMVR   << " nanoSecond and persent is:" << timeOfXProcessDMVR / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXProcessDMVR," << timeOfXProcessDMVR << "," << timeOfXProcessDMVR / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXBIPMVRefine: " << timeOfXBIPMVRefine   << " nanoSecond and persent is:" << timeOfXBIPMVRefine / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXBIPMVRefine," << timeOfXBIPMVRefine << "," << timeOfXBIPMVRefine / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXPrefetch: " << timeOfXPrefetch   << " nanoSecond and persent is:" << timeOfXPrefetch / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXPrefetch," << timeOfXPrefetch << "," << timeOfXPrefetch / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXDMVRCost: " << timeOfXDMVRCost   << " nanoSecond and persent is:" << timeOfXDMVRCost / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXDMVRCost," << timeOfXDMVRCost << "," << timeOfXDMVRCost / (10000000 * totalProcessTime) << endl;

  cout << "timeOfCopyBufferSimd: " << timeOfCopyBufferSimd   << " nanoSecond and persent is:" << timeOfCopyBufferSimd / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfCopyBufferSimd," << timeOfCopyBufferSimd << "," << timeOfCopyBufferSimd / (10000000 * totalProcessTime) << endl;

  cout << "timeOfSimdInterpolateLuma10Bit2P16: " << timeOfSimdInterpolateLuma10Bit2P16   << " nanoSecond and persent is:" << timeOfSimdInterpolateLuma10Bit2P16 / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSimdInterpolateLuma10Bit2P16," << timeOfSimdInterpolateLuma10Bit2P16 << "," << timeOfSimdInterpolateLuma10Bit2P16 / (10000000 * totalProcessTime) << endl;

  cout << "timeOfSimdInterpolateLuma10Bit2P4: " << timeOfSimdInterpolateLuma10Bit2P4   << " nanoSecond and persent is:" << timeOfSimdInterpolateLuma10Bit2P4 / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSimdInterpolateLuma10Bit2P4," << timeOfSimdInterpolateLuma10Bit2P4 << "," << timeOfSimdInterpolateLuma10Bit2P4 / (10000000 * totalProcessTime) << endl;

  cout << "timeOfPaddingSimd: " << timeOfPaddingSimd   << " nanoSecond and persent is:" << timeOfPaddingSimd / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfPaddingSimd," << timeOfPaddingSimd << "," << timeOfPaddingSimd / (10000000 * totalProcessTime) << endl;

  cout << "timeOfSimdInterpolateN2_10BIT_M4: " << timeOfSimdInterpolateN2_10BIT_M4   << " nanoSecond and persent is:" << timeOfSimdInterpolateN2_10BIT_M4 / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSimdInterpolateN2_10BIT_M4," << timeOfSimdInterpolateN2_10BIT_M4 << "," << timeOfSimdInterpolateN2_10BIT_M4 / (10000000 * totalProcessTime) << endl;

  cout << "timeOfSimdInterpolateLuma10Bit2P8: " << timeOfSimdInterpolateLuma10Bit2P8   << " nanoSecond and persent is:" << timeOfSimdInterpolateLuma10Bit2P8 / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSimdInterpolateLuma10Bit2P8," << timeOfSimdInterpolateLuma10Bit2P8 << "," << timeOfSimdInterpolateLuma10Bit2P8 / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXinitMC: " << timeOfXinitMC   << " nanoSecond and persent is:" << timeOfXinitMC / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXinitMC," << timeOfXinitMC << "," << timeOfXinitMC / (10000000 * totalProcessTime) << endl;

  cout << "timeOfSimdFilter: " << timeOfSimdFilter   << " nanoSecond and persent is:" << timeOfSimdFilter / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSimdFilter," << timeOfSimdFilter << "," << timeOfSimdFilter / (10000000 * totalProcessTime) << endl;

  cout << "timeOfIsBiPredFromDifferentDirEqDistPoc: " << timeOfIsBiPredFromDifferentDirEqDistPoc   << " nanoSecond and persent is:" << timeOfIsBiPredFromDifferentDirEqDistPoc / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfIsBiPredFromDifferentDirEqDistPoc," << timeOfIsBiPredFromDifferentDirEqDistPoc << "," << timeOfIsBiPredFromDifferentDirEqDistPoc / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXPad: " << timeOfXPad   << " nanoSecond and persent is:" << timeOfXPad / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXPad," << timeOfXPad << "," << timeOfXPad / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXSubPelErrorSrfc: " << timeOfXSubPelErrorSrfc   << " nanoSecond and persent is:" << timeOfXSubPelErrorSrfc / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXSubPelErrorSrfc," << timeOfXSubPelErrorSrfc << "," << timeOfXSubPelErrorSrfc / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXDMVRSubPixelErrorSurface: " << timeOfXDMVRSubPixelErrorSurface   << " nanoSecond and persent is:" << timeOfXDMVRSubPixelErrorSurface / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXDMVRSubPixelErrorSurface," << timeOfXDMVRSubPixelErrorSurface << "," << timeOfXDMVRSubPixelErrorSurface / (10000000 * totalProcessTime) << endl;

  cout << "timeOfFilterVer: " << timeOfFilterVer   << " nanoSecond and persent is:" << timeOfFilterVer / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfFilterVer," << timeOfFilterVer << "," << timeOfFilterVer / (10000000 * totalProcessTime) << endl;

  cout << "timeOfDiv_for_maxq7: " << timeOfDiv_for_maxq7   << " nanoSecond and persent is:" << timeOfDiv_for_maxq7 / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfDiv_for_maxq7," << timeOfDiv_for_maxq7 << "," << timeOfDiv_for_maxq7 / (10000000 * totalProcessTime) << endl;

  cout << "timeOfFilterHor: " << timeOfFilterHor   << " nanoSecond and persent is:" << timeOfFilterHor / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfFilterHor," << timeOfFilterHor << "," << timeOfFilterHor / (10000000 * totalProcessTime) << endl;

  cout << "timeOfSetRefinedMotionField: " << timeOfSetRefinedMotionField   << " nanoSecond and persent is:" << timeOfSetRefinedMotionField / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSetRefinedMotionField," << timeOfSetRefinedMotionField << "," << timeOfSetRefinedMotionField / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXUseStrongFiltering: " << timeOfXUseStrongFiltering   << " nanoSecond and persent is:" << timeOfXUseStrongFiltering / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXUseStrongFiltering," << timeOfXUseStrongFiltering << "," << timeOfXUseStrongFiltering / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXReconIntraQT: " << timeOfXReconIntraQT   << " nanoSecond and persent is:" << timeOfXReconIntraQT / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXReconIntraQT," << timeOfXReconIntraQT << "," << timeOfXReconIntraQT / (10000000 * totalProcessTime) << endl;


  // ---------------------- Triangle ----------------------------------//

  cout << "timeOfXWeightedTriangleBlk_SSE: " << timeOfXWeightedTriangleBlk_SSE   << " nanoSecond and persent is:" << timeOfXWeightedTriangleBlk_SSE / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXWeightedTriangleBlk_SSE," << timeOfXWeightedTriangleBlk_SSE << "," << timeOfXWeightedTriangleBlk_SSE / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXCheckRDCostMergeTriangle2Nx2N: " << timeOfXCheckRDCostMergeTriangle2Nx2N   << " nanoSecond and persent is:" << timeOfXCheckRDCostMergeTriangle2Nx2N / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXCheckRDCostMergeTriangle2Nx2N," << timeOfXCheckRDCostMergeTriangle2Nx2N << "," << timeOfXCheckRDCostMergeTriangle2Nx2N / (10000000 * totalProcessTime) << endl;

  cout << "timeOfWeightedTriangleBlk: " << timeOfWeightedTriangleBlk   << " nanoSecond and persent is:" << timeOfWeightedTriangleBlk / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfWeightedTriangleBlk," << timeOfWeightedTriangleBlk << "," << timeOfWeightedTriangleBlk / (10000000 * totalProcessTime) << endl;

  cout << "timeOfGetTriangleMergeCandidates: " << timeOfGetTriangleMergeCandidates   << " nanoSecond and persent is:" << timeOfGetTriangleMergeCandidates / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfGetTriangleMergeCandidates," << timeOfGetTriangleMergeCandidates << "," << timeOfGetTriangleMergeCandidates / (10000000 * totalProcessTime) << endl;

  cout << "timeOfSpanTriangleMotionInfo: " << timeOfSpanTriangleMotionInfo   << " nanoSecond and persent is:" << timeOfSpanTriangleMotionInfo / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfSpanTriangleMotionInfo," << timeOfSpanTriangleMotionInfo << "," << timeOfSpanTriangleMotionInfo / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXCheckSimilarMotion: " << timeOfXCheckSimilarMotion   << " nanoSecond and persent is:" << timeOfXCheckSimilarMotion / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXCheckSimilarMotion," << timeOfXCheckSimilarMotion << "," << timeOfXCheckSimilarMotion / (10000000 * totalProcessTime) << endl;

  cout << "timeOfTraverseTUs: " << timeOfTraverseTUs   << " nanoSecond and persent is:" << timeOfTraverseTUs / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfTraverseTUs," << timeOfTraverseTUs << "," << timeOfTraverseTUs / (10000000 * totalProcessTime) << endl;

  cout << "timeOfBinProbModel_Std: " << timeOfBinProbModel_Std   << " nanoSecond and persent is:" << timeOfBinProbModel_Std / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfBinProbModel_Std," << timeOfBinProbModel_Std << "," << timeOfBinProbModel_Std / (10000000 * totalProcessTime) << endl;

  cout << "timeOfCoding_tree: " << timeOfCoding_tree   << " nanoSecond and persent is:" << timeOfCoding_tree / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfCoding_tree," << timeOfCoding_tree << "," << timeOfCoding_tree / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXSetEdgefilterMultiple: " << timeOfXSetEdgefilterMultiple   << " nanoSecond and persent is:" << timeOfXSetEdgefilterMultiple / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXSetEdgefilterMultiple," << timeOfXSetEdgefilterMultiple << "," << timeOfXSetEdgefilterMultiple / (10000000 * totalProcessTime) << endl;

  cout << "timeOfCtxInterDir: " << timeOfCtxInterDir   << " nanoSecond and persent is:" << timeOfCtxInterDir / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfCtxInterDir," << timeOfCtxInterDir << "," << timeOfCtxInterDir / (10000000 * totalProcessTime) << endl;

  cout << "timeOfxSetLoopfilterParam: " << timeOfxSetLoopfilterParam   << " nanoSecond and persent is:" << timeOfxSetLoopfilterParam / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfxSetLoopfilterParam," << timeOfxSetLoopfilterParam << "," << timeOfxSetLoopfilterParam / (10000000 * totalProcessTime) << endl;

  cout << "timeOfMvp_flag: " << timeOfMvp_flag   << " nanoSecond and persent is:" << timeOfMvp_flag / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfMvp_flag," << timeOfMvp_flag << "," << timeOfMvp_flag / (10000000 * totalProcessTime) << endl;

  cout << "timeOfInitROMer: " << timeOfInitROMer   << " nanoSecond and persent is:" << timeOfInitROMer / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfInitROMer," << timeOfInitROMer << "," << timeOfInitROMer / (10000000 * totalProcessTime) << endl;


  // ---------------------- GBi ----------------------------------//
  cout << "timeOfXSetGtxFlagBits: " << timeOfXSetGtxFlagBits   << " nanoSecond and persent is:" << timeOfXSetGtxFlagBits / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXSetGtxFlagBits," << timeOfXSetGtxFlagBits << "," << timeOfXSetGtxFlagBits / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXSetSigFlagBits: " << timeOfXSetSigFlagBits  << " nanoSecond and persent is:" << timeOfXSetSigFlagBits / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXSetSigFlagBits," << timeOfXSetSigFlagBits << "," << timeOfXSetSigFlagBits / (10000000 * totalProcessTime) << endl;

  cout << "timeOfCu_gbi_flag: " << timeOfCu_gbi_flag  << " nanoSecond and persent is:" << timeOfCu_gbi_flag / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfCu_gbi_flag," << timeOfCu_gbi_flag << "," << timeOfCu_gbi_flag / (10000000 * totalProcessTime) << endl;

  cout << "timeOfAddWeightedAvg: " << timeOfAddWeightedAvg  << " nanoSecond and persent is:" << timeOfAddWeightedAvg / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfAddWeightedAvg," << timeOfAddWeightedAvg << "," << timeOfAddWeightedAvg / (10000000 * totalProcessTime) << endl;

  cout << "timeOfRemoveWeightHighFreq_SSE: " << timeOfRemoveWeightHighFreq_SSE  << " nanoSecond and persent is:" << timeOfRemoveWeightHighFreq_SSE / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfRemoveWeightHighFreq_SSE," << timeOfRemoveWeightHighFreq_SSE << "," << timeOfRemoveWeightHighFreq_SSE / (10000000 * totalProcessTime) << endl;

  cout << "timeOfUpdateCandList: " << timeOfUpdateCandList  << " nanoSecond and persent is:" << timeOfUpdateCandList / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfUpdateCandList," << timeOfUpdateCandList << "," << timeOfUpdateCandList / (10000000 * totalProcessTime) << endl;

  // ---------------------- BDOF or BIO ----------------------------------//
  cout << "timeOfApplyBiOptFlow: " << timeOfApplyBiOptFlow  << " nanoSecond and persent is:" << timeOfApplyBiOptFlow / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfApplyBiOptFlow," << timeOfApplyBiOptFlow << "," << timeOfApplyBiOptFlow / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXAddBIOAvg4: " << timeOfXAddBIOAvg4  << " nanoSecond and persent is:" << timeOfXAddBIOAvg4 / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXAddBIOAvg4," << timeOfXAddBIOAvg4 << "," << timeOfXAddBIOAvg4 / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXBioGradFilter: " << timeOfXBioGradFilter  << " nanoSecond and persent is:" << timeOfXBioGradFilter / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXBioGradFilter," << timeOfXBioGradFilter << "," << timeOfXBioGradFilter / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXSubPuBio: " << timeOfXSubPuBio  << " nanoSecond and persent is:" << timeOfXSubPuBio / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXSubPuBio," << timeOfXSubPuBio << "," << timeOfXSubPuBio / (10000000 * totalProcessTime) << endl;

  cout << "timeOfCalcBIOSums_SSE: " << timeOfCalcBIOSums_SSE  << " nanoSecond and persent is:" << timeOfCalcBIOSums_SSE / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfCalcBIOSums_SSE," << timeOfCalcBIOSums_SSE << "," << timeOfCalcBIOSums_SSE / (10000000 * totalProcessTime) << endl;

  cout << "timeOfAddBIOAvg4_SSE: " << timeOfAddBIOAvg4_SSE  << " nanoSecond and persent is:" << timeOfAddBIOAvg4_SSE / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfAddBIOAvg4_SSE," << timeOfAddBIOAvg4_SSE << "," << timeOfAddBIOAvg4_SSE / (10000000 * totalProcessTime) << endl;

  cout << "timeOfRightShiftMSB: " << timeOfRightShiftMSB  << " nanoSecond and persent is:" << timeOfRightShiftMSB / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfRightShiftMSB," << timeOfRightShiftMSB << "," << timeOfRightShiftMSB / (10000000 * totalProcessTime) << endl;

  cout << "timeOfIsBiPredFromDifferentDir: " << timeOfIsBiPredFromDifferentDir  << " nanoSecond and persent is:" << timeOfIsBiPredFromDifferentDir / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfIsBiPredFromDifferentDir," << timeOfIsBiPredFromDifferentDir << "," << timeOfIsBiPredFromDifferentDir / (10000000 * totalProcessTime) << endl;

  cout << "timeOfGeneIntrainterPred: " << timeOfGeneIntrainterPred  << " nanoSecond and persent is:" << timeOfGeneIntrainterPred / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfGeneIntrainterPred," << timeOfGeneIntrainterPred << "," << timeOfGeneIntrainterPred / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXIntraRecBlk: " << timeOfXIntraRecBlk  << " nanoSecond and persent is:" << timeOfXIntraRecBlk / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXIntraRecBlk," << timeOfXIntraRecBlk << "," << timeOfXIntraRecBlk / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXCalcDQ: " << timeOfXCalcDQ  << " nanoSecond and persent is:" << timeOfXCalcDQ / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXCalcDQ," << timeOfXCalcDQ << "," << timeOfXCalcDQ / (10000000 * totalProcessTime) << endl;

  cout << "timeOfTransformUnit: " << timeOfTransformUnit  << " nanoSecond and persent is:" << timeOfTransformUnit / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfTransformUnit," << timeOfTransformUnit << "," << timeOfTransformUnit / (10000000 * totalProcessTime) << endl;

  cout << "timeOfXPatternSearchFast: " << timeOfXPatternSearchFast  << " nanoSecond and persent is:" << timeOfXPatternSearchFast / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfXPatternSearchFast," << timeOfXPatternSearchFast << "," << timeOfXPatternSearchFast / (10000000 * totalProcessTime) << endl;

  cout << "timeOfInterHadActive: " << timeOfInterHadActive  << " nanoSecond and persent is:" << timeOfInterHadActive / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfInterHadActive," << timeOfInterHadActive << "," << timeOfInterHadActive / (10000000 * totalProcessTime) << endl;

  cout << "timeOfCompressGOP: " << timeOfCompressGOP  << " nanoSecond and persent is:" << timeOfCompressGOP / (10000000 * totalProcessTime) << endl;
  MyExcelFile << "timeOfCompressGOP," << timeOfCompressGOP << "," << timeOfCompressGOP / (10000000 * totalProcessTime) << endl;

  // ----------------------    -----------------------------------------//
  
  cout << "timeOfXPredAffineInterSearch_1: " << timeOfXPredAffineInterSearch_1  << " nanoSecond and persent is:" << (timeOfXPredAffineInterSearch_1 * 100  / timeOfXPredAffineInterSearch) << endl;
  MyExcelFile << "timeOfXPredAffineInterSearch_1," << timeOfXPredAffineInterSearch_1 << "," << (timeOfXPredAffineInterSearch_1 * 100 / timeOfXPredAffineInterSearch)<< endl;

  cout << "timeOfXPredAffineInterSearch_2: " << timeOfXPredAffineInterSearch_2  << " nanoSecond and persent is:" << (timeOfXPredAffineInterSearch_2 * 100  / timeOfXPredAffineInterSearch) << endl;
  MyExcelFile << "timeOfXPredAffineInterSearch_2," << timeOfXPredAffineInterSearch_2 << "," << (timeOfXPredAffineInterSearch_2 * 100 / timeOfXPredAffineInterSearch)<< endl;

  cout << "timeOfXPredAffineInterSearch_3: " << timeOfXPredAffineInterSearch_3  << " nanoSecond and persent is:" << (timeOfXPredAffineInterSearch_3 * 100  / timeOfXPredAffineInterSearch) << endl;
  MyExcelFile << "timeOfXPredAffineInterSearch_3," << timeOfXPredAffineInterSearch_3 << "," << (timeOfXPredAffineInterSearch_3 * 100 / timeOfXPredAffineInterSearch)<< endl;

  cout << "accurateTime: " << accurateTime << endl;
  MyExcelFile << "accurateTime," << accurateTime << endl;

  cout << "approximateTime: " << approximateTime << endl;
  MyExcelFile << "approximateTime," << approximateTime << endl;

  // ---------------------- EncodeTime ----------------------------------//

  MyExcelFile << endl << endl << endl << "EncodeTime(second)," << totalProcessTime << "," << "100" << endl;


#if JVET_O0756_CALCULATE_HDRMETRICS
  printf(" Encoding Time (Total Time): %12.3f ( %12.3f ) sec. [user] %12.3f ( %12.3f ) sec. [elapsed]\n",
         ((endClock - startClock) * 1.0 / CLOCKS_PER_SEC) - (metricTimeuser/1000.0),
         (endClock - startClock) * 1.0 / CLOCKS_PER_SEC,
         encTime / 1000.0,
         totalTime / 1000.0);
#else
  printf(" Total Time: %12.3f sec. [user] %12.3f sec. [elapsed]\n",
         (endClock - startClock) * 1.0 / CLOCKS_PER_SEC,
         encTime / 1000.0);
#endif

  return 0;
}

//! \}
