/*
# Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: X11
*/
#include "hls_math.h"
#include "cluster.h"
#include  <iostream>
#include <cmath>
#include "ap_fixed.h"
#include "ap_int.h"

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

  ap_ufixed<6,2> precise_col = 0;
  ap_ufixed<5,2> precise_row = 0;

  ap_int<5> nConstituents = 0;
  writeCluster_findNumPixels:
  for (unsigned int i = 0; i < maxPixelsInCluster; ++i)
  {
    #pragma HLS PIPELINE II=5
    if (clusterConstituents[index][i] == pairInitilizer) break;
    ++nConstituents;
  }

  writeCluster_getPreciseCentroid:
  for (int i = 0; i < maxPixelsInCluster; i++)
  {
    //#pragma HLS loop_tripcount min=0 max=6
#pragma HLS unroll
    precise_col += (clusterConstituents[index][i].first);
  precise_row += (clusterConstituents[index][i].second);
  }

ap_ufixed<1,1> offset = 0.5;

  precise_col += (offset*nConstituents); 
  precise_row += (offset*nConstituents); 

  precise_col = precise_col/nConstituents; 
  precise_row = precise_row/nConstituents; 

  writeCluster_makeClusterInfo:

  myCluster.first.first = hls::floor(precise_col); //Centroid column
  myCluster.first.second = hls::floor(precise_row); //Centroid row
  // better resource usage than conditional if statement 
  ap_ufixed<5,1> diffCol = precise_col - myCluster.first.first; 
  ap_ufixed<4,1> diffRow = precise_row - myCluster.first.second;

  if(diffCol < offset){
    myCluster.second.first = 0; 
    if(diffRow < offset)myCluster.second.second = 0; 
    else myCluster.second.second = 1; 
  }
  else{
    myCluster.second.first = 1; 
    if(diffRow < offset)myCluster.second.second = 0; 
    else myCluster.second.second = 1;
  }


  //Now remove the cluster and shift everything back one in the array
  writeCluster_clearCluster:
  for (unsigned int j = index; j < clusterDepth - 1; j++)
  {
    #pragma HLS loop_flatten off
    hit_t clusterPair = clusterConstituents[j][0];
	 writeCluster_clearPixels:
    for (unsigned int k = 0; k < maxPixelsInCluster; k++)
    {
      #pragma HLS unroll
      #ifndef __SYNTHESIS__
      std::cout << "Cluster index  " << index << " has constituent (" << clusterConstituents[j][k].first << ", " << clusterConstituents[j][k].second << ")" << std::endl;
      #endif
      clusterConstituents[j][k] = clusterConstituents[j + 1][k];
    }
    if (clusterPair == pairInitilizer) break;
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
        ap_int<2> deltaColumn = 0;
        ap_int<2> deltaRow = 0;
  
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
          ap_int<5> nConstituents = 0;
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
#pragma HLS INLINE off
  for (int i = 0; i < hitBufferSize; i++)
  {
#pragma HLS pipeline
    buf[i] = input[i];
  }
}

void write_data(cluster_t buf[clusBufferSize], cluster_t output[nClusters])
{
  WR_Loop_Row:
#pragma HLS INLINE off
  for (int i = 0; i < clusBufferSize; i++)
  {
#pragma HLS pipeline II=7
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

   #ifndef __SYNTHESIS__
   for (unsigned int i = 0; i < nClusters; i++)
     std::cout << "Cluster info: (col, row) = (" << out[i].first.first << ", " << out[i].first.second << "), (col quad, row quad) = (" << out[i].second.first << ", " << out[i].second.second << ")" << std::endl;
   #endif
}

