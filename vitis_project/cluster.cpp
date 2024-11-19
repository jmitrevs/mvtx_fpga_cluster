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


bool isInRange(ap_int<2> min, ap_int<11> value, ap_int<2> max)
{
    return min <= value && value <= max;
}

void flushFirstCluster(buffers_t &clusterConstituents, buffersValid_t &clusterValids, hls::stream<output_t> &sink) {

    //Remember to weight from center of pixel, not the edge (add 0.5 to each pixel)
    //This should be right but the rows count from the top left of the chip (so row 0 is at +0.687cm while rown 511 is at -0.687cm)

    ap_ufixed<17,13> precise_col = 0;
    ap_ufixed<16,12> precise_row = 0;

    ap_uint<4> nConstituents = 0;

    const ap_ufixed<2,0> offset = 0.5;

    writeCluster_getPreciseCentroid:
    for (int i = 0; i < maxPixelsInCluster; i++) {
        #pragma HLS unroll
        if (clusterValids[0].test(i)) {
            ++nConstituents;
            precise_col  += static_cast<ap_uint<10>>(clusterConstituents[0][i](18, 9));
            precise_row  += static_cast<ap_uint<9>>(clusterConstituents[0][i](8, 0));
        }
    }

    if (nConstituents > 0) {
        // actually have a cluster
        precise_col += (offset*nConstituents);
        precise_row += (offset*nConstituents);

        precise_col = precise_col/nConstituents;
        precise_row = precise_row/nConstituents;

        ap_uint<10> col = precise_col; //Centroid column
        ap_uint<9> row = precise_row; //Centroid row

        // better resource usage than conditional if statement
        ap_ufixed<2,0> diffCol = precise_col - col;
        ap_ufixed<2,0> diffRow = precise_row - row;

        output_t theCluster = 0;  // note, bit 21 stays 0
        theCluster(20, 11) = col;
        theCluster(10, 2) = row;
        theCluster(25, 22) = nConstituents;

        if (diffRow >= offset) {
            theCluster.set(0);
        }
        if (diffCol >= offset) {
            theCluster.set(1);
        }

        //Now write the cluster
        sink.write(theCluster);

        // shift everything back one in the array
        writeCluster_clearCluster:
        for (unsigned int j = 1; j < clusterDepth; j++) {
            clusterConstituents[j-1] = clusterConstituents[j];
            clusterValids[j-1] = clusterValids[j];
        }
        clusterValids[clusterDepth-1] = 0;  // clear the valids
    }
}

// this flushes all the clusters
void flushClusters(buffers_t &clusterConstituents, buffersValid_t &clusterValids, hls::stream<output_t> &sink) {
    flush_clusters:
    for (unsigned i = 0; i < clusterDepth; i++) {
        #pragma HLS PIPELINE II=3
        flushFirstCluster(clusterConstituents, clusterValids, sink);
    }
}
void cluster_algo(hls::stream<input_t> &source, hls::stream<output_t> &sink)
{
    // read the input
    auto thisInput = source.read();

    // now handle the input
    ap_uint<1> headerBit = thisInput[19];
    ap_uint<10> colBit = thisInput.range(18,9);
    ap_uint<9> rowBit = thisInput.range(8,0);

    std::cout << " col " << colBit << " row " << rowBit << std::endl;

    static buffers_t clusterConstituents;
    static buffersValid_t clusterValids = {{0, 0}};  // Note this impies clusterDepth = 2
    #pragma HLS ARRAY_PARTITION variable=clusterConstituents complete
    #pragma HLS ARRAY_PARTITION variable=clusterValids complete

    if (headerBit) {
        flushClusters(clusterConstituents, clusterValids, sink);
        output_t theOutput = 0;
        theOutput(21, 2) = thisInput;
        sink.write(theOutput);
        return;
    }

    // header bit is not set, so this is a hit
    const hit_t thisHit = (colBit, rowBit);

    // iterate over possible clusters
    cluster_loop:
    for (unsigned int iclus = 0; iclus < clusterDepth; iclus++) {
#pragma HLS PIPELINE
        if (clusterValids[iclus] == 0) {
            // The cluster buffer is empty, so add the hit to start a cluster
            clusterConstituents[iclus][0] = thisHit;
            clusterValids[iclus].set(0);
            break;  // finished processing
        }

        // there is a cluster
        bool addCluster = false;

        pixel_loop:
        for (unsigned int pixel = 0; pixel < maxPixelsInCluster; pixel++) {
            // iterate over possible pixels
            if (!clusterValids[iclus].test(pixel)) {
                if (addCluster) {
                    // add this hit to the cluster
                    clusterConstituents[iclus][pixel] = thisHit;
                    clusterValids[iclus].set(pixel);
                }
                // Either already added above, or not part of cluster; continue to next iteration
                break;
            }

            auto deltaColumn = colBit - static_cast<ap_uint<10>>(clusterConstituents[0][pixel](18, 9));
            auto deltaRow = rowBit - static_cast<ap_uint<9>>(clusterConstituents[0][pixel](8, 0));

            addCluster = addCluster || (isInRange(0, deltaColumn, 1) && isInRange(-1, deltaRow, 1));
        }  // for pixel

        if (addCluster) {
            // already added (or hit maxPixelsInCluster), so done
            break;
        } else if (iclus == clusterDepth - 1) {
            // no more room to add a cluster, so flush first
            flushFirstCluster(clusterConstituents, clusterValids, sink);

            // now start a new cluster in the newly empty place
            clusterConstituents[iclus][0] = thisHit;
            clusterValids[iclus].set(0);
        }
    } // for cluster
}
