// Tell emacs that this is a C++ source
//  -*- C++ -*-.
#ifndef MVTXFPGACLUSTERING_H
#define MVTXFPGACLUSTERING_H

#include <fun4all/SubsysReco.h>

#include <string>
#include <vector>
#include <bits/stdc++.h>

class PHCompositeNode;

class hitset {
  public:

   hitset(const std::string &name = "hitset") {}
   ~hitset() {}

   int layer;
   int stave;
   int chip;
   std::vector<std::pair<int, int>> hits;
   void addHit(int col, int row) { auto hitpair = std::make_pair(col, row); hits.push_back(hitpair); std::sort(hits.begin(), hits.end()); }

   //bool operator==(const &check) const {return ((this->layer == check.layer)
   //                                          && (this->stave == check.stave)
   //                                          && (this->chip  == check.chip))}
};

class clusset {
  public:

   clusset(const std::string &name = "clusset") {}
   ~clusset() {}

   int layer;
   int stave;
   int chip;
   std::vector<std::pair<std::pair<int, int>, std::pair<int, int>>> clusters;
   void addCluster(int col, int row, int col_quadrant, int row_quadrant) { auto cluspair = std::make_pair(col, row); 
                                                                           auto quadpair = std::make_pair(col_quadrant, row_quadrant);
                                                                           auto clusterInfo = std::make_pair(cluspair, quadpair);
                                                                           clusters.push_back(clusterInfo); }
};

class mvtxFPGAclustering : public SubsysReco
{
 public:
 
  hitset myHitSet;
  clusset myFPGAClusters;

  mvtxFPGAclustering(const std::string &name = "mvtxFPGAclustering");

  ~mvtxFPGAclustering() override;

  /** Called during initialization.
      Typically this is where you can book histograms, and e.g.
      register them to Fun4AllServer (so they can be output to file
      using Fun4AllServer::dumpHistos() method).
   */
  int Init(PHCompositeNode *topNode) override;

  /** Called for first event when run number is known.
      Typically this is where you may want to fetch data from
      database, because you know the run number. A place
      to book histograms which have to know the run number.
   */
  //int InitRun(PHCompositeNode *topNode) override;

  /** Called for each event.
      This is where you do the real work.
   */
  int process_event(PHCompositeNode *topNode) override;

  /// Clean up internals after each event.
  //int ResetEvent(PHCompositeNode *topNode) override;

  /// Called at the end of each run.
  //int EndRun(const int runnumber) override;

  /// Called at the end of all processing.
  int End(PHCompositeNode *topNode) override;

  /// Reset
  int Reset(PHCompositeNode * /*topNode*/) override;

  void Print(const std::string &what = "ALL") const override;

  void runFast( bool val ) { m_runFast = val; }
  void useFile( std::string file ){ file = m_inFile; }
  void writeFile( std::string file ){ m_outFile = file; }

 private:

  unsigned int event = 0;
  unsigned int layer = 0;
  unsigned int stave = 0;
  unsigned int chip = 0;
  unsigned int row = 0;
  unsigned int col = 0;
  std::vector<float> localX;
  std::vector<float> localY;
  std::vector<float> clusZ;
  std::vector<float> clusPhi;
  std::vector<unsigned int> clusSize;
  unsigned int nClusters = 0;
  std::vector<float> localX_fpga;
  std::vector<float> localY_fpga;
  std::vector<unsigned int> clusSize_fpga;
  unsigned int nClusters_fpga = 0;

  bool m_runFast = false; 
  
  std::string m_inFile = "Signal.json";

  std::string m_outFile = "outputClusters.root";

  clusset runFPGAClusterAlgorithm(const hitset &aHitSet);
  void calculateClusterCentroid(std::pair<int, int> constituents[], int &col, int &row, int &col_quad, int &row_quad);
  void calculateLocalClusterPostiion(float &x, float &y, int col, int row, int col_quad, int row_quad);
  bool isInRange(int min, int value, int max);
  void writeCluster(const int index, clusset &myClusSet);
};

#endif // MVTXFPGACLUSTERING_H

