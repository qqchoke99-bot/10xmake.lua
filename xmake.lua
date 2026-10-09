add_rules("mode.debug", "mode.release")

package("preloader")
    set_homepage("https://github.com/LiteLDev/preloader-android")
    set_description("Preloader Android")
    add_urls("https://github.com/LiteLDev/preloader-android.git")
    add_versions("main", "main")
    add_deps("cmake")
    on_install("android", function (package)
        import("package.tools.cmake").install(package)
    end)
package_end()

add_requires("preloader")
add_requires("nlohmann_json v3.11.3")
add_requires("fmt")

target("SoundPhysicsLite")
    set_kind("shared")
    set_languages("c++20")
    set_strip("all")
    add_files("src/main.cpp", "src/core/**.cpp", "src/module/*.cpp", "src/launcher/*.cpp")
    add_includedirs("include", {public = true})
    add_includedirs("src")
    add_packages("preloader", "nlohmann_json", "fmt")

    if is_plat("android") then
        add_cxflags("-fPIC", "-Oz", "-ffunction-sections", "-fdata-sections", "-flto",
                    "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
                    "-fmerge-all-constants", "-fno-stack-protector", "-fexceptions", "-w",
                    "-fvisibility=hidden")
        add_cxxflags("-fno-rtti", "-fvisibility-inlines-hidden")
        add_shflags("-Wl,--gc-sections", "-Wl,--icf=all", "-flto", "-Wl,--hash-style=gnu",
                    "-Wl,-z,max-page-size=16384")
        add_links("android", "log")
    end

    after_build(function (target)
        if not target:is_plat("android") then return end
        import("lib.detect.find_tool")
        local python = find_tool("python3") or find_tool("python")
        local args = {}
        if not python then
            python = find_tool("py")
            if python then table.insert(args, "-3") end
        end
        assert(python, "Python 3 is required to package SoundPhysicsLite.levipack")
        table.insert(args, path.join(os.projectdir(), "scripts", "package_levipack.py"))
        table.insert(args, "--library")
        table.insert(args, target:targetfile())
        table.insert(args, "--icon")
        table.insert(args, path.join(os.projectdir(), "assets", "icon.png"))
        table.insert(args, "--output")
        table.insert(args, path.join(target:targetdir(), "SoundPhysicsLite.levipack"))
        os.execv(python.program, args)
    end)
