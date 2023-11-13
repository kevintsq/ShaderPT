#version 460

layout (local_size_x = 16, local_size_y = 16) in ;  // 16x16 workgroup size

struct Ray {
    vec3 origin;
    vec3 direction;
};  // ray origin and direction

struct Sphere {
    vec4 geometry;
    vec3 emission;
    vec4 color;
};  // sphere geometry, emission and color

layout (std430, binding = 0) buffer b0 { Sphere spheres[]; };  // sphere buffer

layout (rgba32f, location = 0) readonly uniform image2D srcImage;
layout (rgba32f, location = 1) writeonly uniform image2D destImage;

uniform uvec2 imgSize;
uniform uint frameIndex;

const float PI = 3.1415926535897932384626433832795;
const float INF = 1e20;
const float EPS = 1e-4;
const int MAX_DEPTH = 64;
const int RUSSIAN_ROULETTE_THRESH = 5;
const float Nc = 1.000277;  // index of refraction of air
const float Nt = 1.5;  // index of refraction of glass
const float NtNc = Nt / Nc;  // ratio of indices of refraction
const float NcNt = Nc / Nt;  // ratio of indices of refraction
const float A = Nt - Nc;  // Schlick's approximation
const float B = Nt + Nc;  // Schlick's approximation
const float R0 = (A * A) / (B * B);  // Schlick's approximation

const float DIFFUSE = 0;
const float SPECULAR = 1;
const float REFRACTIVE = 2;

vec3 rand(uvec3 x) {  // random number generator
    for (int i = 0; i < 3; i++) {
        x = ((x >> 8U) ^ x.yzx) * 1103515245U;
    }
    return vec3(x) * (1.0 / float(0xffffffffU));
}

double intersect(Sphere sphere, Ray ray) {
    // solve t ^ 2 * d.d + 2 * t * (o - p).d + (o - p).(o - p) - R ^ 2 = 0
    dvec3 oc = dvec3(sphere.geometry.xyz) - ray.origin;  // oc = o - p, o is sphere center
    double b = dot(oc, ray.direction);  // 1/2 b from quadratic eq. setup
    double det = b * b - dot(oc, oc) + sphere.geometry.w * sphere.geometry.w;  // (b ^ 2 - 4ac) / 4: a = 1 because ray normalized
    if (det < 0) {  // no intersection, ray misses sphere
        return INF;
    } else {
        det = sqrt(det);  // sqrt((b ^ 2 - 4ac) / 4)
    }
    double d = b - det;  // d = t
    if (d > EPS) {  // ray hits sphere
        return d;  // return distance to sphere
    } else {  // ray hits sphere from inside
        d = b + det;  // d = t
        if (d > EPS) {  // ray hits sphere
            return d;  // return distance to sphere
        } else {  // ray misses sphere
            return INF;
        }
    }
}

void main() {  // main ray tracing function
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);  // coordel coordinates
    if (coord.x >= imgSize.x || coord.y >= imgSize.y) {
        return;
    }  // out of bounds

    Ray camera = Ray(vec3(0, 0.52, 7.4), normalize(vec3(0, -0.06, -1)));  // define camera look from and gaze direction
    // horizontal (x) camera direction, assumes upright camera
    vec3 cameraX = normalize(cross(camera.direction, abs(camera.direction.y) < 0.9 ? vec3(0, 1, 0) : vec3(0, 0, 1)));
    // vertical vector of the camera, cross product gets vector perpendicular to both cameraX and gaze direction
    vec3 cameraY = cross(cameraX, camera.direction);
    vec2 sdim = vec2(0.036, 0.024);  // screen dimensions

    vec2 random2 = 2 * rand(uvec3(coord, frameIndex)).xy;
    vec2 tent = vec2(random2.x < 1 ? sqrt(random2.x) - 1 : 1 - sqrt(2 - random2.x), random2.y < 1 ? sqrt(random2.y) - 1 : 1 - sqrt(2 - random2.y));  // tent filter sample
    vec2 screenCoord = ((coord + 0.5 * (0.5 + vec2((frameIndex / 2) % 2, frameIndex % 2) + tent)) / vec2(imgSize) - 0.5) * sdim;  // screen coordinates
    vec3 screenPosition = camera.origin + cameraX * screenCoord.x + cameraY * screenCoord.y;  // screen position
    vec3 lc = camera.origin + camera.direction * 0.035;
    vec3 accrad = vec3(0);  // accumulated radiance
    vec3 accmat = vec3(1);  // accumulated material
    Ray ray = Ray(lc, normalize(lc - screenPosition));  // construct ray
    int n_spheres = spheres.length();
    for (int depth = 0; depth < MAX_DEPTH; depth++) {  // loop over ray bounces (max depth = 64)
        double minDistance = INF;  // intersect ray with scene
        Sphere nearestObject;
        for (int i = 0; i < n_spheres; i++) {
            Sphere sphere = spheres[i];  // check each sphere, one at a time
            double d = intersect(sphere, ray);  // perform intersection test in double precision
            if (d < minDistance) {  // keep the closest intersection.
                minDistance = d;  // update minimum distance
                nearestObject = sphere;  // update nearest object
            }
        }
        if (minDistance < INF) {  // object hit
            accrad += accmat * nearestObject.emission;  // accumulate emission
            accmat *= nearestObject.color.xyz;  // accumulate material

            // Russian Roulette: Stop the recursion randomly based on the surface reflectivity.
            // Use the maximum component (r, g, b) of the surface color.
            // Don’t do Russian Roulette until after depth 5
            vec3 random = rand(uvec3(coord, frameIndex * 64 + depth));
            float p = max(max(nearestObject.color.x, nearestObject.color.y), nearestObject.color.z);  // max reflectance color
            if (depth > RUSSIAN_ROULETTE_THRESH) {
                if (random.z >= p) {
                    break;
                } else {
                    accmat /= p;  // scale accumulated material by reflectance
                }
            }

            vec3 hitPoint = ray.origin + ray.direction * float(minDistance);  // ray intersection point
            vec3 normal = normalize(hitPoint - nearestObject.geometry.xyz);  // sphere normal at intersection point
            // Properly oriented surface normal: When a ray hits a glass surface,
            // the ray tracer must determine if it is entering or exiting glass to compute the refraction ray.
            vec3 orientedNormal = dot(normal, ray.direction) < 0 ? normal : - normal;
            vec3 reflectionDirection = reflect(ray.direction, normal);  // reflection direction

            if (nearestObject.color.w == DIFFUSE) {
                float r1 = 2 * PI * random.x;  // random angle around the normal
                float r2 = random.y;  // random distance from the center of the normal
                float r2s = sqrt(r2);  // cosine sampling
                // Use normal to create orthonormal coordinate frame (w, u, v)
                vec3 w = normal;
                vec3 u = normalize((cross(abs(w.x) > 0.1 ? vec3(0, 1, 0): vec3(1, 0, 0), w)));  // u is perpendicular to the normal w
                vec3 v = cross(w, u);  // v is perpendicular to both w and u
                ray = Ray(hitPoint, normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1 - r2)));  // random reflection ray
            } else if (nearestObject.color.w == SPECULAR) {
                ray = Ray(hitPoint, reflectionDirection);  // reflect ray
            } else if (nearestObject.color.w == REFRACTIVE) {
                bool into = normal == orientedNormal;  // is ray entering or exiting glass?
                float nnt = into ? NcNt : NtNc;  // ratio of indices of refraction
                float ddn = dot(ray.direction, orientedNormal);  // cosine of angle between ray and normal
                float cos2t = 1 - nnt * nnt * (1 - ddn * ddn);  // cosine of angle between refracted ray and normal
                if (cos2t >= 0) {  // Fresnel reflection/refraction
                    vec3 refractionDirection = normalize(ray.direction * nnt - normal * ((into ? 1: -1) * (ddn * nnt + sqrt(cos2t))));  // refracted ray
                    float c = 1 - (into ? - ddn : dot(refractionDirection, normal));  // Schlick's approximation
                    float Re = R0 + (1 - R0) * c * c * c * c * c;  // reflectance
                    float Tr = 1 - Re;  // transmittance
                    float P = 0.25 + 0.5 * Re;  // probability of reflection
                    float RP = Re / P;  // reflection factor
                    float TP = Tr / (1 - P);  // transmission factor
                    ray = Ray(hitPoint, random.x < P ? reflectionDirection: refractionDirection);  // pick reflection with probability P
                    accmat *= random.x < P ? RP: TP;  // energy compensation
                } else {  // Total internal reflection
                    // Total internal reflection occurs when the light ray attempts to leave glass at too shallow an angle.
                    // If the angle is too shallow, all the light is reflected.
                    ray = Ray(hitPoint, reflectionDirection);
                }
            }
        }
    }
    vec3 prevColor = imageLoad(srcImage, coord).xyz;  // load previous color
    vec3 color = mix(prevColor, accrad, 1.0 / (frameIndex + 1));  // mix with previous color
    imageStore(destImage, coord, vec4(color, 1.0));  // write to image
}
