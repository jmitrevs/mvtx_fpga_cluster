/*
# Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: X11
*/

#include "cluster.h"
#include  <iostream>
#include <cmath>

hit_t pairInitilizer{0, 0}; //If this is  (0,0) then we miss any pixel in the corner. To use this pixel, set initialiser to (-1,-1) but you need to initialise all the array elements to -1 inside cluster algo so could be a memory issue
hit_t clusterConstituents[clusterDepth][maxPixelsInCluster];

bool isInRange(int min, int value, int max) //This may be an issue. Vitis might not allow return values so a void could be used here (or just write the logic out)
{
  return min <= value && value <= max;
}

void writeCluster(const int index, cluster_t& myCluster) //Can we handle moving clusters in global memory or do we need to pass the cluster list as an IO
{
  //Remember to weight from center of pixel, not the edge (add 0.5 to each pixel)
  //This should be right but the rows count from the top left of the chip (so row 0 is at +0.687cm while rown 511 is at -0.687cm)

  float precise_col = 0;
  float precise_row = 0;

  int nConstituents = 0;
  writeCluster_findNumPixels:
  for (unsigned int i = 0; i < maxPixelsInCluster; ++i)
  {
    #pragma HLS PIPELINE II=5
    if (clusterConstituents[index][i] == pairInitilizer) break;
    ++nConstituents;
  }


  writeCluster_getPreciseCentroid:
  for (int i = 0; i < nConstituents; i++)
  {
    #pragma HLS UNROLL
    precise_col += (clusterConstituents[index][i].first + 0.5) / nConstituents;
    precise_row += (clusterConstituents[index][i].second + 0.5) / nConstituents;
  }

  writeCluster_makeClusterInfo:
  myCluster.first.first = floor(precise_col); //Centroid column
  myCluster.first.second = floor(precise_row); //Centroid row
  myCluster.second.first = precise_col - myCluster.first.first < 0.5 ? 0 : 1; //Column quadrant
  myCluster.second.second = precise_row - myCluster.first.second < 0.5 ? 0 : 1; //Row quadrant

  //Now remove the cluster and shift everything back one in the array
  writeCluster_clearCluster:
  for (unsigned int j = index; j < clusterDepth - 1; j++)
  {
    #pragma HLS loop_flatten off
	 writeCluster_clearPixels:
    for (unsigned int k = 0; k < maxPixelsInCluster; k++)
    {
      #pragma HLS unroll
      #ifndef __SYNTHESIS__
      std::cout << "Cluster index  " << index << " has constituent (" << clusterConstituents[j][k].first << ", " << clusterConstituents[j][k].second << ")" << std::endl;
      #endif
      clusterConstituents[j][k] = clusterConstituents[j + 1][k];
    }
    if (clusterConstituents[j][0] == pairInitilizer) break;
  }
}

void cluster_algo(hit_t source[hitBufferSize], cluster_t sink[clusBufferSize])
{
  int write_index = 0;
  cluster_t constructedCluster; //Can we pass the whole sink to writeCluster?
  ClusterAlgoBegin_Loop:
  for (unsigned int k = 0; k < hitBufferSize; k++) 
  {
    #pragma HLS loop_flatten off

    ClusterDepth_Loop:
    for(unsigned int i = 0; i < clusterDepth; i++)
    {
      hit_t theFirstPixelPair = clusterConstituents[i][0];
      if (theFirstPixelPair != pairInitilizer) //We have a cluster here, lets check if this hit belongs to it                                                        
      {
        bool addPairToCluster = false;
        int deltaColumn = 0;
        int deltaRow = 0;
  
        for (unsigned int j = 0; j < maxPixelsInCluster; j++) // -1 to avoid running to the end of the array where we need a boundary or pair initializer
        {
          #pragma HLS unroll 
      	  hit_t clusterPair = clusterConstituents[i][j]; 

      	  if (clusterPair == pairInitilizer) //we've found the empty array element, check and see if this cluster is complete
      	  {
      	    if (deltaColumn > 1) //Careful here! We dont write a cluster until the next loop iteration so we need an extra memory slot
      	    {
      	      writeCluster(i, constructedCluster);
              sink[write_index] = constructedCluster;
              ++write_index;
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
  	        if (pixelPair == pairInitilizer) break;
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

   while(clusterConstituents[0][0] != pairInitilizer)
   {
     writeCluster(0, constructedCluster);
     sink[write_index] = constructedCluster;
      ++write_index;
   }
}

void read_data(hit_t input[nHits], hit_t buf[hitBufferSize])
{
  RD_Loop_Row:
  for (int i = 0; i < hitBufferSize; i++)
  {
    buf[i] = input[i];
  }
}

void write_data(cluster_t buf[clusBufferSize], cluster_t output[nClusters])
{
  WR_Loop_Row:
  for (int i = 0; i < clusBufferSize; i++)
  {
    output[i] = buf[i];
  }
}

void cluster(hit_t in[nHits], cluster_t out[nClusters])
{
   hit_t buf_in[hitBufferSize];
   cluster_t buf_out[clusBufferSize];

   // Read input data. Fill the internal buffer.
   read_data(in, buf_in);

   cluster_algo(buf_in, buf_out);

   // Write out the results.
   write_data(buf_out, out);

   //#ifndef __SYNTHESIS__
   for (unsigned int i = 0; i < nClusters; i++)
     std::cout << "Cluster info: (col, row) = (" << out[i].first.first << ", " << out[i].first.second << "), (col quad, row quad) = (" << out[i].second.first << ", " << out[i].second.second << ")" << std::endl;
   //#endif
}

