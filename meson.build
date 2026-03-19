project('kgc_standalone', 'cpp', default_options: ['cpp_std=c++20'])

glslang_validator_exe = find_program('glslangValidator', required: true)

compile_shaders_vsmain_target = custom_target('compile shaders.vsmain',
    output: 'shaders.vsmain.tmp.bin',
    input: 'standalone/shaders.glsl',
    command: [
        glslang_validator_exe,
        '-e', 'vsmain',
        '-DGLSLANG_VERT',
        '--source-entrypoint', 'main',
        '-S', 'vert',
        '--target-env', 'vulkan1.2',
        '@INPUT@',
        '-o', '@OUTPUT@',
    ]
)

compile_shaders_fsmain_target = custom_target('compile shaders.fsmain',
    output: 'shaders.fsmain.tmp.bin',
    input: 'standalone/shaders.glsl',
    command: [
        glslang_validator_exe,
        '-e', 'fsmain',
        '-DGLSLANG_FRAG',
        '--source-entrypoint', 'main',
        '-S', 'frag',
        '--target-env', 'vulkan1.2',
        '@INPUT@',
        '-o', '@OUTPUT@',
    ]
)

spirv_link_exe = find_program('spirv-link', required: true)

link_shaders_target = custom_target('link shaders',
    depends: [
        compile_shaders_vsmain_target,
        compile_shaders_fsmain_target,
    ],
    output: 'shaders.bin',
    command: [
        spirv_link_exe,
        '--target-env', 'vulkan1.2',
        'shaders.vsmain.tmp.bin', 'shaders.fsmain.tmp.bin',
        '-o', '@OUTPUT',
    ]
)

sdl3_subproject = subproject('sdl3')
imgui_subproject = subproject('imgui')

vk = dependency('vulkan', version: '>=1.2.0', required: true)
sdl3 = dependency('sdl3', version: '>=3.0.0', required: true)
imgui = dependency('imgui', version: '>=1.92.5', required: true)

executable('standalone',
    sources: [
        'standalone/main.cpp',
        'standalone/kvk.cpp',
        'standalone/imnodes/imnodes.cpp',
        link_shaders_target,
    ],
    include_directories: [
        include_directories('include'),
    ],
    dependencies: [vk, sdl3, imgui],
    override_options: ['cpp_std=c++20'],
)
