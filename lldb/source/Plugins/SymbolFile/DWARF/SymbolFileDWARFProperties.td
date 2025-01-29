include "../../../../include/lldb/Core/PropertiesBase.td"

let Definition = "symbolfiledwarf" in {
  def IgnoreIndexes: Property<"ignore-file-indexes", "Boolean">,
    Global,
    DefaultFalse,
    Desc<"Ignore indexes present in the object files and always index DWARF manually.">;
  def UseSourceLocations: Property<"use-source-locations", "Boolean">,
    Global,
    DefaultTrue,
    Desc<"Generate source locations for declarations to improve error diagnostics.">;
}
