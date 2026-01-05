// Water shader varying definitions

vec4 v_worldPos     : TEXCOORD0; // xyz = world position, w = view depth
vec4 v_screenPos    : TEXCOORD1; // xy = screen UV, zw = clip pos
vec3 v_normal       : NORMAL;
vec3 v_tangent      : TANGENT;
vec3 v_bitangent    : BINORMAL;
vec2 v_texcoord0    : TEXCOORD2; // Base UV
vec2 v_texcoord1    : TEXCOORD3; // Scrolling UV 1
vec2 v_texcoord2    : TEXCOORD4; // Scrolling UV 2
vec4 v_foamUV       : TEXCOORD5; // xy = foam UV, zw = caustics UV

vec3 a_position     : POSITION;
vec3 a_normal       : NORMAL;
vec2 a_texcoord0    : TEXCOORD0;
vec4 a_tangent      : TANGENT;
