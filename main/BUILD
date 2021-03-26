load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library","cc_import")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "ffmpeg_src_example",
    srcs = glob([
        "ffmpeg_src_example.cc",
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

cc_binary(
    name = "rtp_src_example",
    srcs = glob([
        "rtp_src_example.cc",
    ]),
    visibility = [
        "//visibility:public",
    ],
    deps = [
      "//log",
      "//dtls",
      "//srtp",
      "//webrtctransport"
    ],
)

 
