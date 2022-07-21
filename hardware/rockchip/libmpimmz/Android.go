package libmpimmz

import (
    "android/soong/android"
    "android/soong/cc"
    "fmt"
    "strings"
)

func init() {
    fmt.Println("libmpimmz conditional Compile")
    android.RegisterModuleType("cc_libmpimmz", DefaultsFactory)
}

func DefaultsFactory() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, Defaults)
    return module
}

func Defaults(ctx android.LoadHookContext) {
    type props struct {
        Srcs []string
        Cflags []string
    }

    p := &props{}
    p.Srcs = getSrcs(ctx)
    p.Cflags = getCflags(ctx)

    ctx.AppendProperties(p)
}

func getSrcs(ctx android.BaseContext) ([]string) {
    var src []string

    if (strings.EqualFold(ctx.AConfig().Getenv("PRODUCT_KERNEL_VERSION"), "5.10")) {
        src = append(src, "src/BufferAllocator.cpp")
    }

    return src
}

func getCflags(ctx android.BaseContext) ([]string) {
    var cppflags []string

    if (strings.EqualFold(ctx.AConfig().Getenv("PRODUCT_KERNEL_VERSION"), "5.10")) {
        cppflags = append(cppflags,"-DSUPPORT_DMABUF_ALLOCATOR=1")
    }

    return cppflags
}
