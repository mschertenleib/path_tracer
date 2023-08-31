#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include "simd.hpp"
#include "vec3.hpp"

struct Hit_info
{
    pmask intersected;
    pfloat t;
    pfloat u;
    pfloat v;
};

void intersect_triangle(const pfloat3 &ray_origin,
                        const pfloat3 &ray_direction,
                        const pfloat3 &vertex_0,
                        const pfloat3 &vertex_1,
                        const pfloat3 &vertex_2,
                        Hit_info &hit_info);

#endif // GEOMETRY_HPP
