package libRkTeeWeaver

import (
    "android/soong/android"
    "android/soong/cc"
    "fmt"
)

func init() {
    fmt.Println("libRkTeeWeaver want to conditional Compile")
    android.RegisterModuleType("cc_libRkTeeWeaver", DefaultsFactory)
}

func DefaultsFactory() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, AddOpteeShardLibs)
    return module
}

func AddOpteeShardLibs(ctx android.LoadHookContext) {
    type props struct {
        Shared_libs []string
    }
    var src_fix string = "libRkTeeWeaver."+getOpteeVersion(ctx)
    p := &props{}
    p.Shared_libs = append(p.Shared_libs, src_fix)
    ctx.AppendProperties(p)
}

func isContain(items []string, item string) bool {
    for _, eachItem := range items {
        if eachItem == item {
            return true
        }
    }
    return false
}

func getOpteeVersion(ctx android.BaseContext) (string) {
    var optee_version string = "v1"
    var optee_v2_list = []string{"rk3326", "rk356x"}
    if (isContain(optee_v2_list, ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))) {
        optee_version = "v2"
    }
    fmt.Println("Optee Version: " + optee_version)
    return optee_version
}
