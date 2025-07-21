# Uvula

This library is a standalone UV-unwrapper for potentially big meshes, that provides grouped and non-overlapping patches of projected faces on a texture. They can be used e.g. for painting on a mesh.

## Build

This project uses conan2 as dependency manager, so building it should be straightforward if you have it installed properly:

```bash
conan install . --build=missing
source build/Release/generators/conanbuild.sh
cmake --preset conan-release
cmake --build --preset conan-release
```

## Python binding

The python bindings are built by default, but can be ignore by adding `-o with_python_bindings=True` when doing the setup with `conan`
Once built, just make sure you have the library in the path and call the `unwrap` function:

```python
import pyUvula as uvula
vertices = numpy.ndarray((100, 3))
indices = numpy.ndarray((100, 3))
uvs, texture_width, texture_height = uvula.unwrap(vertices, indices)
```

The returned width and height are the recommended values for the texture. It is usually almost square, and one of the sides is 4096. The final texture size can be different because the UV coordinates are given in [0,1] range but the width/ratio should be kept.

## Command-line tester

A command-line tool is provided for the convenience of testing, and can be built by adding `-o with_cli=True` when doing the setup with `conan`. The the use is pretty simple:

```bash
./build/Release/cli/uvula /home/myself/dinosaur.stl -o /home/myself/dinosaur_unwrapped.obj
```

Then you can display the resulting OBJ file with e.g. Blender.
