local native = require('tundra.native')

-----------------------------------------------------------------------------------------------------------------------

local mac_opts = {
    "-Wall", "-I.",
    "-DRV_MAC",
    { "-DRV_DEBUG", "-O0", "-g"; Config = "*-*-debug" },
    { "-DRV_DEBUG", "-O0", "-fsanitize=address", "-fno-omit-frame-pointer", "-g"; Config = "*-*-debug-asan" },
    { "-DRV_RELEASE", "-O3", "-g"; Config = "*-*-release" },
}

-----------------------------------------------------------------------------------------------------------------------

local macosx = {
    Env = {
		RV_VERSION = native.getenv("RV_VERSION", "Development Version"),
		BGFX_SHADERC = "$(OBJECTDIR)$(SEP)bgfx_shaderc$(PROGSUFFIX)",

        RUST_CARGO_OPTS = {
            { "test"; Config = "*-*-*-test" },
        },

        CCOPTS =  {
            mac_opts,
        },

        CXXOPTS = {
            mac_opts,
            "-DRV_VERSION='\"$(RV_VERSION)\"'",
            "-std=c++11",
        },

        SHLIBOPTS = {
			"-lstdc++",
			{ "-fsanitize=address"; Config = "*-*-debug-asan" },
		},

        PROGCOM = {
			"-lstdc++",
			{ "-fsanitize=address"; Config = "*-*-debug-asan" },
		},
    },

    ReplaceEnv = {
        HOSTSHLIBSUFFIX = ".rvp",
    },

    Frameworks = {
        { "Cocoa" },
        { "Metal" },
        { "QuartzCore" },
        { "OpenGL" }
    },
}

-----------------------------------------------------------------------------------------------------------------------

local gcc_opts = {
    "-I.",
    "-Wno-array-bounds",
    "-Wno-attributes",
    "-Wno-unused-value",
    "-DOBJECT_DIR=\\\"$(OBJECTDIR)\\\"",
    "-I$(OBJECTDIR)",
    "-Wall",
    "-fPIC",
    { "-DRV_DEBUG", "-O0", "-g"; Config = "*-*-debug" },
    { "-DRV_RELEASE", "-O3", "-g"; Config = "*-*-release" },
}

local gcc_env = {
    Env = {
		RV_VERSION = native.getenv("RV_VERSION", "Development Version"),
		BGFX_SHADERC = "$(OBJECTDIR)$(SEP)bgfx_shaderc$(PROGSUFFIX)",

        RUST_CARGO_OPTS = {
            { "test"; Config = "*-*-*-test" },
        },

        CCOPTS = {
			"-Werror=incompatible-pointer-types",
            gcc_opts,
        },

        CXXOPTS = {
            gcc_opts,
            "-DRV_VERSION='\"$(RV_VERSION)\"'",
            "-std=c++11",
        },
    },

    ReplaceEnv = {
        LD = "c++",
        HOSTSHLIBSUFFIX = ".rvp",
    },
}

-----------------------------------------------------------------------------------------------------------------------

local win64_opts = {
    "/EHsc", "/FS", "/MD", "/W3", "/I.", "/DUNICODE", "/D_UNICODE", "/DWIN32", "/D_CRT_SECURE_NO_WARNINGS",
    "\"/DOBJECT_DIR=$(OBJECTDIR:#)\"",
    { "/DRV_DEBUG","/Od"; Config = "*-*-debug" },
    { "/DRV_RELEASE", "/O2"; Config = "*-*-release" },
}

local win64 = {
    Env = {
		RV_VERSION = native.getenv("RV_VERSION", "Development Version"),
		BGFX_SHADERC = "$(OBJECTDIR)$(SEP)bgfx_shaderc$(PROGSUFFIX)",

        RUST_CARGO_OPTS = {
            { "test"; Config = "*-*-*-test" },
        },

        GENERATE_PDB = "1",
        CCOPTS = {
            win64_opts,
        },

        HOSTSHLIBSUFFIX = ".rvp",

        CXXOPTS = {
            win64_opts,
            "\"/DRV_VERSION=$(RV_VERSION:#)\"",
            "/Isrc/external/flatbuffers/include",
        },

        OBJCCOM = "meh",
    },

    ReplaceEnv = {
        HOSTSHLIBSUFFIX = ".rvp",
    },
}

-----------------------------------------------------------------------------------------------------------------------

Build {
    Passes = {
        BuildTools = { Name = "Build Tools", BuildOrder = 1 },
        GenerateSources = { Name = "Generate sources", BuildOrder = 2 },
    },

    Units = {
        "units.lua",
    },

    Configs = {
        Config { Name = "macosx-clang", DefaultOnHost = "macosx", Inherit = macosx, Tools = { "clang-osx", "rust" } },
        Config { Name = "win64-msvc", DefaultOnHost = { "windows" }, Inherit = win64, Tools = { "msvc-vs2019", "rust" } },
        Config { Name = "linux-gcc", DefaultOnHost = { "linux" }, Inherit = gcc_env, Tools = { "gcc", "rust" } },
        Config { Name = "linux-clang", DefaultOnHost = { "linux" }, Inherit = gcc_env, Tools = { "clang", "rust" } },
    },

    IdeGenerationHints = {
        Msvc = {
            -- Remap config names to MSVC platform names (affects things like header scanning & debugging)
            PlatformMappings = {
                ['win64-msvc'] = 'x64',
            },

            -- Remap variant names to MSVC friendly names
            VariantMappings = {
                ['release']    = 'Release',
                ['debug']      = 'Debug',
            },
        },
    },

    Variants = { "debug", "release" },
    SubVariants = { "default" },
}

-- vim: ts=4:sw=4:sts=4

