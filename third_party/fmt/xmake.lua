target("fmt")
    set_kind("shared")
    set_targetdir("$(projectdir)/build")
    add_defines("FMT_EXPORT", "FMT_SHARED")
    add_files("src/format.cc")
