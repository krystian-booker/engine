vec3 a_position   : POSITION;
vec2 a_texcoord0  : TEXCOORD0;
float a_texcoord1 : TEXCOORD1;  // height_factor

vec4 i_data0      : TEXCOORD7;  // position.xyz, rotation
vec4 i_data1      : TEXCOORD6;  // scale, bend, color_packed (2 floats), random

vec2 v_texcoord0  : TEXCOORD0;
vec4 v_color      : COLOR0;
vec3 v_worldPos   : TEXCOORD2;
float v_fade      : TEXCOORD3;
