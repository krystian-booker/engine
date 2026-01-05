vec3 a_position     : POSITION;
vec3 a_normal       : NORMAL;
vec2 a_texcoord0    : TEXCOORD0;
vec4 a_tangent      : TANGENT;

vec4 v_worldPos     : TEXCOORD0;
vec3 v_normal       : NORMAL;
vec3 v_tangent      : TANGENT;
vec3 v_bitangent    : BINORMAL;
vec2 v_texcoord0    : TEXCOORD1;  // Global terrain UV (0-1)
vec2 v_tileUV       : TEXCOORD2;  // Tiled UV for detail textures
vec4 v_clipPos      : TEXCOORD3;
