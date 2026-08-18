#include <iostream>
#include <omp.h>

int main() {
    omp_set_num_threads(10);
    #pragma omp parallel for
    for(int i = 0; i < 10; i++) {
        std::cout << "Hello World... from thread = " << omp_get_thread_num() << std::endl;
    }
    return 0;
}

// clang++ -Wall -std=c++17 main.cpp -o main
// clang -Xclang -fopenmp -L/opt/homebrew/opt/libomp/lib -I/opt/homebrew/opt/libomp/include -lomp omptest.c -o omptest 

// WORKS: clang++ -Wall -std=c++17 -Xclang -fopenmp -L/opt/homebrew/opt/libomp/lib -I/opt/homebrew/opt/libomp/include main.cpp -o main 

// g++-15 -Wall -std=c++17 -fopenmp -L/opt/homebrew/opt/libomp/lib -I/opt/homebrew/opt/libomp/include main.cpp -o main 
// export OMP_NUM_THREADS=4

// clang++ main.cpp -o main -Xpreprocessor -fopenmp -lomp -L/opt/homebrew/opt/libomp/lib -I/opt/homebrew/opt/libomp/include
