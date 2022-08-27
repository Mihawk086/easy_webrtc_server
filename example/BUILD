load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library","cc_import")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "rtp_src_example",
    linkopts = [
        "-L /usr/local/lib",
        "-lsrtp2",
        "-lssl",
        "-lcrypto",
        "-lmuduo_http",
        "-lmuduo_net",
        "-lmuduo_base",
        "-lpthread",
    ],
    srcs = glob([
        "rtp_src_example.cc",
    ]),
    visibility = [
        "//visibility:public",
    ],
    deps = [
      "//common",
      "//rtc",
      "//net"
    ],
)

 
