/*
# Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: X11
*/
#include "cluster.h"

#include <ap_fixed.h>
#include <ap_int.h>
#include <cmath>
#include <iostream>
#include <hls_math.h>

hit_t pairInitilizer{0, 0}; //If this is (0,0) then we miss any pixel in the corner. To use this pixel, set initialiser to (-1,-1) but you need to initialise all the array elements to -1 inside cluster algo so could be a memory issue
hit_t clusterConstituents[clusterDepth][maxPixelsInCluster];

//bool isInRange(ap_int<3> min, ap_int<3> value, ap_int<3> max) //This may be an issue. Vitis might not allow return values so a void could be used here (or just write the logic out)
bool isInRange(int min, int value, int max) //This may be an issue. Vitis might not allow return values so a void could be used here (or just write the logic out)
{
  return min <= value && value <= max;
}

void writeCluster(const int index, cluster_t& myCluster) //Can we handle moving clusters in global memory or do we need to pass the cluster list as an IO
{
      #ifndef __SYNTHESIS__
      std::cout << "Entered " << __func__ << std::endl;
      #endif
  //Remember to weight from center of pixel, not the edge (add 0.5 to each pixel)
  //This should be right but the rows count from the top left of the chip (so row 0 is at +0.687cm while rown 511 is at -0.687cm)

  ap_ufixed<14,10> precise_col = 0; 
  ap_ufixed<13,9> precise_row = 0; 

  unsigned int nConstituents = 0;
  //ap_uint<5> nConstituents = 0;
  writeCluster_findNumPixels:
  for (int i = 0; i < maxPixelsInCluster; ++i)
  {
      #ifndef __SYNTHESIS__
      std::cout << "Cluster index  " << index << " has constituent (" << clusterConstituents[index][i].first << ", " << clusterConstituents[index][i].second << ")" << std::endl;
      #endif

    #pragma HLS PIPELINE II=5
    if (clusterConstituents[index][i] == pairInitilizer) break;
    ++nConstituents;
  }

  writeCluster_getPreciseCentroid:
  ap_ufixed<4,0> offset = 0.5;
  for (int i = 0; i < maxPixelsInCluster; i++)
  {
    #pragma HLS unroll
    if(i == nConstituents)break; 
    //precise_col2 += (clusterConstituents[index][i].first + 0.5)/nConstituents;
    //precise_row2 += (clusterConstituents[index][i].second+0.5)/nConstituents;
    precise_col  += (clusterConstituents[index][i].first+offset)/nConstituents;
    precise_row  += (clusterConstituents[index][i].second+offset)/nConstituents;
#ifndef __SYNTHESIS__
    std::cout << "In loop: precise col is " << precise_col << " precise_row is " << precise_row << std::endl;
    //std::cout << "In loop: precise col2 is " << precise_col2 << " precise_row2 is " << precise_row2 << std::endl;
#endif

  }

//  ap_ufixed<1,0> offset = 0.5;
//#ifndef __SYNTHESIS__
//  std::cout << "offset is: " << offset << std::endl;
//#endif
//  //float offset = 0.5; 
//
  // precise_col += (offset*nConstituents); 
  //precise_row += (offset*nConstituents); 
 //
 //precise_col = precise_col/nConstituents; 
 //precise_row = precise_row/nConstituents; 

  writeCluster_makeClusterInfo:
  myCluster.first.first = hls::floor(precise_col); //Centroid column
  myCluster.first.second = hls::floor(precise_row); //Centroid row
  // better resource usage than conditional if statement 
  ap_ufixed<4,0> diffCol = precise_col - myCluster.first.first; 
  ap_ufixed<4,0> diffRow = precise_row - myCluster.first.second;
#ifndef __SYNTHESIS__
  std::cout << "precise col is " << precise_col << " precise_row is " << precise_row << std::endl;
  //std::cout << "precise col2 is " << precise_col2 << " precise_row2 is " << precise_row2 << std::endl;

  std::cout << "diffCol is " << diffCol << " diffRow is " << diffRow << std::endl;
#endif
  //float diffCol = precise_col - myCluster.first.first; 
  //float diffRow = precise_row - myCluster.first.second;

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
      //#ifndef __SYNTHESIS__
      //std::cout << "Cluster index  " << index << " has constituent (" << clusterConstituents[j][k].first << ", " << clusterConstituents[j][k].second << ")" << std::endl;
      //#endif
      clusterConstituents[j][k] = clusterConstituents[j + 1][k];
    }
    if (clusterPair == pairInitilizer)
    {
      //#ifndef __SYNTHESIS__
      //std::cout << "Breaking inside writeCluster" << std::endl;
      //#endif
      break; //We're not breaking here anymore? We enter an infinite loop on the last cluster, maybe a bad index?
    }
  }
}

void cluster_algo(hit_t source[hitBufferSize], cluster_t sink[clusBufferSize])
{
      //#ifndef __SYNTHESIS__
      //std::cout << "Entered " << __func__ << std::endl;
      //#endif
  int write_index = 0;
  //ap_uint<3> write_index = 0;
  cluster_t constructedCluster; //Can we pass the whole sink to writeCluster?
  ClusterAlgoBegin_Loop:
  for (unsigned int k = 0; k < hitBufferSize; k++) 
  {
    #pragma HLS loop_flatten off
    ClusterDepth_Loop:
    for (unsigned int i = 0; i < clusterDepth; i++)
    {
      hit_t theFirstPixelPair = clusterConstituents[i][0];
      if (theFirstPixelPair != pairInitilizer) //We have a cluster here, lets check if this hit belongs to it                                                        
      {
        bool addPairToCluster = false;
        int deltaColumn = 0; //Do we want a signed 2-bit int here? Is the first bit not the sign?
        int deltaRow = 0;
        //ap_int<11> deltaColumn = 0; //Do we want a signed 2-bit int here? Is the first bit not the sign?
        //ap_int<10> deltaRow = 0;
  
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
//#ifndef __SYNTHESIS__
//std::cout << "New pixel comparison beginning" << std::endl;
//std::cout << "First pixel (column, row) = (" << source[k].first << ", " << source[k].second << ")" << std::endl;
//std::cout << "Second pixel (column, row) = (" << clusterPair.first << ", " << clusterPair.second << ")" << std::endl;
//std::cout << "(delta column, delta row) = (" << deltaColumn << ", " << deltaRow << ")" << std::endl;
//std::string rangeCheck = adjacentPixel ? "These pixels are adjacent and should be added" : "These pixels are too far apart and should not be combined";
//std::cout << rangeCheck << std::endl;
//#endif
      	  if (adjacentPixel)
      	  {
      	    addPairToCluster = true;
      	    break;
      	  }

        }
      	      
        if(addPairToCluster)
        {
          int nConstituents = 0;
          //ap_int<5> nConstituents = 0;
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

//   while(clusterConstituents[0][0] != pairInitilizer)
//   {
//#ifndef __SYNTHESIS_
//     std::cout << "Writing cluster with write index " << write_index << std::endl; 
//#endif
//     writeCluster(0, constructedCluster);
//     sink[write_index] = constructedCluster;
//     ++write_index;
//   }
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
      //#ifndef __SYNTHESIS__
      //std::cout << "Entered " << __func__ << std::endl;
      //#endif
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

