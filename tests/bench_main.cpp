// Benchmark de consola (sin OpenGL) para medir el efecto real de OpenMP
// en tu maquina. No corre la app grafica, solo triangula una nube de
// puntos sintetica fija y mide el tiempo.
//
// Uso (PowerShell):
//   .\delaunay_benchmark.exe            -> usa todos los nucleos disponibles
//   $env:OMP_NUM_THREADS=1; .\delaunay_benchmark.exe   -> fuerza 1 nucleo (secuencial)
//
// Corre ambos y anota los tiempos en la tabla del informe.

#include "delaunay/Delaunay3D.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>

#ifdef _OPENMP
#include <omp.h>
#endif

static std::vector<Vec3> generatePoints(int n, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-50.0, 50.0);
    std::vector<Vec3> pts;
    pts.reserve(n);
    for (int i = 0; i < n; ++i) pts.emplace_back(dist(rng), dist(rng), dist(rng));
    return pts;
}

static void runCase(int numPoints) {
    auto points = generatePoints(numPoints, 1);

    auto t0 = std::chrono::steady_clock::now();
    auto tets = BowyerWatson3D::triangulate(points);
    auto t1 = std::chrono::steady_clock::now();
    double triangulateSec = std::chrono::duration<double>(t1 - t0).count();

    auto t2 = std::chrono::steady_clock::now();
    auto boundary = BowyerWatson3D::boundaryFaces(tets);
    auto t3 = std::chrono::steady_clock::now();
    double boundarySec = std::chrono::duration<double>(t3 - t2).count();

    std::cout << std::left << std::setw(10) << numPoints
              << std::setw(14) << tets.size()
              << std::setw(10) << boundary.size()
              << std::fixed << std::setprecision(4)
              << std::setw(18) << triangulateSec
              << boundarySec << "\n";
}

int main() {
#ifdef _OPENMP
    std::cout << "OpenMP: SI (compilado con soporte)\n";
    std::cout << "Nucleos maximos disponibles para este proceso: " << omp_get_max_threads() << "\n";
#else
    std::cout << "OpenMP: NO (compilado sin -fopenmp o find_package(OpenMP) no lo encontro)\n";
#endif
    std::cout << "\n";
    std::cout << std::left << std::setw(10) << "Puntos" << std::setw(14) << "Tetraedros"
              << std::setw(10) << "Caras" << std::setw(18) << "triangulate(s)" << "boundaryFaces(s)\n";
    std::cout << std::string(64, '-') << "\n";

    for (int n : {500, 1500, 3000, 5000,10000, 20000,30000,60000}) {
        runCase(n);
    }

    return 0;
}
