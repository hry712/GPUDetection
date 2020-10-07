if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

add_llvm_library( LLVMDetection MODULE BUILDTREE_ONLY
  LoopDetection.cpp
  LoopInfoDetection.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )