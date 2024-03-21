include "../../../../include/lldb/Core/PropertiesBase.td"

let Definition = "typesystemclang" in {
  def ShowASTColors: Property<"show-ast-colors", "Boolean">,
    Global,
    DefaultTrue,
    Desc<"If true, any AST nodes dumped by LLDB will be syntax highlighted.">;
}
