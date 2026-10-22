add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})
set_project("crimmusic")
set_languages("c++17")

add_requires("libsdl3", "nlohmann_json", "taglib", "stb")
add_requires("imgui", {configs = {sdl3 = true, sdl3_renderer = true}})


add_rules("mode.debug", "mode.release")

target("crimmusic")
    set_kind("binary")
    add_includedirs("include", "external")
    add_files("src/*.cpp")
    add_syslinks("pthread", "dl", "m")
    add_packages("libsdl3", "imgui", "nlohmann_json", "taglib", "stb")
