package rockchip_prebuilts

import (
    "android/soong/android"
    "android/soong/cc"
    "fmt"
    "strings"
)

func isContain(items []string, item string) bool {
    for _, eachItem := range items {
        if eachItem == item {
            return true
        }
    }
    return false
}

func getOpteePrefix(platform string) string {
    var optee_v2_list = []string{"rk3326", "rk356x"}
    if isContain(optee_v2_list, platform) {
        return "v2/"
    } else {
        return "v1/"
    }
}

func getVpuPrefix(platform string) string {
    var vpu_v1_list = []string{"rk3126c", "rk3288", "rk3368"}
    if isContain(vpu_v1_list, platform) {
        return ""
    } else {
        return "mpp_dev/"
    }
}

func init() {
    fmt.Println("Rockchip conditional compile")
    android.RegisterModuleType("cc_rockchip_prebuilt_library_shared", RockchipPrebuiltLibsFactory)
    android.RegisterModuleType("cc_rockchip_prebuilt_binary", RockchipPrebuiltBinFactory)
    android.RegisterModuleType("cc_rockchip_prebuilt_library_static", RockchipPrebuiltLibsStaticFactory)
}

func RockchipPrebuiltLibsFactory() (android.Module) {
    // Register rockchip_prebuilt_libs factory as PrebuiltSharedLibraryFactory
    module := cc.PrebuiltSharedLibraryFactory()

    // Add new props for rockchip conditional compile
    addon_props := &soongRockchipPrebuiltProperties{}
    module.AddProperties(addon_props)

    // Add Hook for PrebuiltSharedLibraryFactory
    android.AddLoadHook(module, AppendMultilibs)
    return module
}

func RockchipPrebuiltBinFactory() (android.Module) {
    // Register rockchip_prebuilt_binary factory as PrebuiltBinaryFactory
    module := cc.PrebuiltBinaryFactory()

    // Add new props for rockchip conditional compile
    addon_props := &soongRockchipPrebuiltProperties{}
    module.AddProperties(addon_props)

    // Add Hook for PrebuiltBinaryFactory
    android.AddLoadHook(module, ChangeSrcsPath)
    return module
}

func RockchipPrebuiltLibsStaticFactory() (android.Module) {
    // Register rockchip_prebuilt_libs factory as PrebuiltStaticLibraryFactory
    module := cc.PrebuiltStaticLibraryFactory()

    // Add new props for rockchip conditional compile
    addon_props := &soongRockchipPrebuiltProperties{}

    module.AddProperties(addon_props)

    // Add Hook for PrebuiltStaticLibraryFactory
    //android.AddLoadHook(module, AppendArchStaticLibs)
    android.AddLoadHook(module, AppendArchStaticLibs)
    return module
}

/* *
 * For prebuilt binary and object
 * optee --> srcs: v1(2)/arm(64)/$(name)
 * optee --> srcs: arm(64)/$(name)
 */
func ChangeSrcsPath(ctx android.LoadHookContext) {
    var prefix string = ""
    var module_name string = ctx.ModuleName()[9:]
    type props struct {
        Srcs []string
    }
    p := &props{}
    if (ctx.ContainsProperty("optee")) {
        prefix = getOpteePrefix(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))
    }
    if (strings.EqualFold(ctx.AConfig().DevicePrimaryArchType().String(),"arm64")) {
        prefix += "arm64/"
    } else {
        prefix += "arm/"
    }
    if (ctx.ContainsProperty("vpu")) {
        prefix += getVpuPrefix(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))
    }
    p.Srcs = append(p.Srcs, prefix + module_name)
    //fmt.Println("srcs: ", p.Srcs)
    ctx.AppendProperties(p)
}

type Ex_srcs struct {
    Srcs []string
}

type Ex_multilibType struct {
    Lib32 Ex_srcs
    Lib64 Ex_srcs
}

type soongRockchipPrebuiltProperties struct {
    Optee bool
    Vpu bool
    Aiq bool
}

func AppendMultilibs(ctx android.LoadHookContext) {
    if (strings.EqualFold(ctx.AConfig().DevicePrimaryArchType().String(),"arm64")) {
        type props struct {
            Compile_multilib *string
            Multilib Ex_multilibType
        }
        p := &props{}
        p.Compile_multilib = peferCompileMultilib(ctx)
        p.Multilib = configArm64Lib(ctx)
        ctx.AppendProperties(p)
    } else {
        type props struct {
            Compile_multilib *string
            Srcs []string
        }
        p := &props{}
        p.Compile_multilib = peferCompileMultilib(ctx)
        p.Srcs = configArmLib(ctx)
        ctx.AppendProperties(p)
    }
}

func AppendArchStaticLibs(ctx android.LoadHookContext) {
        type props struct {
            Compile_multilib *string
            Multilib Ex_multilibType
        }
        p := &props{}
        p.Compile_multilib = peferCompileMultilib(ctx)
        p.Multilib = configArm64LibStatic(ctx)
        ctx.AppendProperties(p)
}
// Change the lib path, chose lib/lib64
func peferCompileMultilib(ctx android.LoadHookContext) (*string) {
    /*fmt.Println("TARGET_PRODUCT:", ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))
    fmt.Println("TARGET_ARCH:", ctx.AConfig().DevicePrimaryArchType().String())
    fmt.Println("MODULE NAME:", ctx.ModuleName()[9:]) // Skip 'prebuilt_'
    fmt.Println("isOptee:", ctx.ContainsProperty("optee"))*/

    var compile_multilib string
    target_arch := ctx.AConfig().DevicePrimaryArchType().String()
    if (strings.EqualFold(target_arch,"arm64")) {
        compile_multilib = "both"
    } else {
        compile_multilib = "32"
    }
    //fmt.Println("compile_multilib:", compile_multilib)
    return &compile_multilib
}

func configArm64Lib(ctx android.LoadHookContext) (Ex_multilibType) {
    var srcs []string
    var module_name string = ctx.ModuleName()[9:] + ".so"
    var multilib Ex_multilibType
    var prefix64 string = ""
    var prefix32 string = ""
    if (ctx.ContainsProperty("optee")) {
        prefix64 = getOpteePrefix(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))
        prefix32 = prefix64
    }
    prefix64 += "arm64/"
    prefix32 += "arm/"
    if (ctx.ContainsProperty("vpu")) {
        prefix64 += getVpuPrefix(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))
        prefix32 += getVpuPrefix(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))
    }
    multilib.Lib32.Srcs = append(srcs, prefix32 + module_name)
    multilib.Lib64.Srcs = append(srcs, prefix64 + module_name)
    //fmt.Println("multilib.lib32.srcs:", multilib.Lib32.Srcs )
    //fmt.Println("multilib.lib64.srcs:", multilib.Lib64.Srcs)
    return multilib
}

func configArmLib(ctx android.LoadHookContext) ([]string) {
    var srcs []string
    //fmt.Println("TARGET_PRODUCT:", ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))
    var prefix string = ""
    var module_name string = ctx.ModuleName()[9:] + ".so"
    if (ctx.ContainsProperty("optee")) {
        prefix = getOpteePrefix(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))
    }
    prefix += "arm/"
    if (ctx.ContainsProperty("vpu")) {
        prefix += getVpuPrefix(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))
    }
    srcs = append(srcs, prefix + module_name)
    //fmt.Println("srcs:", srcs)
    return srcs
}

func configArm64LibStatic(ctx android.LoadHookContext) (Ex_multilibType) {
    var srcs []string
    var module_name string = ctx.ModuleName()[9:] + ".a"
    var arch Ex_multilibType
    var prefix64 string = ""
    var prefix32 string = ""
    var suffix string = "."
    if (ctx.ContainsProperty("aiq")) {
	var platform = ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM")
	var rkaiq_list = []string{"rk356x", "rk3588"}
	if isContain(rkaiq_list, platform) {
		suffix += platform[2:]
	} else {
		suffix += "356x"
	}
    }
    prefix64 += "android/arm64/"
    prefix32 += "android/arm/"
    arch.Lib32.Srcs = append(srcs, prefix32 + module_name + suffix)
    arch.Lib64.Srcs = append(srcs, prefix64 + module_name + suffix)
    //fmt.Println("arch.arm.srcs:", arch.Lib32.Srcs )
    //fmt.Println("arch.arm64.srcs:", arch.Lib64.Srcs)
    return arch
}
