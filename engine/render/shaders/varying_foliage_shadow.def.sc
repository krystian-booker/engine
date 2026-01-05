vec3 a_position     : POSITION;
vec2 a_texcoord0    : TEXCOORD0;

// Instance data: 4x4 transform matrix (row-major)
vec4 i_data0        : TEXCOORD7;  // Row 0
vec4 i_data1        : TEXCOORD6;  // Row 1
vec4 i_data2        : TEXCOORD5;  // Row 2
vec4 i_data3        : TEXCOORD4;  // Row 3

vec2 v_texcoord0    : TEXCOORD0;
