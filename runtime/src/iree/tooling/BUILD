# Copyright 2020 The IREE Authors
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//build_tools/bazel:build_defs.oss.bzl", "iree_cmake_extra_content", "iree_runtime_cc_test")

package(
    default_visibility = ["//visibility:public"],
    features = ["layering_check"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "cpu_features",
    srcs = ["cpu_features.c"],
    hdrs = ["cpu_features.h"],
    deps = [
        "//runtime/src/iree/base",
        "//runtime/src/iree/base:target_platform",
    ],
)

cc_library(
    name = "device_util",
    srcs = ["device_util.c"],
    hdrs = ["device_util.h"],
    deps = [
        "//runtime/src/iree/base",
        "//runtime/src/iree/base:tracing",
        "//runtime/src/iree/base/internal:flags",
        "//runtime/src/iree/base/internal:synchronization",
        "//runtime/src/iree/hal",
        "//runtime/src/iree/hal/drivers",
    ],
)

cc_library(
    name = "numpy_io",
    srcs = ["numpy_io.c"],
    hdrs = ["numpy_io.h"],
    deps = [
        "//runtime/src/iree/base",
        "//runtime/src/iree/base:tracing",
        "//runtime/src/iree/hal",
    ],
)

iree_runtime_cc_test(
    name = "numpy_io_test",
    srcs = ["numpy_io_test.cc"],
    deps = [
        ":device_util",
        ":numpy_io",
        "//runtime/src/iree/base/internal:file_io",
        "//runtime/src/iree/testing:gtest",
        "//runtime/src/iree/testing:gtest_main",
        "//runtime/src/iree/tooling/testdata/npy",
    ],
)

# TODO(benvanik): fold these into iree/runtime and use that instead.
cc_library(
    name = "vm_util",
    srcs = ["vm_util.cc"],
    hdrs = ["vm_util.h"],
    deps = [
        ":numpy_io",
        "//runtime/src/iree/base",
        "//runtime/src/iree/base:cc",
        "//runtime/src/iree/base:tracing",
        "//runtime/src/iree/base/internal:span",
        "//runtime/src/iree/hal",
        "//runtime/src/iree/modules/hal",
        "//runtime/src/iree/vm",
        "//runtime/src/iree/vm:bytecode_module",
        "//runtime/src/iree/vm:cc",
    ],
)

cc_test(
    name = "vm_util_test",
    srcs = ["vm_util_test.cc"],
    deps = [
        ":device_util",
        ":vm_util",
        "//runtime/src/iree/base",
        "//runtime/src/iree/hal",
        "//runtime/src/iree/modules/hal",
        "//runtime/src/iree/testing:gtest",
        "//runtime/src/iree/testing:gtest_main",
        "//runtime/src/iree/vm",
        "//runtime/src/iree/vm:cc",
    ],
)

iree_cmake_extra_content(
    content = """
# libyaml does not build cleanly on bare-metal systems
if(IREE_ENABLE_THREADING)
""",
    inline = True,
)

cc_library(
    name = "trace_replay",
    srcs = ["trace_replay.c"],
    hdrs = ["trace_replay.h"],
    deps = [
        ":yaml_util",
        "//runtime/src/iree/base",
        "//runtime/src/iree/base:tracing",
        "//runtime/src/iree/base/internal:file_io",
        "//runtime/src/iree/base/internal:path",
        "//runtime/src/iree/hal",
        "//runtime/src/iree/modules/hal",
        "//runtime/src/iree/vm",
        "//runtime/src/iree/vm:bytecode_module",
        "@com_github_yaml_libyaml//:yaml",
    ],
)

cc_library(
    name = "yaml_util",
    srcs = ["yaml_util.c"],
    hdrs = ["yaml_util.h"],
    deps = [
        "//runtime/src/iree/base",
        "@com_github_yaml_libyaml//:yaml",
    ],
)

iree_cmake_extra_content(
    content = """
endif()
""",
    inline = True,
)
