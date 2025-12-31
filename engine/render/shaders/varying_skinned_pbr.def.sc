// Varying definitions for skinned PBR shaders

// Varyings (interpolated between vertex and fragment shader)
vec4 v_worldPos     : TEXCOORD0;    // World-space position (xyz), w = linear depth
vec3 v_normal       : NORMAL;        // World-space normal
vec3 v_tangent      : TANGENT;       // World-space tangent
vec3 v_bitangent    : BINORMAL;      // World-space bitangent
vec2 v_texcoord0    : TEXCOORD1;     // Primary UV coordinates
vec4 v_color0       : COLOR0;        // Vertex color
vec4 v_clipPos      : TEXCOORD2;     // Clip-space position (for screen-space effects)
vec4 v_prevClipPos  : TEXCOORD3;     // Previous frame clip-space position (for TAA/motion blur)

// Vertex attributes (input to vertex shader)
vec3 a_position     : POSITION;
vec3 a_normal       : NORMAL;
vec2 a_texcoord0    : TEXCOORD0;
vec4 a_color0       : COLOR0;
vec3 a_tangent      : TANGENT;
ivec4 a_indices     : BLENDINDICES;  // Bone indices (4 influences)
vec4 a_weight       : BLENDWEIGHT;   // Bone weights (4 influences)
