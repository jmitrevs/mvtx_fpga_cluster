open_project -reset "proj_clusterizer"

set_top cluster_algo
add_files cluster.cpp
add_files -tb cluster_testbench.cpp
add_files -tb in_jakub.dat

open_solution -reset "solution"
set_part {xcku115-flvf1924-2-e}
create_clock -period 4.167 -name default

csim_design
