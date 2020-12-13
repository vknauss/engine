#version 330

in vec2 v_texCoords;

layout(location=0) out vec4 f_color;

const float PI = 3.141592653;

void main() {


    vec2 r = 0.5 - v_texCoords;
    float dist = sqrt(dot(r, r)) * 30;

    vec3 rainbow = 2 * pow(clamp(vec3(sin(dist + 2 * PI / 3) + 0.5, sin(dist + 4 * PI / 3) + 0.5, sin(dist) + 0.5), vec3(0.0), vec3(1.0)), vec3(2.2));



    f_color = vec4(mix(vec3(0.2, 0.8, 2.0), rainbow, clamp(-10*(r.y-0.1), 0.0, 1.0) * (1.0 - clamp(abs(100 - dist*dist)/(6*PI*PI), 0.0, 1.0))) , 1.0);
}
