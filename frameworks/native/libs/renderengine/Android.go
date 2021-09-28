package librenderengine

import (
        "android/soong/android"
        "android/soong/cc"
//        "fmt"
        "strings"
)

func init() {
	//该打印会在执行mm命令时，打印在屏幕上
    //fmt.Println("librenderengine want to conditional Compile")

	android.RegisterModuleType("cc_librenderengine", DefaultsFactory)
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
	var cppflags []string
	//该打印输出为: TARGET_PRODUCT:rk3328 fmt.Println("TARGET_PRODUCT:",ctx.AConfig().Getenv("TARGET_PRODUCT")) //通过 strings.EqualFold 比较字符串，可参考go语言字符串对比
    //fmt.Println("TARGET_BOARD_PLATFORM_GPU:",ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU")) //通过 strings.EqualFold 比较字符串，可参考go语言字符串对比
    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-t720")) {
	//添加 DEBUG 宏定义
        cppflags = append(cppflags,"-DMALI_PRODUCT_ID_T72X=1")
    } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-t760")) {
        cppflags = append(cppflags,"-DMALI_PRODUCT_ID_T76X=1")
    } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-t860")) {
        cppflags = append(cppflags,"-DMALI_PRODUCT_ID_T86X=1")
    } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"G6110")) {
        cppflags = append(cppflags,"-DGPU_G6110")
    } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-tDVx")) {
        cppflags = append(cppflags,"-DMALI_PRODUCT_ID_TDVX=1")
    } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali400")) {
        cppflags = append(cppflags,"-DMALI_PRODUCT_ID_400=1")
    } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali450")) {
        cppflags = append(cppflags,"-DMALI_PRODUCT_ID_450=1")
    }

    //fmt.Println("TARGET_PRODUCT:",ctx.AConfig().Getenv("TARGET_PRODUCT"))
    if (strings.Contains(ctx.AConfig().Getenv("TARGET_PRODUCT"),"rk3288")) {
        cppflags = append(cppflags,
           "-DRK_NV12_10_TO_NV12_BY_RGA=0",
           "-DRK_NV12_10_TO_NV12_BY_NENO=1")
    }else if (strings.Contains(ctx.AConfig().Getenv("TARGET_PRODUCT"),"rk3399")){
        cppflags = append(cppflags,
            "-DRK_HDR=1")
    }else if (strings.Contains(ctx.AConfig().Getenv("TARGET_PRODUCT"),"rk3566")){

    }else if (strings.Contains(ctx.AConfig().Getenv("TARGET_PRODUCT"),"rk3568")){

    }else if (strings.Contains(ctx.AConfig().Getenv("TARGET_PRODUCT"),"rk356x")){

    }else{
        cppflags = append(cppflags,
            "-DRK_NV12_10_TO_NV12_BY_RGA=1",
            "-DRK_NV12_10_TO_NV12_BY_NENO=0")
    }

	//将需要区分的环境变量在此区域添加 //....
	return cppflags
}
