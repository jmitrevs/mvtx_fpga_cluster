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
#include <hls_stream.h>

hit_t pairInitilizer{0, 0}; //If this is  (0,0) then we miss any pixel in the corner. To use this pixel, set initialiser to (-1,-1) but you need to initialise all the array elements to -1 inside cluster algo so could be a memory issue
hit_t clusterConstituents[clusterDepth][maxPixelsInCluster];

bool isInRange(ap_int<2> min, ap_int<11> value, ap_int<2> max) //This may be an issue. Can we wrap the value around to reduce bits?
{
  return min <= value && value <= max;
}

void writeCluster(ap_uint<5> index, cluster_t& myCluster) //Can we handle moving clusters in global memory or do we need to pass the cluster list as an IO
{
  //Remember to weight from center of pixel, not the edge (add 0.5 to each pixel)
  //This should be right but the rows count from the top left of the chip (so row 0 is at +0.687cm while rown 511 is at -0.687cm)

  //float precise_col2 = 0;
  //float precise_row2 = 0;
  ap_ufixed<17,13> precise_col = 0; 
  ap_ufixed<16,12> precise_row = 0; 

  ap_uint<5> nConstituents = 0;
  writeCluster_findNumPixels:
  for (unsigned int i = 0; i < maxPixelsInCluster; ++i)
  {
    #pragma HLS PIPELINE II=5
    if (clusterConstituents[index][i] == pairInitilizer) break;
    ++nConstituents;
  }

  writeCluster_getPreciseCentroid:
  ap_ufixed<2,0> offset = 0.5;
  for (int i = 0; i < maxPixelsInCluster; i++)
  {
    //#pragma HLS unroll
    #pragma HLS PIPELINE II=5
    if (i == nConstituents) break; 
    //precise_col2 += (clusterConstituents[index][i].first + 0.5)/nConstituents;
    //precise_row2 += (clusterConstituents[index][i].second+0.5)/nConstituents;
    precise_col  += (clusterConstituents[index][i].first);//+offset)/ nConstituents;
    precise_row  += (clusterConstituents[index][i].second);//+offset)/nConstituents;
  }

//  ap_ufixed<1,0> offset = 0.5;
//#ifndef __SYNTHESIS__
//  std::cout << "offset is: " << offset << std::endl;
//#endif
//  //float offset = 0.5; 
//
  precise_col += (offset*nConstituents); 
  precise_row += (offset*nConstituents); 
 //
 precise_col = precise_col/nConstituents; 
 precise_row = precise_row/nConstituents; 

  writeCluster_makeClusterInfo:
  myCluster.first.first = hls::floor(precise_col); //Centroid column
  myCluster.first.second = hls::floor(precise_row); //Centroid row
  // better resource usage than conditional if statement 
  ap_ufixed<2,0> diffCol = precise_col - myCluster.first.first; 
  ap_ufixed<2,0> diffRow = precise_row - myCluster.first.second;
//#ifndef __SYNTHESIS__
//  std::cout << "precise col is " << precise_col << " precise_row is " << precise_row << std::endl;
  //std::cout << "precise col2 is " << precise_col2 << " precise_row2 is " << precise_row2 << std::endl;

//  std::cout << "diffCol is " << diffCol << " diffRow is " << diffRow << std::endl;
//#endif
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
	  writeCluster_clearPixels:
    for (unsigned int k = 0; k < maxPixelsInCluster; k++)
    {
      //#pragma HLS unroll
      //#ifndef __SYNTHESIS__
      //std::cout << "Cluster index  " << index << " has constituent (" << clusterConstituents[j][k].first << ", " << clusterConstituents[j][k].second << ")" << std::endl;
      //#endif
      clusterConstituents[j][k] = clusterConstituents[j + 1][k];
    }
    if (clusterConstituents[j][0] == pairInitilizer) break;
  }
}

void cluster_algo(hls::stream<hit_t> &source, hls::stream<cluster_t> &sink)
{
  cluster_t constructedCluster; //Can we pass the whole sink to writeCluster?
  ClusterAlgoBegin_Loop:
  //for (unsigned int k = 0; k < source.size(); k++) 
  while (!source.empty())
  {
    //#pragma HLS loop_flatten off 
    hit_t thisHit = source.read();
    ClusterDepth_Loop:
    for(unsigned int i = 0; i < clusterDepth; i++)
    {
      hit_t theFirstPixelPair = clusterConstituents[i][0];
      if (theFirstPixelPair != pairInitilizer) //We have a cluster here, lets check if this hit belongs to it                                                        
      {
        bool addPairToCluster = false;
        ap_int<11> deltaColumn = 0;
        ap_int<10> deltaRow = 0;
  
        for (unsigned int j = 0; j < maxPixelsInCluster; j++) // -1 to avoid running to the end of the array where we need a boundary or pair initializer
        {
          //#pragma HLS unroll 
      	  hit_t clusterPair = clusterConstituents[i][j]; 

      	  if (clusterPair == pairInitilizer) //we've found the empty array element, check and see if this cluster is complete
      	  {
      	    if (deltaColumn > 1) //Careful here! We dont write a cluster until the next loop iteration so we need an extra memory slot
      	    {
      	      writeCluster(i, constructedCluster);
              sink.write(constructedCluster);
      	      --i;
      	    }
      	    break;
      	  }
      	
          deltaColumn = thisHit.first - clusterPair.first; //If the difference between this hits column and the last column in the cluster is > 1, we've completed the cluster                                     
      	  deltaRow = thisHit.second - clusterPair.second;
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
            //#pragma HLS unroll
  
  	        hit_t pixelPair = clusterConstituents[i][n]; 
  	        if (pixelPair == pairInitilizer) break;
  	        ++nConstituents;
  	      }
  
          clusterConstituents[i][nConstituents] = thisHit;
          break; //Only to be used when we add a hit to a cluster                                       
        }
        else continue; //Use a continue to get to the next cluster array. Is this right?                
    
      }
      else
      {
        clusterConstituents[i][0] = thisHit;
        break;
      }
  	
    }
  } // end the for loop

   while(clusterConstituents[0][0] != pairInitilizer)
   {
     writeCluster(0, constructedCluster);
     sink.write(constructedCluster);
   }
}

void read_data(hit_t input[nHits], hls::stream<hit_t> &buf)
{
  RD_Loop_Row:
  for (int i = 0; i < hitBufferSize; i++)
  {
    buf << input[i];
  }
}
/*
void write_data(hls::stream<cluster_t> &buf, cluster_t output[nClusters])
{
  WR_Loop_Row:
  for (int i = 0; i < clusBufferSize; i++)
  while (!buf.empty())
  {
    buf << output[i] = ;
  }
}
*/
void cluster(hit_t in[nHits], cluster_t out[nClusters])
{
   hls::stream<hit_t> buf_in;
   hls::stream<cluster_t> buf_out;

   // Read input data. Fill the internal buffer.
   read_data(in, buf_in);
/*
  #ifndef __SYNTHESIS__
   hit_t streamed_hit = buf_in.read();
   int col = streamed_hit.first;
   int row = streamed_hit.second;
   std::cout << "Streamed hit: (col, row) = (" << col << ", " << row << ")" << std::endl;
  #endif
*/
   cluster_algo(buf_in, buf_out);

   // Write out the results
   //write_data(buf_out, out);
   int iVal = 0;
   while (!buf_out.empty())
   {
     out[iVal] = buf_out.read();
     iVal++;
   }

   #ifndef __SYNTHESIS__
   for (unsigned int i = 0; i < nClusters; i++)
     std::cout << "Cluster info: (col, row) = (" << out[i].first.first << ", " << out[i].first.second << "), (col quad, row quad) = (" << out[i].second.first << ", " << out[i].second.second << ")" << std::endl;
   #endif
/*
  #ifndef __SYNTHESIS__
   cluster_t streamed_clus = buf_out.read();
   int col = streamed_clus.first.first;
   int row = streamed_clus.first.second;
   int quad_col = streamed_clus.second.first;
   int quad_row = streamed_clus.second.second;
   std::cout << "Streamed clus: (col, row) = (" << col << ", " << row << "), (col quad, row quad) = (" << quad_col << ", " << quad_row << ")" << std::endl;
  #endif
*/
}

