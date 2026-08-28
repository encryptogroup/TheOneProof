
set -e

mkdir build
cd build

echo "Compiling version with OpenMP... ($(date +%H:%M:%S), container timezone might differ from host timezone)"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=ON ..
make -j8 benchmarks
cp benchmarks/fliop benchmarks/fliop_omp
cp benchmarks/fliop_dotp benchmarks/fliop_dotp_omp
cp benchmarks/semi benchmarks/semi_omp
cp benchmarks/semi_dotp benchmarks/semi_dotp_omp

echo "Compiling version without OpenMP... ($(date +%H:%M:%S), container timezone might differ from host timezone)"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=OFF ..
make -j8 benchmarks

cd ..

echo "Compiled! ($(date +%H:%M:%S), container timezone might differ from host timezone)"
