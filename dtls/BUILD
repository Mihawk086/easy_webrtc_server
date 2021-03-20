load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library","cc_import")

filegroup(
    name = "all_files",
    srcs = glob(
        ["**/*"],
    ),
)

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "dtls",
    srcs = glob([
        "*.c",
        "*.cpp",
        "*.cc",
    ]),
    hdrs = glob([
        "*.h",
    ]),
    visibility = [
        "//visibility:public",
    ],
    deps = [
      "//log",
      "//thirdparty/srtp2:srtp2",
      "//thirdparty/openssl:crypto",
      "//thirdparty/openssl:ssl"
    ],
)
 
