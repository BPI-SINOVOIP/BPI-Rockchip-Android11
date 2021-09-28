#include <binder/IBinder.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/LazyServiceRegistrar.h>
#include <utils/Log.h>
#include "LazyTestService.h"

using android::BBinder;
using android::IBinder;
using android::IPCThreadState;
using android::OK;
using android::sp;
using android::binder::LazyServiceRegistrar;
using android::binder::LazyTestService;

int main() {
  sp<LazyTestService> service1 = new LazyTestService();
  sp<LazyTestService> service2 = new LazyTestService();

  auto lazyRegistrar = LazyServiceRegistrar::getInstance();
  LOG_ALWAYS_FATAL_IF(OK != lazyRegistrar.registerService(service1, "aidl_lazy_test_1"), "");
  LOG_ALWAYS_FATAL_IF(OK != lazyRegistrar.registerService(service2, "aidl_lazy_test_2"), "");

  IPCThreadState::self()->joinThreadPool();

  return 1;
}
