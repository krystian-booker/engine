vec3 a_position     : POSITION;
vec3 a_normal       : NORMAL;
vec2 a_texcoord0    : TEXCOORD0;
vec4 a_color0       : COLOR0;
vec3 a_tangent      : TANGENT;

// Instance data: 4x4 transform matrix (row-major)
vec4 i_data0        : TEXCOORD7;  // Row 0
vec4 i_data1        : TEXCOORD6;  // Row 1
vec4 i_data2        : TEXCOORD5;  // Row 2
vec4 i_data3        : TEXCOORD4;  // Row 3

vec4 v_worldPos     : TEXCOORD0;
vec3 v_normal       : NORMAL;
vec3 v_tangent      : TANGENT;
vec3 v_bitangent    : BINORMAL;
vec2 v_texcoord0    : TEXCOORD1;
vec4 v_color0       : COLOR0;
vec4 v_clipPos      : TEXCOORD2;
