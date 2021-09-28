cc_library(
    name = "libnos_transport",
    srcs = [
        "crc16.c",
        "transport.c",
    ],
    hdrs = [
        "crc16.h",
        "include/nos/transport.h",
    ],
    includes = [
        "include",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//host/generic:nos_headers",
        "//host/generic/libnos_datagram",
    ],
)

cc_test(
    name = "libnos_transport_test",
    srcs = [
        "test/test.cpp",
    ],
    copts = [
        "-fsanitize=address",
        "-Ihost/generic/libnos_transport",
    ],
    linkopts = ["-fsanitize=address"],
    deps = [
        ":libnos_transport",
        "//host/generic:nos_headers",
        "@gtest",
    ],
)
