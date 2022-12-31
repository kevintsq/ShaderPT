#version 450 core

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 textureCoord;

out vec2 texCoord;

void main() {
    // texCoord = vec2(textureCoord.s, 1.0 - textureCoord.t);  // Flip the texture vertically
    texCoord = textureCoord;
    gl_Position = vec4(position, 0.0, 1.0);
}