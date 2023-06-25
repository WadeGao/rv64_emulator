add_includedirs(".")
add_includedirs("include")
set_kind("shared")
set_targetdir("$(projectdir)/build")

target("gtest")
    add_files("src/gtest-all.cc")
    add_deps("gtest_parallel")

target("gtest_main")
    add_files("src/gtest_main.cc")
    add_deps("gtest")

target("gmock")
    add_files("src/gmock-all.cc")
    add_deps("gtest")

target("gmock_main")
    add_files("src/gmock_main.cc")
    add_deps("gmock")
