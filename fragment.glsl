#version 450 core

in vec2 texCoord;

uniform sampler2D image;

out vec4 finalColor;

void main () {
    vec2 flippedTexCoord = vec2(texCoord.x, 1.0 - texCoord.y);  // flip y coordinate to match OpenGL's texture coordinate system
    vec3 flippedColor = texture(image, flippedTexCoord).xyz;  // sample the texture at the flipped coordinate
//    finalColor = vec4(flippedColor, 1.0);  // set the output color
    finalColor = vec4(pow(vec3(clamp(flippedColor, 0, 1)), vec3(0.45)), 1.0);  // gamma correction
}

//#define Clamp(A, c, B) clamp(c, A, B)
//
//float LinearTosRGB(in float value) {
//    value = Clamp(0.0f, value, 1.0f);
//
//    float result = value * 12.92f;
//    if (value >= 0.0031308f) {
//        result = (1.055f * pow(value, 1.0f / 2.4f)) - 0.055f;
//    }
//
//    return result;
//}
//
//vec3 LinearVectorTosRGBVector(in vec3 value) {
//    vec3 result;
//
//    result.x = LinearTosRGB(value.x);
//    result.y = LinearTosRGB(value.y);
//    result.z = LinearTosRGB(value.z);
//
//    return result;
//}

//vec3 LinearToInverseGamma(vec3 rgb, float gamma)
//{
//    return mix(pow(rgb, vec3(1.0 / gamma)) * 1.055 - 0.055, rgb * 12.92, vec3(lessThan(rgb, 0.0031308.xxx)));
//}
//
//// ACES tone mapping curve fit to go from HDR to LDR
//// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
//vec3 ACESFilm(vec3 x)
//{
//    float a = 2.51;
//    float b = 0.03;
//    float c = 2.43;
//    float d = 0.59;
//    float e = 0.14;
//    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
//}
//
//void main()
//{
//    vec3 color = texture(image, texCoord).xyz;
//
//    color = ACESFilm(color);
//    color = LinearToInverseGamma(color, 2.4);
//    finalColor = vec4(color, 1.0);
//}
