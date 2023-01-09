set_languages("cxx17")
add_rules("mode.debug", "mode.release", "mode.minsizerel", "mode.releasedbg", "mode.asan")

add_cxxflags("-fno-rtti", "-fno-exceptions")
add_cxxflags("-fdata-sections", "-ffunction-sections")
add_ldflags("-Wl,--print-gc-sections,--gc-sections,--icf=safe")

target("rv64_emulator")
    if is_mode("debug") then
        add_cxxflags("-g")
        add_defines("DEBUG")
    end
    set_kind("binary")
    set_targetdir("build")
    add_includedirs(".")
    add_includedirs("third_party/fmt/include")
    add_files("src/**.cc" )
    add_deps("fmt")

target("gtest")
    set_kind("static")
    set_targetdir("build")
    add_includedirs("third_party/gtest")
    add_includedirs("third_party/gtest/include")
    add_files("third_party/gtest/src/gtest-all.cc")

target("gtest_main")
    set_kind("static")
    set_targetdir("build")
    add_includedirs("third_party/gtest")
    add_includedirs("third_party/gtest/include")
    add_files("third_party/gtest/src/gtest_main.cc")

target("gmock")
    set_kind("static")
    set_targetdir("build")
    add_includedirs("third_party/gtest")
    add_includedirs("third_party/gtest/include")
    add_files("third_party/gtest/src/gmock-all.cc")

target("fmt")
    set_kind("static")
    set_targetdir("build")
    add_includedirs("third_party/fmt/include")
    add_files("third_party/fmt/src/format.cc")

target("unit_test")
    if is_mode("debug") then
        add_cxxflags("-g")
        add_defines("DEBUG")
    end
    add_cxxflags("-fno-access-control")
    set_kind("binary")
    set_targetdir("build")
    add_includedirs(".")
    add_includedirs("third_party/fmt/include")
    add_includedirs("third_party/gtest/include")
    add_files("test/**.cc")
    add_files("src/**.cc|rv64_emulator.cc")
    add_deps("gtest")
    add_deps("fmt")
