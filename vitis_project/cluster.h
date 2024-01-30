#ifndef __CLUSTER_H__
#define __CLUSTER_H__

#include <utility>

#define nHits 22
#define bufferSize 22

typedef std::pair<int, int> hit_t;

extern "C" {
  void cluster(hit_t in[nHits], hit_t out[nHits]);
}
#endif // __CLUSTER_H__ not defined

