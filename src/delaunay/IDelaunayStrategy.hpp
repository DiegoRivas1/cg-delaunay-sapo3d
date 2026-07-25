#pragma once
#include "Delaunay3D.hpp"

// Permite intercambiar la implementacion de triangulacion sin tocar
// el codigo de la app (ImGui podria elegir entre estrategias si en
// el futuro se agrega, por ej., una variante incremental distinta
// o una version paralelizada).
class IDelaunayStrategy {
public:
    virtual ~IDelaunayStrategy() = default;
    virtual std::vector<Tetrahedron> triangulate(const std::vector<Vec3>& points) = 0;
    virtual const char* name() const = 0;
};

class BowyerWatsonStrategy : public IDelaunayStrategy {
public:
    std::vector<Tetrahedron> triangulate(const std::vector<Vec3>& points) override {
        return BowyerWatson3D::triangulate(points);
    }
    const char* name() const override { return "Bowyer-Watson"; }
};
