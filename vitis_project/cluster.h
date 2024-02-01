#ifndef __CLUSTER_H__
#define __CLUSTER_H__

#include <utility>

#define nHits 22
#define bufferSize 22
#define clusterDepth 1 // buffer size for the number of clusters
#define maxPixelsInCluster 5

typedef std::pair<int, int> hit_t;

extern "C" {
  void cluster(hit_t in[nHits], int &out);
}
#endif // __CLUSTER_H__ not defined

