package libgralloc_priv

import (
    "android/soong/android"
    "android/soong/cc"
    "fmt"
    "strings"
    "strconv"
)

func init() {
    //该打印会在执行mm命令时，打印在屏幕上
    fmt.Println("libgralloc_priv want to conditional Compile")
    android.RegisterModuleType("cc_libgralloc_priv", DefaultsFactory)
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
        Shared_libs []string
        Include_dirs []string
    }
    p := &props{}
    p.Cflags = globalCflagsDefaults(ctx)
    p.Include_dirs = globalIncludeDefaults(ctx)
    p.Srcs = getSrcs(ctx)
    p.Shared_libs = getSharedLibs(ctx)

    ctx.AppendProperties(p)
}


func getSharedLibs(ctx android.BaseContext) ([]string) {
    var libs []string
    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_RK_GRALLOC_VERSION"),"4") ) {
        libs = append(libs, "libgralloctypes")
        libs = append(libs, "libhidlbase")
        libs = append(libs, "libsync")
        libs = append(libs, "android.hardware.graphics.mapper@4.0")
    }
    return libs
}

func getSrcs(ctx android.BaseContext) ([]string) {
    var src []string
    sdkVersion := ctx.AConfig().PlatformSdkVersionInt()
    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_RK_GRALLOC_VERSION"),"4") ) {
        if (sdkVersion >= 30 ) {
            src = append(src, "platform_gralloc4.cpp")
        }
    }
    return src
}

func globalIncludeDefaults(ctx android.BaseContext) ([]string) {
    var include_dirs []string
    version, err := strconv.Atoi(ctx.Config().PlatformSdkVersion())
    if (err == nil && version < 29 ) {
        if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"G6110")) {
            fmt.Println("G6110 don't contains hardware/rockchip/libgralloc!");
        } else {
            include_dirs = append(include_dirs,"hardware/rockchip/libgralloc")
        }
    } else {
        if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-tDVx") || strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-G52")) {
            include_dirs = append(include_dirs,"hardware/rockchip/libgralloc/bifrost")
        } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-t860") || strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-t760")) {
            include_dirs = append(include_dirs,"hardware/rockchip/libgralloc/midgard")
        } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali400") || strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali450")) {
            include_dirs = append(include_dirs,"hardware/rockchip/libgralloc/utgard")
        } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"G6110")) {
            fmt.Println("G6110 don't contains hardware/rockchip/libgralloc!");
        } else {
            include_dirs = append(include_dirs,"hardware/rockchip/libgralloc")
        }
    }
    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_RK_GRALLOC_VERSION"),"4") ) {
        include_dirs = append(include_dirs, "frameworks/native/include")
        include_dirs = append(include_dirs, "system/core/libsync")
        include_dirs = append(include_dirs, "system/core/libsync/include")
        include_dirs = append(include_dirs, "external/libdrm/include/drm")
    }
    fmt.Println(include_dirs, ctx.Config().PlatformSdkVersion())
    return include_dirs

}

func globalCflagsDefaults(ctx android.BaseContext) ([]string) {
    var cppflags []string
    //该打印输出为: TARGET_PRODUCT:rk3328 fmt.Println("TARGET_PRODUCT:",ctx.AConfig().Getenv("TARGET_PRODUCT")) //通过 strings.EqualFold 比较字符串，可参考go语言字符串对比
    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-t720")) {
        //添加 DEBUG 宏定义
        cppflags = append(cppflags,"-DMALI_PRODUCT_ID_T72X=1")
        cppflags = append(cppflags,"-DMALI_AFBC_GRALLOC=0")
    } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-t760")) {
        cppflags = append(cppflags,"-DMALI_PRODUCT_ID_T76X=1")
        cppflags = append(cppflags,"-DMALI_AFBC_GRALLOC=1")
    } else if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"mali-t860")) {
        cppflags = append(cppflags,"-DMALI_PRODUCT_ID_T86X=1")
        cppflags = append(cppflags,"-DMALI_AFBC_GRALLOC=1")
    }

    if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_RK_GRALLOC_VERSION"),"4") ) {
        cppflags = append(cppflags,"-DUSE_GRALLOC_4")
    } else {
        if (strings.EqualFold(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM_GPU"),"G6110")) {
            cppflags = append(cppflags,"-DGPU_G6110")
        } else {
            cppflags = append(cppflags,"-DUSE_DRM")
        }
    }
    //将需要区分的环境变量在此区域添加 //....
    return cppflags
}
