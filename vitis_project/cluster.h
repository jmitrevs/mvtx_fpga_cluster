#ifndef __CLUSTER_H__
#define __CLUSTER_H__

#include <ap_int.h>
#include <utility>

#define nHits 22
#define nClusters 7
#define hitBufferSize 22
#define clusBufferSize 7
#define clusterDepth 2 // buffer size for the number of clusters
//clusterDepth cant be equal to number of clusters in our logic. We need a free memory point to add the new cluster to before the complete cluster is written out
#define maxPixelsInCluster 6 //

// here we define the new input
// format is 1 bit header, 19 bit bco (when header is 1)
// col/row (when header is 0)
typedef ap_uint<20> input_t; 
typedef std::pair<ap_uint<10>, ap_uint<9>> hit_t;
typedef std::pair<ap_uint<1>, ap_uint<1>> quad; //Errors if this typedef is quad_t, must be defined as a type somewhere else
typedef std::pair<hit_t, quad> cluster_t; //First pair is column and row, second is col quad, row quad

extern "C" {
  void cluster(input_t in[nHits], cluster_t out[nClusters]);
}
#endif // __CLUSTER_H__ not defined

