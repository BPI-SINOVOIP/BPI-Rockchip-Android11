package librockit

import (
        "android/soong/android"
        "android/soong/cc"
        "fmt"
        "strings"
)

func init() {
  //该打印会在执行mm命令时，打印在屏幕上
  fmt.Println("librockit want to conditional Compile")
  android.RegisterModuleType("cc_librockit_prebuilt_library_shared", DefaultsFactory)
}

func DefaultsFactory() (android.Module) {
// 要获取对应module的factory，这一步很重要
    module := cc.PrebuiltSharedLibraryFactory()
    android.AddLoadHook(module, Defaults)
    return module
}

func Defaults(ctx android.LoadHookContext) {
    type props struct {
        Srcs []string
    }
    p := &props{}

    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_EBOOK"),"true")) {
        //fmt.Println("librockit want to conditional Compile: use ebook so")
        p.Srcs = append(p.Srcs,"ebook/librockit.so")
    } else {
        //fmt.Println("librockit want to conditional Compile: use default so")
        p.Srcs = append(p.Srcs,"librockit.so")
    }
    ctx.AppendProperties(p)
}
