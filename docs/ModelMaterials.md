# Model specular highlights

GB materials with the specular bit (`0x0008`) opt into a white directional Phong
highlight. The loader retains the stored exponent and packed frame colors, and
reads surface normals from all supported vertex layouts. Packed specular RGB is
control data in some compatible files; it is not used as a literal highlight
tint. A material exponent alone does not enable lighting.

The exponent is bounded to 1–1024 when the flag is enabled. Nonfinite and negative
exponents disable highlights. Version 7 has no stored exponent; flagged materials
use the compatibility default of 40. Material opacity, cutout and blend flags
remain independent of specular lighting.

Player equipment, monster and pet meshes, weapon thumbnails, the 3D item/object
viewer and the capture tool upload this optional surface metadata. Models without
the flag retain their existing texture-only rendering. Terrain, indoor lightmaps
and world-map mesh uploads do not opt into this path.

The OpenGL renderer adds a white Phong highlight to the existing textured RGB,
using a normalized world-space light direction of `(-0.35, 0.8, -0.5)`. It does not
replace the scene's base lighting or simulate refraction. Normals use the inverse
transpose of the model transform; bone skinning likewise transforms bind normals
and normalizes the weighted result. Singular transforms and invalid normals do
not produce highlights. Two-sided transparent glossy meshes retain stable
back-to-front triangle ordering, including their UVs and vertex normals.

Animated material color channels are retained, but are not newly interpreted as
runtime switches. This change supports the explicit stored specular flag and
exponent; it does not add a separate animated material controller to actors.

## Verification and provenance

The implementation is OpenKal code using standard Phong reflection and
matrix algebra. The public Khronos [GLSL specification, sections 8.5–8.6](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html)
was consulted on 2026-10-30 for reflection and matrix-operation semantics. No
external source, shader, fixture or asset was imported.

`GbMaterialTest.cpp` generates its own material data around the existing
project-owned triangle fixture. `MeshSpecularTest.cpp` generates quads, layered
triangles, normals and textures in memory. These fixtures are authored by OpenKal
contributors under the project's GPL-3.0-or-later license and require no game data.
The tests check loader fields, malformed values, scaled/skinned normals,
view-dependent highlights, exponent response, alpha preservation, transparent
ordering and isolation from unconfigured meshes.
