#!/bin/sh
# Compile the Glide shaders to SPIR-V and write them into glide_spv.h, which
# is checked in: building the game needs no shader compiler. Run after
# editing glide.vert, glide.frag or glide_u.glsl (glslangValidator, from
# glslang or the Vulkan SDK).
set -e
cd "$(dirname "$0")"
glslangValidator -V --vn k_glide_vert -o vert.h glide.vert >/dev/null
glslangValidator -V --vn k_glide_frag -o frag.h glide.frag >/dev/null
{
  echo "/* glide_spv.h: SPIR-V of shaders/glide.vert and glide.frag, written by"
  echo " * shaders/build.sh -- do not edit. */"
  sed -n '/^const uint32_t/,$p' vert.h | sed 's/^const/static const/'
  sed -n '/^const uint32_t/,$p' frag.h | sed 's/^const/static const/'
} > ../glide_spv.h
rm -f vert.h frag.h
echo "wrote glide_spv.h"
