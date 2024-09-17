#ifndef __CLUSTER_H__
#define __CLUSTER_H__

#include <ap_int.h>
#include <utility>
#include <array>

#include "hls_stream.h"
#include "ap_int.h"

constexpr unsigned int nHits = 22;
constexpr unsigned int nClusters = 7;
constexpr unsigned int hitBufferSize = 22;
constexpr unsigned int clusBufferSize = 7;
constexpr unsigned int clusterDepth = 2; // buffer size for the number of clusters
//clusterDepth cant be equal to number of clusters in our logic. We need a free memory point to add the new cluster to before the complete cluster is written out
constexpr unsigned int maxPixelsInCluster = 6;

// here we define the new input
// format is 1 bit header, 19 bit bco (when header is 1)
// col/row (when header is 0)
typedef ap_uint<20> input_t;
typedef ap_uint<22> output_t;
typedef std::pair<ap_uint<10>, ap_uint<9>> hit_t;

using buffer_t = std::array<hit_t, maxPixelsInCluster>;
using buffers_t = std::array<buffer_t, clusterDepth>;

using bufferValid_t = ap_uint<maxPixelsInCluster>;
using buffersValid_t = std::array<bufferValid_t, clusterDepth>;

void cluster_algo(hls::stream<input_t> &source, hls::stream<output_t> &sink);
#endif // __CLUSTER_H__ not defined

