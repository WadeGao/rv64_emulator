target("unit_test")
    if is_mode("debug") then
        add_cxxflags("-g")
    end
    if is_mode("coverage") then
        add_cflags("-O3 -fprofile-arcs -ftest-coverage")
        add_cxxflags("-O3 -fprofile-arcs -ftest-coverage")
        add_ldflags("-fprofile-arcs -ftest-coverage")
        after_build(function (target)
            os.run("$(projectdir)/scripts/gen_coverage_report.sh")
        end)
    end
    add_cxxflags("-fno-access-control")
    set_kind("binary")
    set_targetdir("$(projectdir)/build")
    add_includedirs("$(projectdir)/third_party/gtest/include")
    add_files("**.cc")
    add_files("$(projectdir)/src/**.cc|rv64_emulator.cc")
    add_deps("gtest")
    add_deps("asmjit")
    add_defines("FMT_HEADER_ONLY")
    add_defines("UNIT_TEST")
