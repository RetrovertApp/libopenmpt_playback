require "tundra.syntax.glob"
require "tundra.path"
require "tundra.util"

-----------------------------------------------------------------------------------------------------------------------

local function get_c_cpp_src(dir)
	return Glob {
		Dir = dir,
		Extensions = { ".cpp", ".c" },
		Recursive = true,
	}
end

-----------------------------------------------------------------------------------------------------------------------

SharedLibrary {
	Name = "libopenmpt",
	Defines = { "LIBOPENMPT_BUILD" },

    Env = {
       CXXOPTS = {
			{ "-std=c++17"; Config = "linux-*-*" },
			{ "-std=c++17"; Config = "mac*-*-*" },
			{ "/std:c++latest"; Config = "win64-*-*" },
		},
    },

	Includes = {
	    "../retrovert_api/c",
		"libopenmpt",
		"libopenmpt/src",
		"libopenmpt/common"
	},

	Sources = {
		get_c_cpp_src("libopenmpt/soundlib"),
		get_c_cpp_src("libopenmpt/common"),
		get_c_cpp_src("libopenmpt/sounddsp"),
		"libopenmpt/libopenmpt/libopenmpt_c.cpp",
		"libopenmpt/libopenmpt/libopenmpt_cxx.cpp",
		"libopenmpt/libopenmpt/libopenmpt_impl.cpp",
		"libopenmpt/libopenmpt/libopenmpt_ext_impl.cpp",
		"libopenmpt_plugin.cpp",
	},

	Libs = {
		{ "stdc++"; Config = "macosx-*-*" },
		{ "Rpcrt4.lib"; Config = "win64-*-*" },
	},
}

-----------------------------------------------------------------------------------------------------------------------

Default "libopenmpt"
