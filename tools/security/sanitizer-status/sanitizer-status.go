package sanitizer_status

import (
	"android/soong/android"
	"android/soong/cc"
)

func init() {
	android.RegisterModuleType("sanitizer_status_library_shared",
		libraryFactory)
}

func libraryFactory() android.Module {
	module := cc.LibrarySharedFactory()
	android.AddLoadHook(module, loadHook)
	return module
}

func loadHook(ctx android.LoadHookContext) {
	type props struct {
		Cflags []string
	}

	p := &props{}

	sanitizers := ctx.Config().SanitizeDevice()

	if android.InList("address", sanitizers) {
		p.Cflags = append(p.Cflags, "-DANDROID_SANITIZE_ADDRESS=1")
	}
	if android.InList("hwaddress", sanitizers) {
		p.Cflags = append(p.Cflags, "-DANDROID_SANITIZE_HWADDRESS=1")
	}
	if android.InList("coverage", sanitizers) {
		p.Cflags = append(p.Cflags, "-DANDROID_SANITIZE_COVERAGE=1")
	}

	ctx.AppendProperties(p)
}
