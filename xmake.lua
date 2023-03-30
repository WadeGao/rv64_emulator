set_languages("cxx17")
add_rules("mode.debug", "mode.release", "mode.minsizerel", "mode.releasedbg", "mode.asan")

add_cxxflags("-fno-rtti", "-fno-exceptions")
add_cxxflags("-fdata-sections", "-ffunction-sections")
add_ldflags("-Wl,--print-gc-sections,--gc-sections")

add_includedirs("include")
add_includedirs("third_party")

target("rv64_emulator")
    if is_mode("debug") then
        add_cxxflags("-g")
        add_defines("DEBUG")
    end
    set_kind("binary")
    set_targetdir("build")
    add_files("src/**.cc" )
    add_defines("FMT_HEADER_ONLY")

target("gtest")
    set_kind("shared")
    set_targetdir("build")
    add_includedirs("third_party/gtest")
    add_includedirs("third_party/gtest/include")
    add_files("third_party/gtest/src/gtest-all.cc")

target("gtest_main")
    set_kind("shared")
    set_targetdir("build")
    add_includedirs("third_party/gtest")
    add_includedirs("third_party/gtest/include")
    add_files("third_party/gtest/src/gtest_main.cc")
    add_deps("gtest")

target("gmock")
    set_kind("shared")
    set_targetdir("build")
    add_includedirs("third_party/gtest")
    add_includedirs("third_party/gtest/include")
    add_files("third_party/gtest/src/gmock-all.cc")
    add_deps("gtest")

target("gmock_main")
    set_kind("shared")
    set_targetdir("build")
    add_includedirs("third_party/gtest")
    add_includedirs("third_party/gtest/include")
    add_files("third_party/gtest/src/gmock_main.cc")
    add_deps("gmock")

target("fmt")
    set_kind("shared")
    set_targetdir("build")
    add_defines("FMT_EXPORT", "FMT_SHARED")
    add_files("third_party/fmt/src/format.cc")

target("unit_test")
    if is_mode("debug") then
        add_cxxflags("-g")
        add_defines("DEBUG")
    end
    if is_mode("coverage") then
        add_cflags("-fprofile-arcs -ftest-coverage")
        add_cxxflags("-fprofile-arcs -ftest-coverage")
        add_ldflags("-fprofile-arcs -ftest-coverage")
        after_build(function (target) 
            os.run("./scripts/gen_coverage_report.sh")
        end)
    end
    add_cxxflags("-fno-access-control")
    set_kind("binary")
    set_targetdir("build")
    add_includedirs("third_party/gtest/include")
    add_files("test/**.cc")
    add_files("src/**.cc|rv64_emulator.cc")
    add_deps("gtest")
    add_defines("FMT_HEADER_ONLY")
