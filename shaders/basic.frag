#version 330 core
in vec3 vNormal;
in vec3 vFragPos;

uniform vec3 uColor;
uniform vec3 uViewPos;
uniform bool uFlatShading;

out vec4 FragColor;

void main() {
    vec3 lightPos = uViewPos + vec3(5.0, 10.0, 5.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    vec3 N = normalize(uFlatShading ? cross(dFdx(vFragPos), dFdy(vFragPos)) : vNormal);
    vec3 L = normalize(lightPos - vFragPos);
    vec3 V = normalize(uViewPos - vFragPos);
    vec3 R = reflect(-L, N);

    float ambient = 0.25;
    float diffuse = max(dot(N, L), 0.0);
    float specular = pow(max(dot(V, R), 0.0), 32.0) * 0.3;

    vec3 result = (ambient + diffuse) * uColor * lightColor + specular * lightColor;
    FragColor = vec4(result, 1.0);
}
