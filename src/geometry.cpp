#include "geometry.hpp"

void intersect_triangle(const pfloat3 &ray_origin,
                        const pfloat3 &ray_direction,
                        const pfloat3 &vertex_0,
                        const pfloat3 &vertex_1,
                        const pfloat3 &vertex_2,
                        Hit_info &hit_info)
{
    const pfloat epsilon {set1(1e-7f)};
    const pfloat zero {setzero()};
    const pfloat one {set1(1.0f)};

    const pfloat3 edge_1 {vertex_1 - vertex_0};
    const pfloat3 edge_2 {vertex_2 - vertex_0};
    const pfloat3 h {cross(ray_direction, edge_2)};
    const pfloat a {dot(edge_1, h)};

    const pmask is_not_parallel {(a < -epsilon) | (a > epsilon)};

    const pfloat f {one / a};
    const pfloat3 s {ray_origin - vertex_0};
    const pfloat u {f * dot(s, h)};
    const pfloat3 q {cross(s, edge_1)};
    const pfloat v {f * dot(ray_direction, q)};
    const pfloat t {f * dot(edge_2, q)};

    hit_info.intersected = is_not_parallel & (u >= zero) & (u <= one) &
                           (v >= zero) & (u + v <= one) & (t > epsilon);
    hit_info.t = select(t, hit_info.t, hit_info.intersected);
    hit_info.u = select(u, hit_info.u, hit_info.intersected);
    hit_info.v = select(v, hit_info.v, hit_info.intersected);
}
