set_languages("cxx17")
add_rules("mode.debug", "mode.release", "mode.minsizerel", "mode.releasedbg", "mode.asan")

add_cxxflags("-fno-rtti", "-fno-exceptions")
add_cxxflags("-fdata-sections", "-ffunction-sections")

add_includedirs("include")
add_includedirs("third_party")

includes("test/xmake.lua")
includes("tools/**/xmake.lua")
includes("third_party/**/xmake.lua")

target("rv64_emulator")
    if is_mode("debug") then
        add_cxxflags("-g")
        add_defines("DEBUG")
    end
    set_kind("binary")
    set_targetdir("build")
    add_files("src/**.cc" )
    add_defines("FMT_HEADER_ONLY")

target("gtest_parallel")
    set_targetdir("build")
    on_build(function (target) 
        os.run("cp third_party/gtest_parallel/gtest-parallel build/")
        os.run("cp third_party/gtest_parallel/gtest_parallel.py build/")
    end)

