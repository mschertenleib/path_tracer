
const float pi = 3.1415926535897931;

struct Ray_payload
{
    vec3 color;
    vec3 emissivity;
    vec3 ray_origin;
    vec3 ray_direction;
    uint rng_state;
    bool hit_sky;
};

uint hash(uint x)
{
    x += x << 10;
    x ^= x >> 6;
    x += x << 3;
    x ^= x >> 11;
    x += x << 15;
    return x;
}

float random(inout uint rng_state)
{
    // Uniform distribution in [0.0, 1.0)
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return float(rng_state) / 4294967296.0;
}
