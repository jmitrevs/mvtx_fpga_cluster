#ifndef __CLUSTER_H__
#define __CLUSTER_H__

#include <utility>

#define nHits 22
#define nClusters 7
#define hitBufferSize 22
#define clusBufferSize 7
#define clusterDepth 2 // buffer size for the number of clusters
//clusterDepth cant be equal to number of clusters in our logic. We need a free memory point to add the new cluster to before the complete cluster is written out
#define maxPixelsInCluster 6 //

typedef std::pair<int, int> hit_t;
typedef std::pair<hit_t, hit_t> cluster_t; //First pair is column and row, second is col quad, row quad

extern "C" {
  void cluster(hit_t in[nHits], cluster_t out[nClusters]);
}
#endif // __CLUSTER_H__ not defined

