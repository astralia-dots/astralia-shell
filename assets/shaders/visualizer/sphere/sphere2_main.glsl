uniform sampler2D tex;
void main()
{
    defaultAudioValues();
    defaultBaseFormValues();
    defaultParticleValues();
    defaultFractalFieldValues();
    defaultSphereValues();
    init();
    setAudio();
    setProps();
    // ES 3.2 read + reset the atomic accumulator here (imageAtomicExchange).
    // On ES 2.0 the accumulator is a normal texture (sphere1_splat.frag) that
    // additive blending already summed per-pixel; the "reset" is just next
    // frame's glClear of that pass's FBO (SphereVisualizer::render).
    // colorTracking is always 0 in this build (see common.glsl), so the old
    // getTrackedColors() per-channel atomic read path is dead code and
    // dropped rather than ported.
    float actualDepth = texture(tex, gl_FragCoord.xy / resolution.xy).r;
    // The shell-evacuation math converges a large share of the particle disc
    // onto the ring, so raw actualDepth peaks in the hundreds (measured ~415
    // at this canvas size). ncs's own working ES2.0 port relies on an 8-bit
    // UNORM accumulator clamping this to <=1.0 as an implicit ceiling on this
    // uncapped pow()/saturation curve; ours uses a float accumulator (see
    // SphereVisualizer::ensure_targets) for headroom against that same
    // clamp's dimming, so the ceiling has to be explicit here instead.
    actualDepth = min(actualDepth, 20.0);
    vec4 noiseCoords = vec4(1, 1, 1, 0);
    modifyNoiseCoordinates(noiseCoords);
    fractalField.noise = vec3(1);
    setPropsWithNoise();
    modifySphericalDisplacement();
    FragColor = step(0.0, actualDepth) * vec4(particle.color.xyz * particle.color.w, particle.color.w);
    FragColor *= (pow(actualDepth, particle.colorIntensityAddStrength)) * (1.0 - pow(1.0 - particle.color.w, actualDepth));
}
