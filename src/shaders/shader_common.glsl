
const float pi = 3.1415926535897931;

struct Ray_payload
{
    vec3 color;
    vec3 emissivity;
    vec3 ray_origin;
    vec3 ray_direction;
    float reflectance_attenuation;
    uint rng_state;
    bool hit_sky;
};

float random(inout uint rng_state)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    // NOTE: due to rounding when converting to float,
    // this can still return 1.0.
    return float(rng_state) * (1.0 / 4294967296.0);
}
