if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

add_llvm_library( LLVMHello MODULE BUILDTREE_ONLY
  Detection.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )