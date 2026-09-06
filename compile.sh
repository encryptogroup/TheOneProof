
set -e

mkdir -p build
cd build

echo "Compiling version with OpenMP... ($(date +%H:%M:%S), container timezone might differ from host timezone)"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=ON -D VERIFY_MOD_2_256=OFF ..
make -j8 benchmarks
make -j8 tests
cp benchmarks/fliop benchmarks/fliop_omp
cp benchmarks/fliop_dotp benchmarks/fliop_dotp_omp
cp benchmarks/semi benchmarks/semi_omp
cp benchmarks/semi_dotp benchmarks/semi_dotp_omp
cp tests/fliop_test tests/fliop_test_omp
cp tests/fliop_dotp_test tests/fliop_dotp_test_omp
cp tests/semi_test tests/semi_test_omp
cp tests/semi_dotp_test tests/semi_dotp_test_omp

echo "Compiling version without OpenMP... ($(date +%H:%M:%S), container timezone might differ from host timezone)"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=OFF -D VERIFY_MOD_2_256=OFF ..
make -j8 benchmarks
make -j8 tests

cd ..

echo "Compiled! ($(date +%H:%M:%S), container timezone might differ from host timezone)"
