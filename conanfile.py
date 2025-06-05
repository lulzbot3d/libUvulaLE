# (c) 2025, UltiMaker -- see LICENCE for details


# TODO!: This needs to be in the standard UltiMaker 'do conan stuff' format (like using our own abstract base, etc.).


from conan import ConanFile
from conan.tools.cmake import cmake_layout

class LibUvulaRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("pybind11/2.13.6")

    #def build_requirements(self):
    #	self.tool_requires("cmake/>=3.8")

    def layout(self):
        cmake_layout(self)

    def package(self):
        copy(self, "*.pyd", src = os.path.join(self.build_folder, "libuvula"), dst = os.path.join(self.package_folder, "lib", "libuvula"), keep_path = False)
        packager = AutoPackager(self)
        packager.run()

    def package_info(self):
        if self.in_local_cache:
            self.runenv_info.append_path("PYTHONPATH", os.path.join(self.package_folder, "lib", "libuvula"))
        else:
            self.runenv_info.append_path("PYTHONPATH", os.path.join(self.build_folder, "libuvula"))
