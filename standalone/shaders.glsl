#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_gpu_shader_int64 : enable

#ifdef GLSLANG_VERT
#define vsmain() main()
#elif defined(GLSLANG_FRAG)
#define fsmain() main()
#endif

#ifdef GLSLANG_VERT
void vsmain() {

}
#endif

#ifdef GLSLANG_FRAG
void fsmain() {

}
#endif
