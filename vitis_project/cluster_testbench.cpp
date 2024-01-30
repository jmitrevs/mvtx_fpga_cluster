#include "cluster.h"
#include <fstream>
#include <utility>
#include <iostream>

int main()
{
  hit_t hits[nHits];
  hit_t clusters[nHits];
 
  int retval = 0, i, j;

  FILE *inFile, *outFile;

  inFile = fopen("in.dat", "r");

  for (unsigned int i = 0; i < nHits; ++i)
  {
    int col, row;
    fscanf(inFile, "%d %d", &col, &row);
    hits[i] = std::make_pair(col, row);
  }

  fclose(inFile);

  
  for (unsigned int i = 0; i < 2; ++i)
  {
    std::cout << "Loop iteration " << i << std::endl;

    for (unsigned int j = 0; j < nHits; j++)
      std::cout << "Printing hit " << j << ": (col, row) =  (" << hits[j].first << ", " << hits[j].second << ")" << std::endl;

    //send hit array to cluster top function
    cluster(hits, clusters);
  }

  outFile=fopen("out.dat","w");
  for (unsigned int i = 0;i < nHits; i++)
  {
    fprintf(outFile, "%d %d\n", clusters[i].first, clusters[i].second);
  }
  fclose(outFile);

  // Compare the results file with the golden results
  retval = system("diff --brief -w out.dat out.golden.dat");
  if (retval != 0)
  {
    printf("Test failed  !!!\n");
    retval=1;
  }
  else
  {
   printf("Test passed !\n");
  }
   return retval;
}

