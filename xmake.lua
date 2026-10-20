add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})
set_project("crimmusic")
set_languages("c++17")

add_rules("mode.debug", "mode.release")

target("crimmusic")
    set_kind("binary")
    add_includedirs("include", "external")
    add_files("src/main.cpp", "src/miniaudio.cpp", "src/Player.cpp")
    add_syslinks("pthread", "dl", "m")
