#include "cluster.h"
#include <fstream>
#include <utility>
#include <iostream>

int main()
{
  input_t inputHits[nHits];
  cluster_t clusters[nClusters];
 
  int retval = 0, i, j;

  FILE *inFile, *outFile;

  // input of the clusterizer will be the output of the decoder
  // take sample file from jakub and use this
  // output with normal formal is in.dat
  inFile = fopen("in_jakub_singleEvent.dat", "r");
  
  for (unsigned int i = 0; i < nHits; ++i)
  {
    // input bit that we will 
    ap_int<20> inputBit; 
    // declare the constituent bits 
    unsigned int colBit; 
    unsigned int rowBit;
    unsigned int headerBit; 

    fscanf(inFile, "%x %x/%x", &headerBit, &colBit, &rowBit);
    inputBit[19] = headerBit; 
    inputBit.range(18,9) = colBit; 
    inputBit.range(8,0) = rowBit; 
    inputHits[i] = inputBit; 
  }

  fclose(inFile);

  for (unsigned int i = 0; i < 1; ++i)
  {
    std::cout << "Loop iteration " << i << std::endl;

    for (unsigned int j = 0; j < nHits; j++)
      std::cout << "Printing hit "  << j << ":   ("   << inputHits[j] << ")" << std::endl;

    //send hit array to cluster top function
    cluster(inputHits, clusters);
  }


  outFile=fopen("out.dat","w");

  for (unsigned int i = 0; i < nClusters; i++){
    fprintf(outFile, "%d %d %d %d\n", clusters[i].first.first, clusters[i].first.second, clusters[i].second.first, clusters[i].second.second);
    std::cout  << clusters[i].first.first << " " << clusters[i].first.second << " " << clusters[i].second.first << " " << clusters[i].second.first << std::endl;
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

}

