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

bool isInRange(ap_int<2> min, ap_int<11> value, ap_int<2> max)
{
  return min <= value && value <= max;
}

void writeCluster(ap_uint<5> index, cluster_t& myCluster) //Can we handle moving clusters in global memory or do we need to pass the cluster list as an IO
{
  //Remember to weight from center of pixel, not the edge (add 0.5 to each pixel)
  //This should be right but the rows count from the top left of the chip (so row 0 is at +0.687cm while rown 511 is at -0.687cm)

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

  const ap_ufixed<2,0> offset = 0.5;
 writeCluster_getPreciseCentroid:
  for (int i = 0; i < maxPixelsInCluster; i++)
    {
      //#pragma HLS unroll
      if (i == nConstituents) break;
      precise_col  += (clusterConstituents[index][i].first);
      precise_row  += (clusterConstituents[index][i].second);
    }

  precise_col += (offset*nConstituents);
  precise_row += (offset*nConstituents);

  precise_col = precise_col/nConstituents;
  precise_row = precise_row/nConstituents;

 writeCluster_makeClusterInfo:
  myCluster.first.first = hls::floor(precise_col); //Centroid column
  myCluster.first.second = hls::floor(precise_row); //Centroid row

  // better resource usage than conditional if statement
  ap_ufixed<2,0> diffCol = precise_col - myCluster.first.first;
  ap_ufixed<2,0> diffRow = precise_row - myCluster.first.second;

  if(diffCol < offset){
    myCluster.second.first = 0;
    if (diffRow < offset) myCluster.second.second = 0;
    else myCluster.second.second = 1;
  }
  else{
    myCluster.second.first = 1;
    if (diffRow < offset) myCluster.second.second = 0;
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
#pragma HLS PIPELINE II=5
          //#pragma HLS unroll
          //#ifndef __SYNTHESIS__
          //std::cout << "Cluster index  " << index << " has constituent (" << clusterConstituents[j][k].first << ", " << clusterConstituents[j][k].second << ")" << std::endl;
          //#endif
          clusterConstituents[j][k] = clusterConstituents[j + 1][k];
        }
      if (clusterConstituents[j][0] == pairInitilizer) break;
    }
}

void cluster_algo(hls::stream<input_t> &source, hls::stream<cluster_t> &sink)
{

  cluster_t constructedCluster;
 ClusterAlgoBegin_Loop:
  while (!source.empty()) {
    //#pragma HLS loop_flatten off
    input_t thisInput = source.read();

    // now handle the input
    ap_uint<1> headerBit = thisInput[19];
    ap_uint<10> colBit = thisInput.range(18,9);
    ap_uint<9> rowBit = thisInput.range(8,0);

    std::cout << " col " << colBit << " row " << rowBit << std::endl;

    hit_t thisHit = {colBit, rowBit};
  ClusterDepth_Loop:
    for(unsigned int iClusDepth = 0; iClusDepth < clusterDepth; iClusDepth++) {  
      hit_t theFirstPixelPair = clusterConstituents[iClusDepth][0];
      if (theFirstPixelPair != pairInitilizer) { //We have a cluster here, lets check if this hit belongs to it
        bool addPairToCluster = false;
        ap_int<11> deltaColumn = 0;
        ap_int<10> deltaRow = 0;

        for (unsigned int jPixInCluster = 0; jPixInCluster < maxPixelsInCluster; jPixInCluster++) {// -1 to avoid running to the end of the array where we need a boundary or pair initializer
              
          //#pragma HLS unroll
          hit_t clusterPair = clusterConstituents[iClusDepth][jPixInCluster];

          if (clusterPair == pairInitilizer) {//we've found the empty array element, check and see if this cluster is complete
                  
            if (deltaColumn > 1) { //Careful here! We dont write a cluster until the next loop iteration so we need an extra memory slot
                      
              writeCluster(iClusDepth, constructedCluster);
              sink.write(constructedCluster);
              --iClusDepth;
            }
            break;  // go to next cluster depth
          }

          deltaColumn = thisHit.first - clusterPair.first; //If the difference between this hits column and the last column in the cluster is > 1, we've completed the cluster
          deltaRow = thisHit.second - clusterPair.second;
          bool adjacentPixel = isInRange(0, deltaColumn, 1) && isInRange(-1, deltaRow, 1);
          if (adjacentPixel) {
            addPairToCluster = true;
            break;
          }

        }  // end of for loop over maxPixelsInCluster

        if(addPairToCluster) {
          ap_int<5> nConstituents = 0;
          for (int kPixInCluster = 0; kPixInCluster < maxPixelsInCluster; kPixInCluster++) {
            //#pragma HLS unroll
            hit_t pixelPair = clusterConstituents[iClusDepth][kPixInCluster];
            if (pixelPair == pairInitilizer) break;
            ++nConstituents;
          }

          clusterConstituents[iClusDepth][nConstituents] = thisHit;
          break; //Only to be used when we add a hit to a cluster
        }
      } else {
        clusterConstituents[iClusDepth][0] = thisHit;
        break;  // go back to while loop, read in next hit
      }
    }  // end of for loop over clusterDepth
  } // end the while loop


  //Can maybe get rid of the memory flush as we stream now
  while(clusterConstituents[0][0] != pairInitilizer) {
    writeCluster(0, constructedCluster);
    sink.write(constructedCluster);
  }

}

void read_data(input_t input[nHits], hls::stream<input_t> &buf)
{
 RD_Loop_Row:
  for (int i = 0; i < hitBufferSize; i++)
    {
      buf << input[i];
    }
}

void write_data(hls::stream<cluster_t> &buf, cluster_t output[nClusters]){
  int iVal = 0;
  while (!buf.empty())
    {
      output[iVal] = buf.read();
      iVal++;
    }
}

void cluster(input_t in[nHits], cluster_t out[nClusters])
{

#pragma HLS DATAFLOW

  hls::stream<input_t> buf_in;
  hls::stream<cluster_t> buf_out;
#pragma HLS STREAM variable=buf_in depth=1024 // needed for cosimulation to work
#pragma HLS STREAM variable=buf_out depth=1024

  // Read input data. Fill the internal buffer.
  read_data(in, buf_in);

  cluster_algo(buf_in, buf_out);

  write_data(buf_out, out);

#ifndef __SYNTHESIS__
  for (unsigned int i = 0; i < nClusters; i++)
    std::cout << "Cluster info: (col, row) = (" << out[i].first.first << ", " << out[i].first.second << "), (col quad, row quad) = (" << out[i].second.first << ", " << out[i].second.second << ")" << std::endl;
#endif
}

