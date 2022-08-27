load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library","cc_import")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "net",
    srcs = glob([
        "*.cc",
    ]),
    hdrs = glob([
        "*.h",
    ]),
    visibility = [
        "//visibility:public",
    ],
    deps = [
      "//common"
    ],
)

 
