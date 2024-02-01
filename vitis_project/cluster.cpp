/*
# Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: X11
*/

#include "cluster.h"
#include  <iostream>

hit_t pairInitilizer{0,0};
hit_t clusterConstituents[clusterDepth][maxPixelsInCluster];

bool isInRange(int min, int value, int max)
{
  return min <= value && value <= max;
}

void writeCluster(const int index)
{
  //Now remove the cluster and shift everything back one in the array                                   
  for (unsigned int j = index; j < clusterDepth - 1; j++)
  {
    #pragma HLS loop_flatten off
    for (unsigned int k = 0; k < maxPixelsInCluster; k++)
    {
      #pragma HLS unroll
      clusterConstituents[j][k] = clusterConstituents[j + 1][k];
    }

    if (clusterConstituents[j][0] == pairInitilizer) break;
  }
}

void cluster_algo(hit_t source[bufferSize], int &nClusters)
{
  nClusters = 0; 

  ClusterAlgoBegin_Loop:
  for (unsigned int k = 0; k < bufferSize; k++) 
  {
    #pragma HLS loop_flatten off

    ClusterDepth_Loop:
    for(unsigned int i = 0; i < clusterDepth; i++)
    {
      hit_t theFirstPixelPair = clusterConstituents[i][0];
      if (theFirstPixelPair.first != 0 && theFirstPixelPair.second != 0) //We have a cluster here, lets check if this hit belongs to it                                                        
      {
        bool addPairToCluster = false;
        int deltaColumn = 0;
        int deltaRow = 0;
  
        for (unsigned int j = 0; j < maxPixelsInCluster; j++)
        {
          #pragma HLS unroll 
  	  hit_t clusterPair = clusterConstituents[i][j]; 
  	  if (clusterPair == pairInitilizer) //we've found the empty array elemnt, check and see if this cluster is complete                                                                                   
  	  {
  	    if (deltaColumn > 1)
  	    {
  	      writeCluster(i);
  	      ++nClusters;
  	      --i;
  	    }
  	    break;
  	  }
  	
          deltaColumn = source[k].first - clusterPair.first; //If the difference between this hits column and the last column in the cluster is > 1, we've completed the cluster                                     
  	  deltaRow = source[k].second - clusterPair.second;
  	  bool adjacentPixel = isInRange(0, deltaColumn, 1) && isInRange(-1, deltaRow, 1); 
  	  if (adjacentPixel)
  	  {
  	    addPairToCluster = true;
  	    break;
  	  }
        }
  	      
        if(addPairToCluster)
        {
          int nConstituents = 0;
          for (int n = 0; n < maxPixelsInCluster; n++)
          {
            #pragma HLS unroll
  
  	    hit_t pixelPair = clusterConstituents[i][n]; 
  	    if (pixelPair.first == 0 && pixelPair.second == 0) break;
  	    ++nConstituents;
  	  }
  
          clusterConstituents[i][nConstituents] = source[k];
          break; //Only to be used when we add a hit to a cluster                                       
        }
        else continue; //Use a continue to get to the next cluster array. Is this right?                
    
      }
      else
      {
        clusterConstituents[i][0] = source[k];
        break;
      }
  	
     }
   } // end the for loop

   while(clusterConstituents[0][0].first != 0 && clusterConstituents[0][0].second != 0)
   {
     writeCluster(0);
     ++nClusters;
   }
}

void read_data(hit_t input[nHits], hit_t buf[bufferSize])
{
  int r;
  
  RD_Loop_Row:
  for (r = 0; r < bufferSize; r++)
  {
    buf[r] = input[r];
  }
}

void write_data(int toWrite, int &output)
{
  WR_Loop_Row:
  output = toWrite;
}

void cluster(hit_t in[nHits], int &out)
{

   hit_t buf_in[bufferSize];
   int outCount = 0; 

   // Read input data. Fill the internal buffer.
   read_data(in, buf_in);

   cluster_algo(buf_in, outCount);

   #ifndef __SYNTHESIS__
   std::cout << "outCount is " << outCount << std::endl;
   #endif

   // Write out the results.
   write_data(outCount, out);
}

