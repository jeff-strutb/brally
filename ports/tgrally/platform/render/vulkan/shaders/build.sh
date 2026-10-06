#!/bin/sh
# Compile the RDP shaders to SPIR-V and write them into ../rdr_vk_spv.h,
# which is checked in: building the game needs no shader compiler. Run after
# editing a shader here (glslangValidator, from glslang or the Vulkan SDK).
set -e
cd "$(dirname "$0")"
for s in rcp.vert rcp.frag blit.vert blit.frag; do
  n=k_$(echo $s | tr . _)
  glslangValidator -V --target-env vulkan1.0 --vn $n -o $s.h $s >/dev/null
done
{
  echo "/* rdr_vk_spv.h: SPIR-V of shaders/rcp.* and blit.*, written by"
  echo " * shaders/build.sh: do not edit. */"
  for s in rcp.vert rcp.frag blit.vert blit.frag; do
    sed -n '/^const uint32_t/,$p' $s.h | sed 's/^const/static const/'
  done
} > ../rdr_vk_spv.h
rm -f *.h
spirv-val --version >/dev/null 2>&1 || exit 0
