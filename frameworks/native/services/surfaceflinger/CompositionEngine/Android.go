package  libcompositionengine

import (
        "android/soong/android"
        "android/soong/cc"
        //"fmt"
        "strings"
)

func init() {
	//该打印会在执行mm命令时，打印在屏幕上
    //fmt.Println("libgui want to conditional Compile")

	android.RegisterModuleType("cc_libcompositionengine", DefaultsFactory)
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
    //fmt.Println("BOARD_HS_DYNAMIC_AFBC_TARGET_comp:",ctx.AConfig().Getenv("BOARD_HS_DYNAMIC_AFBC_TARGET"))

    if (!strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"),"rk356x")){
        cflags = append(cflags,"-DDISABLE_EXTERNAL_DISP_AFBC=1")
        cflags = append(cflags,"-DUSE_HWC2ON1ADAPTER=1")
    }

    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_RK_GRALLOC_VERSION"),"4") ) {
        cflags = append(cflags,"-DUSE_GRALLOC_4=1")
    }

    //将需要区分的环境变量在此区域添加 //....
    return cflags
}
