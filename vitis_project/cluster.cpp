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

void flushFirstCluster(ap_uint<4> chipBit, buffers_t &clusterConstituents, buffersValid_t &clusterValids, hls::stream<output_t> &sink) {

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
        theCluster(25, 22) = chipBit;
        theCluster(20, 11) = col;
        theCluster(9, 1) = row;
        theCluster(29, 26) = nConstituents;

        if (diffRow >= offset) {
            theCluster.set(0);
        }
        if (diffCol >= offset) {
            theCluster.set(10);
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
void flushClusters(ap_uint<4> chipBit, buffers_t &clusterConstituents, buffersValid_t &clusterValids, hls::stream<output_t> &sink) {
    flush_clusters:
    for (unsigned i = 0; i < clusterDepth; i++) {
        #pragma HLS PIPELINE II=3
        flushFirstCluster(chipBit, clusterConstituents, clusterValids, sink);
    }
}


void mergeClusters(buffers_t &clusterConstituents, buffersValid_t &clusterValids) {
    unsigned int nConstituents0 = 0;
    unsigned int nConstituents1 = 0;

    MergeConstituentsLoop:
    for (int i = 0; i < maxPixelsInCluster; i++) {
        if (clusterValids[0].test(i)) ++nConstituents0;
        if (clusterValids[1].test(i)) ++nConstituents1;
    }

    // remove duplicate (randomly from second)
    clusterValids[1].clear(nConstituents1 - 1);

    if (nConstituents0 + nConstituents1 <= maxPixelsInCluster) { // can try to optimize
        // merge
        MergeLoop:
        for (int i = 0; i < nConstituents1; i++) {
            clusterConstituents[0][nConstituents0 + i] = clusterConstituents[1][i];
            clusterValids[0].set(nConstituents0 + i);
        }
        clusterValids[1] = 0;
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
    ap_uint<4> chipBit = thisInput.range(23,20);

    static ap_uint<4> storedChip = 0;
    static buffers_t clusterConstituents;
    static buffersValid_t clusterValids = {{0, 0}};  // Note this impies clusterDepth = 2
    #pragma HLS ARRAY_PARTITION variable=clusterConstituents complete
    #pragma HLS ARRAY_PARTITION variable=clusterValids complete

    if (headerBit) {
        flushClusters(storedChip, clusterConstituents, clusterValids, sink);
        output_t theOutput = 0;
        theOutput(25, 2) = thisInput;
        sink.write(theOutput);
        return;
    }

    // header bit is not set, so this is a hit
    const hit_t thisHit = (colBit, rowBit);

    storedChip = chipBit;

    // iterate over possible clusters
    // Note:  try to add to all clusters, not just first. Merge if added to multiple

    std::array<bool, clusterDepth> addedHit = {{false, false}}; // assumes clusterDepth = 2

    cluster_loop:
    for (unsigned int iclus = 0; iclus < clusterDepth; iclus++) {
#pragma HLS PIPELINE
        if (clusterValids[iclus] == 0) {
            // double check that we haven't already added it
            // ASSUMES CLUSERDEPTH == 2 (though easy to fix:  use max(0, iclus-1) instead of 0)
            if (!addedHit[0]) {
                // The cluster buffer is empty, so add the hit to start a cluster
                clusterConstituents[iclus][0] = thisHit;
                clusterValids[iclus].set(0);
                addedHit[iclus] = true;
            }
            break;  // finished processing
        }

        // there is a cluster
        std::cout << "Cluster found, iclus = " << iclus << std::endl;;

        // Do we add the hit to the cluster
        bool addHit = false;

        pixel_loop:
        for (unsigned int pixel = 0; pixel < maxPixelsInCluster; pixel++) {
            // iterate over possible pixels
            if (!clusterValids[iclus].test(pixel)) {
                if (addHit) {
                    // add this hit to the cluster
                    clusterConstituents[iclus][pixel] = thisHit;
                    clusterValids[iclus].set(pixel);
                    addedHit[iclus] = true;
                }
                // Either already added above, or not part of cluster; continue to next iteration of iclus
                break;
            }

            auto deltaColumn = colBit - static_cast<ap_uint<10>>(clusterConstituents[iclus][pixel](18, 9));
            auto deltaRow = rowBit - static_cast<ap_uint<9>>(clusterConstituents[iclus][pixel](8, 0));

            addHit = addHit || (isInRange(-1, deltaColumn, 1) && isInRange(-1, deltaRow, 1));
            std::cout << "delCol = " << deltaColumn << ", deltaRow = " << deltaRow << ", addHit = " << addHit << std::endl;
        }  // for pixel
    } // for cluster

    // this assumes clusterDept == 2
    if (addedHit[0] && addedHit[1]) {
        // added it to both clusters, so merge clusters if possible
        mergeClusters(clusterConstituents, clusterValids);
    } else if (addedHit[0] || addedHit[1]) {
        // the usual case, all good, don't need to do anything specal
    } else {
        // no more room to add a cluster, so flush first
        flushFirstCluster(storedChip, clusterConstituents, clusterValids, sink);

        // now start a new cluster in the newly empty place
        clusterConstituents[clusterDepth - 1][0] = thisHit;
        clusterValids[clusterDepth - 1].set(0);
    }
}
