from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.build import check_min_cppstd


class CordobaConan(ConanFile):
    name = "cordoba"
    license = "CC0-1.0"
    author = "perplexedpigmy"
    url = "https://github.com/perplexedpigmy/cordoba"
    description = "Lightweight C++ library for managing versioned NoSQL documents powered by libgit2"
    topics = ("git", "database", "nosql", "version-control", "crud")

    # Read version from VERSION file
    with open("VERSION", "r") as f:
        version = f.read().strip()

    package_type = "static-library"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
    }

    exports_sources = "VERSION", "CMakeLists.txt", "cmake/*", "gd/*", "examples/*"

    def validate(self):
        check_min_cppstd(self, 23)

    def config_options(self):
        if self.settings.get_safe("os") == "Windows":
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("libgit2/1.4.3")
        self.requires("spdlog/1.15.0")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.components["gd"].libs = ["gd"]
        self.cpp_info.components["gd"].requires = [
            "libgit2::git2",
            "spdlog::spdlog",
        ]

        self.cpp_info.set_property("cmake_file_name", "cordoba")
        self.cpp_info.set_property("cmake_target_name", "gd::gd")
        self.cpp_info.components["gd"].set_property("cmake_target_name", "gd::gd")
