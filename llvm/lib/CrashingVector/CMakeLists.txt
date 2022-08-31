set(LLVM_LINK_COMPONENTS
  BinaryFormat
  Core
  FrontendOpenMP
  Support
  )

add_llvm_component_library(LLVMCrashingVector
  SmallVector.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/CrashingVector

  LINK_COMPONENTS
  Support
  )
