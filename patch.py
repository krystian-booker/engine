import re

with open('samples/render_test/src/main.cpp', 'r') as f:
    content = f.read()

if 'set_ibl_intensity' not in content:
    content = content.replace('create_camera(world.get());', 'get_renderer()->set_ibl_intensity(1.0f);\n        create_camera(world.get());')

content = re.sub(r'config\.tonemap_config\.op = render::ToneMappingOperator::None;', 
                 'config.tonemap_config.op = render::ToneMappingOperator::ACES;', content)

if 'config.shadow_config.shadow_bias' not in content:
    content = content.replace('config.shadow_config.cascade_count = 4;', 
                              'config.shadow_config.cascade_count = 4;\n            config.shadow_config.shadow_bias = 0.005f;\n            config.shadow_config.normal_bias = 0.2f;')

with open('samples/render_test/src/main.cpp', 'w') as f:
    f.write(content)

with open('engine/render/src/shadow_system.cpp', 'r') as f:
    sc = f.read()
sc = sc.replace('TextureFormat::Depth16', 'TextureFormat::Depth32F')
sc = sc.replace('TextureFormat::Depth24', 'TextureFormat::Depth32F')
with open('engine/render/src/shadow_system.cpp', 'w') as f:
    f.write(sc)

print('Patch applied successfully')
