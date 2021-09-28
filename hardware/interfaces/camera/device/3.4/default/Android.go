package libcameradevice

import (
        "android/soong/android"
        "android/soong/cc"
        "fmt"
        "strings"
)

func init() {
    fmt.Println("libcameradevice want to conditional Compile")
    android.RegisterModuleType("cc_libcameradevice", DefaultsFactory)
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

    var boardName string = "rk356x"
	if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"), "rk3328")) {
	    boardName = "rk3328"
	}
    fmt.Println("libcameradevice curr board is " + boardName)
	p := &props{}
	p.Srcs = getSrcs(ctx)
	p.Cflags = getCflags(ctx)

	ctx.AppendProperties(p)
}

//条件编译主要修改函数
func getCflags(ctx android.BaseContext) ([]string) {
    var cppflags []string

    //该打印输出为: TARGET_PRODUCT:rk3328 fmt.Println("TARGET_PRODUCT:",ctx.AConfig().Getenv("TARGET_PRODUCT")) //通过 strings.EqualFold 比较字符串，可参考go语言字符串对比
    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"),"rk3328") ) {
    } else {
	    cppflags = append(cppflags,"-DRK_GRALLOC_4")
	}

    //将需要区分的环境变量在此区域添加 //....
    return cppflags
}

func getSrcs(ctx android.BaseContext) ([]string) {
    var src []string

    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"),"rk3328") ) {
        src = append(src, "ExternalCameraGralloc.cpp")
	} else {
	    src = append(src, "ExternalCameraGralloc4.cpp")
	}

    return src
}
