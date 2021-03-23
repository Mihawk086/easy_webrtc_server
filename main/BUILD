load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library","cc_import")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "test_webrtc",
    srcs = glob([
        "*.cc",
    ]),
    visibility = [
        "//visibility:public",
    ],
    deps = [
      "//ffmpeg_src",
      "//log",
      "//dtls",
      "//srtp",
      "//webrtctransport"
    ],
)

 
