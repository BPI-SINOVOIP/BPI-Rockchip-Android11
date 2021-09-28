#extension GL_OES_EGL_image_external : require
precision mediump float;

uniform vec3                uResolution;
uniform sampler2D           sTexture;
uniform float               uRadius;
varying vec2                vTextureCoord;

float computeWeight(float x) {
    return -((x * x) / (uRadius * uRadius)) + 1.0;
}

/**
* This shader sets the colour of each fragment to be the weighted average of the uRadius fragments
* vertically adjacent to it to produce a blur effect along the y-axis of the texture
*
* The weights come from the following curve: f(x) = 1 - (x^2)/(r^2)
* where r is the radius of the blur
*/
void main() {
    vec4 sampledColor = vec4(0.0);
    vec4 weightedColor = vec4(0.0);

    float divisor = 0.0;
    float weight = 0.0;

    for (float y = -uRadius; y <= uRadius; y++)
    {
        sampledColor = texture2D(sTexture, vTextureCoord + vec2(0.0, y / uResolution.y));
        weight = computeWeight(y);
        weightedColor += sampledColor * weight;
        divisor += weight;
    }

    gl_FragColor = vec4(weightedColor.r / divisor, weightedColor.g / divisor, weightedColor.b / divisor, 1.0);
}
