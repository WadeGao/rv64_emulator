set_languages("cxx17")
add_rules("mode.debug", "mode.release", "mode.minsizerel", "mode.releasedbg", "mode.asan")

add_cxxflags("-fno-rtti", "-fno-exceptions")
add_cxxflags("-fdata-sections", "-ffunction-sections")
add_cxxflags("-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-fno-ident")

add_includedirs("include")
add_includedirs("third_party")

includes("test/xmake.lua")
includes("tools/**/xmake.lua")
includes("third_party/**/xmake.lua")

target("rv64_emulator")
    if is_mode("debug") then
        add_cxxflags("-g")
    end
    set_kind("binary")
    set_targetdir("build")
    add_files("src/**.cc" )
    add_deps("asmjit")
    add_defines("FMT_HEADER_ONLY")

target("gtest_parallel")
    set_targetdir("build")
    on_build(function (target) 
        os.run("cp third_party/gtest_parallel/gtest-parallel build/")
        os.run("cp third_party/gtest_parallel/gtest_parallel.py build/")
    end)

target("asmjit")
    set_targetdir("build")
    set_kind("shared")
    add_files("third_party/asmjit/src/asmjit/**.cpp")
    if is_plat("linux") then
        add_syslinks("rt")
    end
    add_syslinks("pthread")
    add_cxxflags("-Wall", "-Wextra", "-Wconversion")
    add_cxxflags("-fno-math-errno", "-fno-threadsafe-statics", "-fno-semantic-interposition", "-fmerge-all-constants")
