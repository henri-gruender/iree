# Copyright 2022 The IREE Authors
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

if(IREE_BYTECODE_MODULE_FORCE_SYSTEM_DYLIB_LINKER)
  set(EXECUTABLE_FORMAT_PREFIX "system")
else()
  set(EXECUTABLE_FORMAT_PREFIX "embedded")
endif()
set(NATIVE_EXECUTABLE_FORMAT "\"${EXECUTABLE_FORMAT_PREFIX}-elf-\" IREE_ARCH")

if(IREE_HAL_EXECUTABLE_LOADER_EMBEDDED_ELF)
  iree_hal_cts_test_suite(
    DRIVER_NAME
      local-task
    VARIANT_SUFFIX
      embedded-elf
    DRIVER_REGISTRATION_HDR
      "runtime/src/iree/hal/drivers/local_task/registration/driver_module.h"
    DRIVER_REGISTRATION_FN
      "iree_hal_local_task_driver_module_register"
    COMPILER_TARGET_BACKEND
      "dylib-llvm-aot"
    EXECUTABLE_FORMAT
      "${NATIVE_EXECUTABLE_FORMAT}"
    DEPS
      iree::hal::drivers::local_task::registration
  )
endif()

if(IREE_HAL_EXECUTABLE_LOADER_VMVX_MODULE)
  iree_hal_cts_test_suite(
    DRIVER_NAME
      local-task
    VARIANT_SUFFIX
      vmvx
    DRIVER_REGISTRATION_HDR
      "runtime/src/iree/hal/drivers/local_task/registration/driver_module.h"
    DRIVER_REGISTRATION_FN
      "iree_hal_local_task_driver_module_register"
    COMPILER_TARGET_BACKEND
      "vmvx"
    EXECUTABLE_FORMAT
      "\"vmvx-bytecode-fb\""
    DEPS
      iree::hal::drivers::local_task::registration
  )
endif()
