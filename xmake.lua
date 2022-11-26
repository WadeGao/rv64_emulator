set_languages("cxx17")
add_rules("mode.debug", "mode.release", "mode.minsizerel", "mode.asan")

target("rv64_emulator")
    add_cxxflags("-fno-rtti", "-fno-exceptions")
    add_cxxflags("-ffunction-sections -fdata-sections")
    if is_mode("debug") then
        add_defines("DEBUG")
    end
    set_kind("binary")
    set_targetdir("build")
    add_includedirs(".")
    add_files("src/**.cc" )

target("gtest")
    add_cxxflags("-fno-rtti", "-fno-exceptions")
    add_cxxflags("-ffunction-sections -fdata-sections")
    set_kind("static")
    set_targetdir("build")
    add_includedirs("third_party/gtest")
    add_includedirs("third_party/gtest/include")
    add_files("third_party/gtest/src/gtest-all.cc")

target("gtest_main")
    add_cxxflags("-fno-rtti", "-fno-exceptions")
    add_cxxflags("-ffunction-sections -fdata-sections")
    set_kind("static")
    set_targetdir("build")
    add_includedirs("third_party/gtest")
    add_includedirs("third_party/gtest/include")
    add_files("third_party/gtest/src/gtest_main.cc")

target("gmock")
    add_cxxflags("-fno-rtti", "-fno-exceptions")
    add_cxxflags("-ffunction-sections -fdata-sections")
    set_kind("static")
    set_targetdir("build")
    add_deps("gtest_main")
    add_includedirs("third_party/gtest")
    add_includedirs("third_party/gtest/include")
    add_files("third_party/gtest/src/gmock-all.cc")

target("unit_test")
    add_cxxflags("-fno-access-control")
    add_cxxflags("-fno-rtti", "-fno-exceptions")
    add_cxxflags("-ffunction-sections -fdata-sections")
    set_kind("binary")
    set_targetdir("build")
    add_includedirs(".")
    add_includedirs("third_party/gtest/include")
    add_files("test/**.cc")
    add_files("src/**.cc|rv64_emulator.cc")
    add_deps("gtest")
