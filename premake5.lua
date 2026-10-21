if not enablepch then function enablepch(v) if v == "Off" then flags({"NoPCH"}) end end end
if not linktimeoptimization then function linktimeoptimization(v) if v == "On" then flags({"LinkTimeOptimization"}) end end end
-- Compatibility with Premake beta7 bundled by the upstream repository.
if not multiprocessorcompile then function multiprocessorcompile(_) buildoptions({"/MP"}) end end
newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	value = "../garrysmod_common"
})

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
	"you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
include(gmcommon .. "/generator.v3.lua")

CreateWorkspace({name = "subrosa_mic"})
 CreateProject({serverside = false})
  IncludeDetouring()
  IncludeLuaShared()
