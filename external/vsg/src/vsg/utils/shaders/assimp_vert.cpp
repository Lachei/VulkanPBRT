#include <vsg/io/VSG.h>
static auto assimp_vert = []() {std::istringstream str(
R"(#vsga 0.4.3
Root id=1 vsg::ShaderStage
{
  userObjects 0
  stage 1
  entryPointName "main"
  module id=2 vsg::ShaderModule
  {
    userObjects 0
    hints id=0
    source "#version 450
#extension GL_ARB_separate_shader_objects : enable

#pragma import_defines (VSG_INSTANCE_POSITIONS, VSG_DISPLACEMENT_MAP)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelView;
} pc;

#ifdef VSG_DISPLACEMENT_MAP
layout(binding = 6) uniform sampler2D displacementMap;
#endif

layout(location = 0) in vec3 vsg_Vertex;
layout(location = 1) in vec3 vsg_Normal;
layout(location = 2) in vec2 vsg_TexCoord0;
layout(location = 3) in vec4 vsg_Color;

#ifdef VSG_INSTANCE_POSITIONS
layout(location = 4) in vec3 vsg_position;
#endif

layout(location = 0) out vec3 eyePos;
layout(location = 1) out vec3 normalDir;
layout(location = 2) out vec4 vertexColor;
layout(location = 3) out vec2 texCoord0;

layout(location = 5) out vec3 viewDir;

out gl_PerVertex{ vec4 gl_Position; };

void main()
{
    vec4 vertex = vec4(vsg_Vertex, 1.0);
    vec4 normal = vec4(vsg_Normal, 0.0);

#ifdef VSG_DISPLACEMENT_MAP
    // TODO need to pass as as uniform or per instance attributes
    vec3 scale = vec3(1.0, 1.0, 1.0);

    vertex.xyz = vertex.xyz + vsg_Normal * (texture(displacementMap, vsg_TexCoord0.st).s * scale.z);

    float s_delta = 0.01;
    float width = 0.0;

    float s_left = max(vsg_TexCoord0.s - s_delta, 0.0);
    float s_right = min(vsg_TexCoord0.s + s_delta, 1.0);
    float t_center = vsg_TexCoord0.t;
    float delta_left_right = (s_right - s_left) * scale.x;
    float dz_left_right = (texture(displacementMap, vec2(s_right, t_center)).s - texture(displacementMap, vec2(s_left, t_center)).s) * scale.z;

    // TODO need to handle different origins of displacementMap vs diffuseMap etc,
    float t_delta = s_delta;
    float t_bottom = max(vsg_TexCoord0.t - t_delta, 0.0);
    float t_top = min(vsg_TexCoord0.t + t_delta, 1.0);
    float s_center = vsg_TexCoord0.s;
    float delta_bottom_top = (t_top - t_bottom) * scale.y;
    float dz_bottom_top = (texture(displacementMap, vec2(s_center, t_top)).s - texture(displacementMap, vec2(s_center, t_bottom)).s) * scale.z;

    vec3 dx = normalize(vec3(delta_left_right, 0.0, dz_left_right));
    vec3 dy = normalize(vec3(0.0, delta_bottom_top, -dz_bottom_top));
    vec3 dz = normalize(cross(dx, dy));

    normal.xyz = normalize(dx * vsg_Normal.x + dy * vsg_Normal.y + dz * vsg_Normal.z);
#endif


#ifdef VSG_INSTANCE_POSITIONS
   vertex.xyz = vertex.xyz + vsg_position;
#endif

    gl_Position = (pc.projection * pc.modelView) * vertex;

    eyePos = (pc.modelView * vertex).xyz;

    vec4 lpos = /*vsg_LightSource.position*/ vec4(0.0, 0.0, 1.0, 0.0);
    viewDir = - (pc.modelView * vertex).xyz;
    normalDir = (pc.modelView * normal).xyz;

    vertexColor = vsg_Color;
    texCoord0 = vsg_TexCoord0;
}
"
    code 0
    
  }
  NumSpecializationConstants 0
}
)");
vsg::VSG io;
return io.read_cast<vsg::ShaderStage>(str);
};
