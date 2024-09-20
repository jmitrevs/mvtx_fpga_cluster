#include "cluster.h"
#include <fstream>
#include <utility>
#include <iostream>
#include <string>
#include <algorithm>

void cluster(input_t in[nHits], output_t out[nClusters]);

int main()
{
  input_t inputHits[nHits];
  output_t clusters[nClusters];
 
  int retval = 0, i, j;

  FILE *inFile, *outFile;

  // input of the clusterizer will be the output of the decoder
  // take sample file from jakub and use this
  // output with normal formal is in.dat
  std::ifstream fin("in_jakub.dat");
  
  std::string iline;
  
  hls::stream<input_t> buf_in;
  hls::stream<output_t> buf_out;
#pragma HLS STREAM variable=buf_in depth=1024 // needed for cosimulation to work
#pragma HLS STREAM variable=buf_out depth=1024


  for (int iteration=0; std::getline(fin, iline); iteration++) {
    std::replace(iline.begin(), iline.end(), '/', ' ');
    std::stringstream ssin(iline);

    unsigned int headerBit = 0; 
    unsigned int colBit = 0; 
    unsigned int rowBit = 0;

    ssin >> std::hex >> headerBit >> colBit >> rowBit;

    //std::cout << std::hex << headerBit << " " << colBit << " " << rowBit << std::endl;

    input_t inputBit;
    inputBit[19] = headerBit; 
    if (headerBit) {
      inputBit(18, 0) = colBit;
    } else {
      inputBit(18,9) = colBit; 
      inputBit(8,0) = rowBit; 
    }

    buf_in.write(inputBit);
    cluster_algo(buf_in, buf_out);

    output_t result;
    if (buf_out.read_nb(result)) {
      std::cout << "Iteration " << std::dec << iteration << " received " << std::hex << result << std::endl;
    } else {
      std::cout << "Iteration " << std::dec << iteration << " no ouput " << std::endl;
    } 
  }
}
