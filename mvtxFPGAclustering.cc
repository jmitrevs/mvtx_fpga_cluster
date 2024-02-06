#include "mvtxFPGAclustering.h"

#include <fun4all/Fun4AllReturnCodes.h>

#include <phool/getClass.h>
#include <phool/PHCompositeNode.h>
#include <phool/PHNodeIterator.h>

#include <json/json.h> 
#include <fstream>

#include <TFile.h>
#include <TTree.h>

#include <mvtx/MvtxClusterizer.h>
#include <trackbase/MvtxDefs.h>
#include <trackbase/TrkrHitSetContainerv1.h>
#include <trackbase/TrkrHitv2.h>
#include <trackbase/TrkrHitSet.h>
#include <trackbase/TrkrClusterContainerv4.h>
#include <trackbase/TrkrClusterHitAssocv3.h>
#include <trackbase/TrkrClusterv4.h>

/*
// Online C++ compiler to run C++ program online
#include <iostream>
#include <bits/stdc++.h> 

int main() {
    std::bitset<4> header{0xA};
    int layer = 1;
    std::bitset<2> layer_bits{layer};
    int stave = 3; 
    int chip = 6; 
    int row = 1; 
    int col = 836; 
    int row_quadrant = 1; 
    int col_quadrant = 1;
    std::bitset<2> trailer{0x1};
    
    std::cout << header;
    if (layer == 2) std::cout << layer_bits[layer_bits.size() - 1];
    else std::cout << layer_bits;
    
    std::cout << std::bitset<5>(stave)
              << std::bitset<4>(chip)
              << std::bitset<10>(col)
              << std::bitset<9>(row)
              << std::bitset<1>(col_quadrant)
              << std::bitset<1>(row_quadrant)
              << trailer;

    return 0;
}
*/

unsigned int f4aCounter = 0; //Need to have geometry set first for clustering but this module is getting run before all that so need to do logic in process_event
const unsigned int maxClusters = 10;
const unsigned int maxPixelsInCluster = 25;
std::pair<int, int> pairInitilizer{0,0};
std::pair<int, int> clusterConstituents[maxClusters][maxPixelsInCluster]; //The arrays should hold up to 10 clusters with up to 25 pixel pairs per cluster
//We should do a study to optimise this but my input JSON file has mas 7 clusters on a chip and a max cluster size of 25

//____________________________________________________________________________..
mvtxFPGAclustering::mvtxFPGAclustering(const std::string &name):
 SubsysReco(name)
{
}

//____________________________________________________________________________..
mvtxFPGAclustering::~mvtxFPGAclustering()
{
}

//____________________________________________________________________________..
int mvtxFPGAclustering::Init(PHCompositeNode *topNode)
{

  return Fun4AllReturnCodes::EVENT_OK;
}

//____________________________________________________________________________..
int mvtxFPGAclustering::process_event(PHCompositeNode *topNode)
{
  if (f4aCounter != 0) return Fun4AllReturnCodes::EVENT_OK;

  TFile* outFile = new TFile(m_outFile.c_str(), "RECREATE");
  TTree* outTree = new TTree("Clusters", "Clusters");
  outTree->OptimizeBaskets();
  outTree->SetAutoSave(-5e6);

  PHNodeIterator dstiter(topNode);

  PHCompositeNode* dstNode = dynamic_cast<PHCompositeNode *>(dstiter.findFirst("PHCompositeNode", "DST"));
  if (!dstNode)
  {
    std::cout << "MvtxCombinedRawDataDecoder::InitRun - DST Node missing, doing nothing." << std::endl;
    exit(1);
  }

  TrkrHitSetContainerv1 *trkrHitSetContainer = findNode::getClass<TrkrHitSetContainerv1>(dstNode, "TRKR_HITSET");
  if (!trkrHitSetContainer)
  {
    PHCompositeNode *trkrNode = dynamic_cast<PHCompositeNode *>(dstiter.findFirst("PHCompositeNode", "TRKR"));
    if (!trkrNode)
      {
        trkrNode = new PHCompositeNode("TRKR");
        dstNode->addNode(trkrNode);
      }

    trkrHitSetContainer = new TrkrHitSetContainerv1;
    PHIODataNode<PHObject> *TrkrHitSetContainerNode = new PHIODataNode<PHObject>(trkrHitSetContainer, "TRKR_HITSET", "PHObject");
    trkrNode->addNode(TrkrHitSetContainerNode);
  }

  auto trkrclusters = findNode::getClass<TrkrClusterContainer>(dstNode, "TRKR_CLUSTER");
  if (!trkrclusters)
  {
    PHCompositeNode *trkrNode = dynamic_cast<PHCompositeNode *>(dstiter.findFirst("PHCompositeNode", "TRKR"));
    trkrclusters = new TrkrClusterContainerv4;
    PHIODataNode<PHObject> *TrkrClusterContainerNode = new PHIODataNode<PHObject>(trkrclusters, "TRKR_CLUSTER", "PHObject");
    trkrNode->addNode(TrkrClusterContainerNode);
  }

  auto clusterhitassoc = findNode::getClass<TrkrClusterHitAssoc>(dstNode,"TRKR_CLUSTERHITASSOC");
  if(!clusterhitassoc)
  {
    PHCompositeNode *trkrNode = dynamic_cast<PHCompositeNode *>(dstiter.findFirst("PHCompositeNode", "TRKR"));
    clusterhitassoc = new TrkrClusterHitAssocv3;
    PHIODataNode<PHObject> *newNode = new PHIODataNode<PHObject>(clusterhitassoc, "TRKR_CLUSTERHITASSOC", "PHObject");
    trkrNode->addNode(newNode);
  }

  outTree->Branch("event", &event, "event/i");
  outTree->Branch("layer", &layer, "layer/i");
  outTree->Branch("stave", &stave, "stave/i");
  outTree->Branch("chip", &chip, "chip/i");
  outTree->Branch("localX_offline", &localX);
  outTree->Branch("localY_offline", &localY);
  outTree->Branch("clusZ_offline", &clusZ);
  outTree->Branch("clusPhi_offline", &clusPhi);
  outTree->Branch("clusSize_offline", &clusSize);
  outTree->Branch("nClustersOnChip_offline", &nClusters, "nClustersOnChip_offline/i");
  outTree->Branch("localX_fpga", &localX_fpga);
  outTree->Branch("localY_fpga", &localY_fpga);
  outTree->Branch("clusSize_fpga", &clusSize_fpga);
  outTree->Branch("nClustersOnChip_fpga", &nClusters_fpga, "nClustersOnChip_fpga/i");

  std::ifstream jsonFile(m_inFile.c_str(), std::ifstream::binary);
  Json::Value information;
  jsonFile >> information;

  const Json::Value events = information["Events"];
  unsigned int nEvents = m_runFast ? 1 : events.size();
  for (unsigned int i = 0; i < nEvents; ++i)
  {
    trkrclusters->Reset();    
    trkrHitSetContainer->Reset();
    clusterhitassoc->Reset();

    event = i;

    const Json::Value hits = events[i]["RawHit"]["MVTXHits"];
    unsigned int nHits = hits.size();
    for (unsigned int j = 0; j < nHits; ++j)
    {
      const Json::Value hitInfo = hits[j]["ID"];

      layer = hitInfo["Layer"].asInt();
      stave = hitInfo["Stave"].asInt();
      chip = hitInfo["Chip"].asInt();
      row = hitInfo["Pixel_x"].asInt();
      col = hitInfo["Pixel_z"].asInt();

      auto hitsetkey = MvtxDefs::genHitSetKey(layer, stave, chip, 0);
      TrkrHitSetContainer::Iterator hitsetit = trkrHitSetContainer->findOrAddHitSet(hitsetkey);

      TrkrDefs::hitkey hitkey = MvtxDefs::genHitKey(col, row);
      TrkrHit* hit = nullptr;
      hit = hitsetit->second->getHit(hitkey);
      if (!hit)
      {
        // Otherwise, create a new one
        hit = new TrkrHitv2();
        hitsetit->second->addHitSpecificKey(hitkey, hit);
      }
    }

    MvtxClusterizer myClusterizer;
    myClusterizer.process_event(topNode);

    TrkrHitSetContainer::ConstRange hitsetrange = trkrHitSetContainer->getHitSets(TrkrDefs::TrkrId::mvtxId);

    for (TrkrHitSetContainer::ConstIterator hitsetitr = hitsetrange.first; hitsetitr != hitsetrange.second; ++hitsetitr)
    {
      TrkrHitSet *hitset = hitsetitr->second;
      auto hitsetkey = hitset->getHitSetKey();

      myHitSet.layer = layer = TrkrDefs::getLayer(hitsetkey);
      myHitSet.stave = stave = MvtxDefs::getStaveId(hitsetkey);
      myHitSet.chip = chip = MvtxDefs::getChipId(hitsetkey);

      if (Verbosity() >= VERBOSITY_SOME) std::cout << "\n*\n*\n*\n   Hit info for layer " << myHitSet.layer << ", stave " << myHitSet.stave << ", chip " << myHitSet.chip << "\n*\n*\n*\n";
 
      TrkrClusterContainer::ConstRange clusterrange = trkrclusters->getClusters(hitsetkey);

      for (TrkrClusterContainer::ConstIterator clusteritr = clusterrange.first; clusteritr != clusterrange.second; ++clusteritr)
      {
        TrkrCluster *cluster = clusteritr->second;

        localX.push_back(cluster->getLocalX());
        localY.push_back(cluster->getLocalY());
        clusZ.push_back(cluster->getZSize());
        clusPhi.push_back(cluster->getPhiSize());
        clusSize.push_back(cluster->getAdc());
        ++nClusters;
      }

      TrkrHitSet::ConstRange hitsetrange = hitset->getHits();

      for (TrkrHitSet::ConstIterator hititr = hitsetrange.first; hititr != hitsetrange.second; ++hititr)
      {
        TrkrDefs::hitkey hitKey = hititr->first;
        col = MvtxDefs::getCol(hitKey);
        row = MvtxDefs::getRow(hitKey);
        myHitSet.addHit(col, row);
      }
      
      //OK, now cluster this chip
      myFPGAClusters = runFPGAClusterAlgorithm(myHitSet);
      
      outTree->Fill();

      nClusters = 0;
      nClusters_fpga = 0;
      localX.clear();
      localY.clear();
      clusZ.clear();
      clusPhi.clear();
      clusSize.clear();
      localX_fpga.clear();
      localY_fpga.clear();
      clusSize_fpga.clear();
      myHitSet.hits.clear();
      myFPGAClusters.clusters.clear();

    }
  }

  outFile->Write();
  outFile->Close();
  delete outFile;

  ++f4aCounter;

  return Fun4AllReturnCodes::EVENT_OK;
}

//____________________________________________________________________________..
int mvtxFPGAclustering::End(PHCompositeNode *topNode)
{
  return Fun4AllReturnCodes::EVENT_OK;
}

//____________________________________________________________________________..
int mvtxFPGAclustering::Reset(PHCompositeNode *topNode)
{
  return Fun4AllReturnCodes::EVENT_OK;
}

//____________________________________________________________________________..
void mvtxFPGAclustering::Print(const std::string &what) const
{
  std::cout << "mvtxFPGAclustering::Print(const std::string &what) const Printing info for " << what << std::endl;
}

clusset mvtxFPGAclustering::runFPGAClusterAlgorithm(const hitset &aHitSet)
{
  int nClusters = 0;

  clusset aClusSet;
  aClusSet.layer = aHitSet.layer;
  aClusSet.stave = aHitSet.stave;
  aClusSet.chip = aHitSet.chip;
  //All clustering should be done with fixed sized arrays if possible. I don't think we can dynamically allocate resources on an FPGA like with vectors
  
  for (auto &hit : aHitSet.hits)
  {
    if (Verbosity() >= VERBOSITY_SOME) std::cout << "The chip hits are: col = " << hit.first << ", row = " << hit.second << std::endl;
  }
  for (auto &hit : aHitSet.hits)
  {
    //I don't think we can use a while loop here as dealing with new clusters is tricky
    //Using a for loop that will start a new cluster if this hit doesn't belong to an existing cluster
    for(unsigned int i = 0; i < maxClusters; i++) // Only works if we move all clusters up the array like a FIFO when we finish a cluster
    {
      if (clusterConstituents[i][0].first != 0 && clusterConstituents[i][0].second != 0) //We have a cluster here, lets check if this hit belongs to it
      {
        bool addPairToCluster = false;
        int deltaColumn = 0;
        int deltaRow = 0;

        for (auto &clusterPair: clusterConstituents[i])
        { 
          if (clusterPair == pairInitilizer) //we've found the empty array elemnt, check and see if this cluster is complete
          {
            if (deltaColumn > 1) 
            {
              writeCluster(i, aClusSet);
              ++nClusters;
              --i;
            }
            break;
          }

          deltaColumn = hit.first - clusterPair.first; //If the difference between this hits column and the last column in the cluster is > 1, we've completed the cluster
          deltaRow = hit.second - clusterPair.second;
          bool adjacentPixel = isInRange(0, deltaColumn, 1) && isInRange(-1, deltaRow, 1); //Maybe this needs to be hardcoded with a direct comparison. Don't know if VHDL can handle inequalities
          if (adjacentPixel)
          {
            addPairToCluster = true;
            break;
          }
        }

        if(addPairToCluster)
        {
          int nConstituents = 0;
          for (auto &pixelPair : clusterConstituents[i])
          {
            if (pixelPair.first == 0 && pixelPair.second == 0) break;
            ++nConstituents;
          }
          clusterConstituents[i][nConstituents] = hit;
          break; //Only to be used when we add a hit to a cluster
        }
        else continue; //Use a continue to get to the next cluster array. Is this right?
      }
      else //We've found an empty array to start a new cluster inside of. Fails if there is a hit in pixel 0,0. Could use NOT logic to check that the hit isn't associated to a (0,0) pixel like (0,1) or (1,0)
      {
        clusterConstituents[i][0] = hit;
        break;
      }

    }

  }

  //Write and clear the cluster buffer
  while(clusterConstituents[0][0].first != 0 && clusterConstituents[0][0].second != 0)
  {
    writeCluster(0, aClusSet);
    ++nClusters;
  }

  if (m_writeVitisFiles)
  {
    if (nClusters == 7)
    {
      std::ofstream vitis_input;
      vitis_input.open("../vitis_project/in.dat");

      std::ofstream vitis_output;
      vitis_output.open("../vitis_project/out.golden.dat");
      for (auto &hit : aHitSet.hits)
      {
        vitis_input << hit.first << " " << hit.second << std::endl;
      }
      vitis_input.close();

      for (auto &cluster : aClusSet.clusters)
      {
        vitis_output << cluster.first.first << " " << cluster.first.second << " " << cluster.second.first << " " << cluster.second.second << std::endl;
      }
      vitis_output.close();
      
      std::cout << "Writing hits for cluster FPGA testing" << std::endl;
      exit(1);
    }
  }

  return aClusSet;
}

void mvtxFPGAclustering::calculateClusterCentroid(std::pair<int, int> constituents[], int &col, int &row, int &col_quad, int &row_quad)
{
  //Remember to weight from center of pixel, not the edge (add 0.5 to each pixel)
  //This should be right but the rows count from the top left of the chip (so row 0 is at +0.687cm while rown 511 is at -0.687cm)
  float precise_col = 0;
  float precise_row = 0;

  int nConstituents = 0;
  for (unsigned int i = 0; i < maxPixelsInCluster; ++i)
  {
    if (constituents[i].first == 0 && constituents[i].second == 0) break;
    ++nConstituents;
  }

  for (int i = 0; i < nConstituents; i++)
  {
    precise_col += (constituents[i].first + 0.5) / nConstituents;
    precise_row += (constituents[i].second + 0.5) / nConstituents;
  }

  col = floor(precise_col);
  row = floor(precise_row);
  col_quad = precise_col - col < 0.5 ? 0 : 1; 
  row_quad = precise_row - row < 0.5 ? 0 : 1; //To account for rows going in reverse 
}

void mvtxFPGAclustering::calculateLocalClusterPostiion(float &x, float &y, int col, int row, int col_quad, int row_quad)
{ 
  float pixelSize[2] = {0.002687991364, 0.002923965454};
  float chipCorner[2] = {0.686784, -1.49563};
  x = chipCorner[0] - (row + 0.5*row_quad) * pixelSize[0];
  y = chipCorner[1] + (col + 0.5*col_quad) * pixelSize[1];
}

bool mvtxFPGAclustering::isInRange(int min, int value, int max)
{
  return min <= value && value <= max;
}

void mvtxFPGAclustering::writeCluster(const int index, clusset &myClusSet)
{

  int centroid_col, centroid_row, centroid_col_quadrant, centroid_row_quadrant;
  float myX, myY;

  calculateClusterCentroid(clusterConstituents[index], centroid_col, centroid_row, centroid_col_quadrant, centroid_row_quadrant);
  calculateLocalClusterPostiion(myX, myY, centroid_col, centroid_row, centroid_col_quadrant, centroid_row_quadrant);

  int nConstituents = 0;
  for (unsigned int i = 0; i < maxPixelsInCluster; ++i)
  {
    if (clusterConstituents[index][i].first == 0 && clusterConstituents[index][i].second == 0) break;
    ++nConstituents;
  }

  myClusSet.addCluster(centroid_col, centroid_row, centroid_col_quadrant, centroid_row_quadrant);
  localX_fpga.push_back(myX);
  localY_fpga.push_back(myY);
  clusSize_fpga.push_back(nConstituents);
  ++nClusters_fpga;
 
  //Now remove the cluster and shift everything back one in the array
  for (unsigned int j = index; j < maxClusters - 1; j++)
  {
    for (unsigned int k = 0; k < maxPixelsInCluster; k++)
    {
      clusterConstituents[j][k] = clusterConstituents[j + 1][k];
    }
    if (clusterConstituents[j][0] == pairInitilizer) break;
  }
}
