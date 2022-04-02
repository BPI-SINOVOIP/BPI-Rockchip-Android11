package  libcompositionengine

import (
    "android/soong/android"
    "android/soong/cc"
    "fmt"
    "strings"
)

func init() {
    //该打印会在执行mm命令时，打印在屏幕上
    //fmt.Println("================libhealthloop want to conditional Compile")

    android.RegisterModuleType("cc_libhealthloop", DefaultsFactory)
}

func DefaultsFactory() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, Defaults)
    return module
}

func Defaults(ctx android.LoadHookContext) {
    type props struct {
        Cflags []string
    }
    p := &props{}
    p.Cflags = globalDefaults(ctx)
    ctx.AppendProperties(p)
}

//条件编译主要修改函数
func globalDefaults(ctx android.BaseContext) ([]string) {
    var cflags []string
    fmt.Println("TARGET_PRODUCT:",ctx.AConfig().Getenv("TARGET_PRODUCT"))

    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_PRODUCT"),"bananapi_r2pro") ||
	strings.EqualFold(ctx.AConfig().Getenv("TARGET_PRODUCT"),"bananapi_r2pro_box")){
        cflags = append(cflags,"-DBOARD_NO_BATTERY=1")
    }

    //将需要区分的环境变量在此区域添加 //....
    return cflags
}
