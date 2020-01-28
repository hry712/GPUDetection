//===- DeviceResetDetection.h -  DeviceReset Detection pass-------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file contains includes and defines for the DeviceReset Detection pass
//
//===--------------------------------------------------------------------------------===//
#ifndef _DEVICERESETDETECTION_H_
#define _DEVICERESETDETECTION_H_

#include "llvm/Pass.h"

using namespace llvm;

namespace llvm {
    Pass *createDRD ();
	Pass *createDRD (bool flag);
}

#endif