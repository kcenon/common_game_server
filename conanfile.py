from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake, cmake_layout


class CommonGameServerConan(ConanFile):
    name = "common_game_server"
    version = "0.1.0"
    description = "Unified Game Server Framework with ECS and Plugin Architecture"
    license = "BSD-3-Clause"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "build_tests": [True, False],
        "build_benchmarks": [True, False],
        "build_services": [True, False],
    }
    default_options = {
        "build_tests": True,
        "build_benchmarks": False,
        "build_services": False,
    }

    def requirements(self):
        self.requires("yaml-cpp/0.8.0")

    def build_requirements(self):
        if self.options.build_tests:
            self.test_requires("gtest/1.15.0")
        if self.options.build_benchmarks:
            self.test_requires("benchmark/1.9.1")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["CGS_BUILD_TESTS"] = bool(self.options.build_tests)
        tc.variables["CGS_BUILD_BENCHMARKS"] = bool(self.options.build_benchmarks)
        tc.variables["CGS_BUILD_SERVICES"] = bool(self.options.build_services)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if self.options.build_tests:
            cmake.test()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "cgs")
        self.cpp_info.set_property("cmake_target_name", "cgs::cgs_core")
        self.cpp_info.includedirs = ["include"]
