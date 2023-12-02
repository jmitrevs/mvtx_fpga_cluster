#include <fun4all/Fun4AllServer.h>
#include <fun4all/Fun4AllInputManager.h>
#include <fun4all/Fun4AllDstInputManager.h>

#include <G4Setup_sPHENIX.C>
#include <G4_ActsGeom.C>

#include <mvtxfpgaclustering/mvtxFPGAclustering.h> 

#include <phool/recoConsts.h>

R__LOAD_LIBRARY(libfun4all.so)
R__LOAD_LIBRARY(libmvtxFPGAclustering.so)

void Fun4All_tester(const std::string file = "/sphenix/u/ycmorales/work/sphenix/Fun4All/Run25332/mvtxCombinerOutput.root")
{
  Fun4AllServer *se = Fun4AllServer::instance();
  se->Verbosity(1);
  recoConsts *rc = recoConsts::instance();

  //Fun4AllInputManager *infile = new Fun4AllDstInputManager("DSTin");
  //infile->AddFile(file.c_str());
  //se->registerInputManager(infile);

  Enable::MVTX = true;
  Enable::INTT = true;
  Enable::TPC = true;
  Enable::MICROMEGAS = true;

  G4Init();
  G4Setup();

  mvtxFPGAclustering *myTester = new mvtxFPGAclustering();
  myTester->Verbosity(INT_MAX);
  //myTester->runFast(true);
  se->registerSubsystem(myTester);
  
  se->run(1);

  se->End();
  delete se;
  gSystem->Exit(0);
}
