/*
# Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: X11
*/

#include "cluster.h"

void cluster_algo(hit_t source[bufferSize], hit_t sink[bufferSize])
{

DCT_Outer_Loop:
   for (unsigned int k = 0; k < bufferSize; k++) 
   {
      sink[k] = source[k];
   }
}

void read_data(hit_t input[nHits], hit_t buf[bufferSize])
{
   int r;

RD_Loop_Row:
   for (r = 0; r < bufferSize; r++)
   {
     buf[r] = input[r];
   }
}

void write_data(hit_t buf[bufferSize], hit_t output[nHits])
{
   int r;

WR_Loop_Row:
   for (r = 0; r < bufferSize; r++)
   {
     output[r] = buf[r];
   }
}

void cluster(hit_t in[nHits], hit_t out[nHits])
{

   hit_t buf_in[bufferSize];
   hit_t buf_out[bufferSize];

   // Read input data. Fill the internal buffer.
   read_data(in, buf_in);

   cluster_algo(buf_in, buf_out);

   // Write out the results.
   write_data(buf_out, out);
}

