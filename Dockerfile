FROM ubuntu:22.04

# Install dependencies from repositories
RUN apt update
RUN apt --yes upgrade
RUN apt install --yes cmake libboost-all-dev git nlohmann-json3-dev python3 clang libssl-dev libgmp3-dev libomp-dev


# emp-tool
WORKDIR /home
RUN git clone https://github.com/emp-toolkit/emp-tool.git
WORKDIR emp-tool
RUN git checkout 8052d95
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .
RUN make -j8
RUN make install
