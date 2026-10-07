# intel-compiler-container

docker build .

# Copy the sha256 from the build, or use tags or something...
docker run --volume ./mkl-conv:/src -it ff3be7e4ca7b00930b247fa34a070fce89c28258de45aaac46361ec0d69a3da1  /bin/bash

inside the container run
make
./run.x

# ./run.x NUM_RUNS START STEP STOP SIZE_W STRIDE_X STRIDE_Y STRIDE_W
 ./run.x  1000 16 16 2048 3 2 2 2
