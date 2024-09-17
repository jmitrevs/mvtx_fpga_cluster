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

