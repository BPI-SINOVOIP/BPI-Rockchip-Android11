#include "android/hardware/tests/msgq/1.0/TestMsgQ.vts.h"
#include <cutils/properties.h>
#include <fmq/MessageQueue.h>

using namespace android::hardware::tests::msgq::V1_0;
using namespace android::hardware;

#define TRACEFILEPREFIX "/data/local/tmp/"

namespace android {
namespace vts {
void profile____android__hardware__tests__msgq__V1_0__ITestMsgQ__EventFlagBits(VariableSpecificationMessage* arg_name,
::android::hardware::tests::msgq::V1_0::ITestMsgQ::EventFlagBits arg_val_name __attribute__((__unused__))) {
    arg_name->set_type(TYPE_ENUM);
    arg_name->mutable_scalar_value()->set_uint32_t(static_cast<uint32_t>(arg_val_name));
    arg_name->set_scalar_type("uint32_t");
}


void HIDL_INSTRUMENTATION_FUNCTION_android_hardware_tests_msgq_V1_0_ITestMsgQ(
        details::HidlInstrumentor::InstrumentationEvent event __attribute__((__unused__)),
        const char* package,
        const char* version,
        const char* interface,
        const char* method __attribute__((__unused__)),
        std::vector<void *> *args __attribute__((__unused__))) {
    if (strcmp(package, "android.hardware.tests.msgq") != 0) {
        LOG(WARNING) << "incorrect package. Expect: android.hardware.tests.msgq actual: " << package;
    }
    std::string version_str = std::string(version);
    int major_version = stoi(version_str.substr(0, version_str.find('.')));
    int minor_version = stoi(version_str.substr(version_str.find('.') + 1));
    if (major_version != 1 || minor_version > 0) {
        LOG(WARNING) << "incorrect version. Expect: 1.0 or lower (if version != x.0), actual: " << version;
    }
    if (strcmp(interface, "ITestMsgQ") != 0) {
        LOG(WARNING) << "incorrect interface. Expect: ITestMsgQ actual: " << interface;
    }

    VtsProfilingInterface& profiler = VtsProfilingInterface::getInstance(TRACEFILEPREFIX);

    bool profiling_for_args = property_get_bool("hal.instrumentation.profile.args", true);
    if (strcmp(method, "configureFmqSyncReadWrite") == 0) {
        FunctionSpecificationMessage msg;
        msg.set_name("configureFmqSyncReadWrite");
        if (profiling_for_args) {
            if (!args) {
                LOG(WARNING) << "no argument passed";
            } else {
                switch (event) {
                    case details::HidlInstrumentor::CLIENT_API_ENTRY:
                    case details::HidlInstrumentor::SERVER_API_ENTRY:
                    case details::HidlInstrumentor::PASSTHROUGH_ENTRY:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of arguments does not match. expect: 1, actual: " << (*args).size() << ", method name: configureFmqSyncReadWrite, event type: " << event;
                            break;
                        }
                        auto *arg_0 __attribute__((__unused__)) = msg.add_arg();
                        ::android::hardware::MQDescriptorSync<uint16_t> *arg_val_0 __attribute__((__unused__)) = reinterpret_cast<::android::hardware::MQDescriptorSync<uint16_t>*> ((*args)[0]);
                        if (arg_val_0 != nullptr) {
                            arg_0->set_type(TYPE_FMQ_SYNC);
                            MessageQueue<uint16_t, kSynchronizedReadWrite> arg_0_q((*arg_val_0), false);
                            if (arg_0_q.isValid()) {
                                for (int i = 0; i < (int)arg_0_q.availableToRead(); i++) {
                                    auto *arg_0_item_i = arg_0->add_fmq_value();
                                    uint16_t arg_0_result;
                                    arg_0_q.read(&arg_0_result);
                                    arg_0_q.write(&arg_0_result);
                                    arg_0_item_i->set_type(TYPE_SCALAR);
                                    arg_0_item_i->mutable_scalar_value()->set_uint16_t(arg_0_result);
                                }
                            }
                        } else {
                            LOG(WARNING) << "argument 0 is null.";
                        }
                        break;
                    }
                    case details::HidlInstrumentor::CLIENT_API_EXIT:
                    case details::HidlInstrumentor::SERVER_API_EXIT:
                    case details::HidlInstrumentor::PASSTHROUGH_EXIT:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of return values does not match. expect: 1, actual: " << (*args).size() << ", method name: configureFmqSyncReadWrite, event type: " << event;
                            break;
                        }
                        auto *result_0 __attribute__((__unused__)) = msg.add_return_type_hidl();
                        bool *result_val_0 __attribute__((__unused__)) = reinterpret_cast<bool*> ((*args)[0]);
                        if (result_val_0 != nullptr) {
                            result_0->set_type(TYPE_SCALAR);
                            result_0->mutable_scalar_value()->set_bool_t((*result_val_0));
                        } else {
                            LOG(WARNING) << "return value 0 is null.";
                        }
                        break;
                    }
                    default:
                    {
                        LOG(WARNING) << "not supported. ";
                        break;
                    }
                }
            }
        }
        profiler.AddTraceEvent(event, package, version, interface, msg);
    }
    if (strcmp(method, "getFmqUnsyncWrite") == 0) {
        FunctionSpecificationMessage msg;
        msg.set_name("getFmqUnsyncWrite");
        if (profiling_for_args) {
            if (!args) {
                LOG(WARNING) << "no argument passed";
            } else {
                switch (event) {
                    case details::HidlInstrumentor::CLIENT_API_ENTRY:
                    case details::HidlInstrumentor::SERVER_API_ENTRY:
                    case details::HidlInstrumentor::PASSTHROUGH_ENTRY:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of arguments does not match. expect: 1, actual: " << (*args).size() << ", method name: getFmqUnsyncWrite, event type: " << event;
                            break;
                        }
                        auto *arg_0 __attribute__((__unused__)) = msg.add_arg();
                        bool *arg_val_0 __attribute__((__unused__)) = reinterpret_cast<bool*> ((*args)[0]);
                        if (arg_val_0 != nullptr) {
                            arg_0->set_type(TYPE_SCALAR);
                            arg_0->mutable_scalar_value()->set_bool_t((*arg_val_0));
                        } else {
                            LOG(WARNING) << "argument 0 is null.";
                        }
                        break;
                    }
                    case details::HidlInstrumentor::CLIENT_API_EXIT:
                    case details::HidlInstrumentor::SERVER_API_EXIT:
                    case details::HidlInstrumentor::PASSTHROUGH_EXIT:
                    {
                        if ((*args).size() != 2) {
                            LOG(ERROR) << "Number of return values does not match. expect: 2, actual: " << (*args).size() << ", method name: getFmqUnsyncWrite, event type: " << event;
                            break;
                        }
                        auto *result_0 __attribute__((__unused__)) = msg.add_return_type_hidl();
                        bool *result_val_0 __attribute__((__unused__)) = reinterpret_cast<bool*> ((*args)[0]);
                        if (result_val_0 != nullptr) {
                            result_0->set_type(TYPE_SCALAR);
                            result_0->mutable_scalar_value()->set_bool_t((*result_val_0));
                        } else {
                            LOG(WARNING) << "return value 0 is null.";
                        }
                        auto *result_1 __attribute__((__unused__)) = msg.add_return_type_hidl();
                        ::android::hardware::MQDescriptorUnsync<uint16_t> *result_val_1 __attribute__((__unused__)) = reinterpret_cast<::android::hardware::MQDescriptorUnsync<uint16_t>*> ((*args)[1]);
                        if (result_val_1 != nullptr) {
                            result_1->set_type(TYPE_FMQ_UNSYNC);
                            MessageQueue<uint16_t, kUnsynchronizedWrite> result_1_q((*result_val_1));
                            if (result_1_q.isValid()) {
                                for (int i = 0; i < (int)result_1_q.availableToRead(); i++) {
                                    auto *result_1_item_i = result_1->add_fmq_value();
                                    uint16_t result_1_result;
                                    result_1_q.read(&result_1_result);
                                    result_1_item_i->set_type(TYPE_SCALAR);
                                    result_1_item_i->mutable_scalar_value()->set_uint16_t(result_1_result);
                                }
                            }
                        } else {
                            LOG(WARNING) << "return value 1 is null.";
                        }
                        break;
                    }
                    default:
                    {
                        LOG(WARNING) << "not supported. ";
                        break;
                    }
                }
            }
        }
        profiler.AddTraceEvent(event, package, version, interface, msg);
    }
    if (strcmp(method, "requestWriteFmqSync") == 0) {
        FunctionSpecificationMessage msg;
        msg.set_name("requestWriteFmqSync");
        if (profiling_for_args) {
            if (!args) {
                LOG(WARNING) << "no argument passed";
            } else {
                switch (event) {
                    case details::HidlInstrumentor::CLIENT_API_ENTRY:
                    case details::HidlInstrumentor::SERVER_API_ENTRY:
                    case details::HidlInstrumentor::PASSTHROUGH_ENTRY:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of arguments does not match. expect: 1, actual: " << (*args).size() << ", method name: requestWriteFmqSync, event type: " << event;
                            break;
                        }
                        auto *arg_0 __attribute__((__unused__)) = msg.add_arg();
                        int32_t *arg_val_0 __attribute__((__unused__)) = reinterpret_cast<int32_t*> ((*args)[0]);
                        if (arg_val_0 != nullptr) {
                            arg_0->set_type(TYPE_SCALAR);
                            arg_0->mutable_scalar_value()->set_int32_t((*arg_val_0));
                        } else {
                            LOG(WARNING) << "argument 0 is null.";
                        }
                        break;
                    }
                    case details::HidlInstrumentor::CLIENT_API_EXIT:
                    case details::HidlInstrumentor::SERVER_API_EXIT:
                    case details::HidlInstrumentor::PASSTHROUGH_EXIT:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of return values does not match. expect: 1, actual: " << (*args).size() << ", method name: requestWriteFmqSync, event type: " << event;
                            break;
                        }
                        auto *result_0 __attribute__((__unused__)) = msg.add_return_type_hidl();
                        bool *result_val_0 __attribute__((__unused__)) = reinterpret_cast<bool*> ((*args)[0]);
                        if (result_val_0 != nullptr) {
                            result_0->set_type(TYPE_SCALAR);
                            result_0->mutable_scalar_value()->set_bool_t((*result_val_0));
                        } else {
                            LOG(WARNING) << "return value 0 is null.";
                        }
                        break;
                    }
                    default:
                    {
                        LOG(WARNING) << "not supported. ";
                        break;
                    }
                }
            }
        }
        profiler.AddTraceEvent(event, package, version, interface, msg);
    }
    if (strcmp(method, "requestReadFmqSync") == 0) {
        FunctionSpecificationMessage msg;
        msg.set_name("requestReadFmqSync");
        if (profiling_for_args) {
            if (!args) {
                LOG(WARNING) << "no argument passed";
            } else {
                switch (event) {
                    case details::HidlInstrumentor::CLIENT_API_ENTRY:
                    case details::HidlInstrumentor::SERVER_API_ENTRY:
                    case details::HidlInstrumentor::PASSTHROUGH_ENTRY:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of arguments does not match. expect: 1, actual: " << (*args).size() << ", method name: requestReadFmqSync, event type: " << event;
                            break;
                        }
                        auto *arg_0 __attribute__((__unused__)) = msg.add_arg();
                        int32_t *arg_val_0 __attribute__((__unused__)) = reinterpret_cast<int32_t*> ((*args)[0]);
                        if (arg_val_0 != nullptr) {
                            arg_0->set_type(TYPE_SCALAR);
                            arg_0->mutable_scalar_value()->set_int32_t((*arg_val_0));
                        } else {
                            LOG(WARNING) << "argument 0 is null.";
                        }
                        break;
                    }
                    case details::HidlInstrumentor::CLIENT_API_EXIT:
                    case details::HidlInstrumentor::SERVER_API_EXIT:
                    case details::HidlInstrumentor::PASSTHROUGH_EXIT:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of return values does not match. expect: 1, actual: " << (*args).size() << ", method name: requestReadFmqSync, event type: " << event;
                            break;
                        }
                        auto *result_0 __attribute__((__unused__)) = msg.add_return_type_hidl();
                        bool *result_val_0 __attribute__((__unused__)) = reinterpret_cast<bool*> ((*args)[0]);
                        if (result_val_0 != nullptr) {
                            result_0->set_type(TYPE_SCALAR);
                            result_0->mutable_scalar_value()->set_bool_t((*result_val_0));
                        } else {
                            LOG(WARNING) << "return value 0 is null.";
                        }
                        break;
                    }
                    default:
                    {
                        LOG(WARNING) << "not supported. ";
                        break;
                    }
                }
            }
        }
        profiler.AddTraceEvent(event, package, version, interface, msg);
    }
    if (strcmp(method, "requestWriteFmqUnsync") == 0) {
        FunctionSpecificationMessage msg;
        msg.set_name("requestWriteFmqUnsync");
        if (profiling_for_args) {
            if (!args) {
                LOG(WARNING) << "no argument passed";
            } else {
                switch (event) {
                    case details::HidlInstrumentor::CLIENT_API_ENTRY:
                    case details::HidlInstrumentor::SERVER_API_ENTRY:
                    case details::HidlInstrumentor::PASSTHROUGH_ENTRY:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of arguments does not match. expect: 1, actual: " << (*args).size() << ", method name: requestWriteFmqUnsync, event type: " << event;
                            break;
                        }
                        auto *arg_0 __attribute__((__unused__)) = msg.add_arg();
                        int32_t *arg_val_0 __attribute__((__unused__)) = reinterpret_cast<int32_t*> ((*args)[0]);
                        if (arg_val_0 != nullptr) {
                            arg_0->set_type(TYPE_SCALAR);
                            arg_0->mutable_scalar_value()->set_int32_t((*arg_val_0));
                        } else {
                            LOG(WARNING) << "argument 0 is null.";
                        }
                        break;
                    }
                    case details::HidlInstrumentor::CLIENT_API_EXIT:
                    case details::HidlInstrumentor::SERVER_API_EXIT:
                    case details::HidlInstrumentor::PASSTHROUGH_EXIT:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of return values does not match. expect: 1, actual: " << (*args).size() << ", method name: requestWriteFmqUnsync, event type: " << event;
                            break;
                        }
                        auto *result_0 __attribute__((__unused__)) = msg.add_return_type_hidl();
                        bool *result_val_0 __attribute__((__unused__)) = reinterpret_cast<bool*> ((*args)[0]);
                        if (result_val_0 != nullptr) {
                            result_0->set_type(TYPE_SCALAR);
                            result_0->mutable_scalar_value()->set_bool_t((*result_val_0));
                        } else {
                            LOG(WARNING) << "return value 0 is null.";
                        }
                        break;
                    }
                    default:
                    {
                        LOG(WARNING) << "not supported. ";
                        break;
                    }
                }
            }
        }
        profiler.AddTraceEvent(event, package, version, interface, msg);
    }
    if (strcmp(method, "requestReadFmqUnsync") == 0) {
        FunctionSpecificationMessage msg;
        msg.set_name("requestReadFmqUnsync");
        if (profiling_for_args) {
            if (!args) {
                LOG(WARNING) << "no argument passed";
            } else {
                switch (event) {
                    case details::HidlInstrumentor::CLIENT_API_ENTRY:
                    case details::HidlInstrumentor::SERVER_API_ENTRY:
                    case details::HidlInstrumentor::PASSTHROUGH_ENTRY:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of arguments does not match. expect: 1, actual: " << (*args).size() << ", method name: requestReadFmqUnsync, event type: " << event;
                            break;
                        }
                        auto *arg_0 __attribute__((__unused__)) = msg.add_arg();
                        int32_t *arg_val_0 __attribute__((__unused__)) = reinterpret_cast<int32_t*> ((*args)[0]);
                        if (arg_val_0 != nullptr) {
                            arg_0->set_type(TYPE_SCALAR);
                            arg_0->mutable_scalar_value()->set_int32_t((*arg_val_0));
                        } else {
                            LOG(WARNING) << "argument 0 is null.";
                        }
                        break;
                    }
                    case details::HidlInstrumentor::CLIENT_API_EXIT:
                    case details::HidlInstrumentor::SERVER_API_EXIT:
                    case details::HidlInstrumentor::PASSTHROUGH_EXIT:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of return values does not match. expect: 1, actual: " << (*args).size() << ", method name: requestReadFmqUnsync, event type: " << event;
                            break;
                        }
                        auto *result_0 __attribute__((__unused__)) = msg.add_return_type_hidl();
                        bool *result_val_0 __attribute__((__unused__)) = reinterpret_cast<bool*> ((*args)[0]);
                        if (result_val_0 != nullptr) {
                            result_0->set_type(TYPE_SCALAR);
                            result_0->mutable_scalar_value()->set_bool_t((*result_val_0));
                        } else {
                            LOG(WARNING) << "return value 0 is null.";
                        }
                        break;
                    }
                    default:
                    {
                        LOG(WARNING) << "not supported. ";
                        break;
                    }
                }
            }
        }
        profiler.AddTraceEvent(event, package, version, interface, msg);
    }
    if (strcmp(method, "requestBlockingRead") == 0) {
        FunctionSpecificationMessage msg;
        msg.set_name("requestBlockingRead");
        if (profiling_for_args) {
            if (!args) {
                LOG(WARNING) << "no argument passed";
            } else {
                switch (event) {
                    case details::HidlInstrumentor::CLIENT_API_ENTRY:
                    case details::HidlInstrumentor::SERVER_API_ENTRY:
                    case details::HidlInstrumentor::PASSTHROUGH_ENTRY:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of arguments does not match. expect: 1, actual: " << (*args).size() << ", method name: requestBlockingRead, event type: " << event;
                            break;
                        }
                        auto *arg_0 __attribute__((__unused__)) = msg.add_arg();
                        int32_t *arg_val_0 __attribute__((__unused__)) = reinterpret_cast<int32_t*> ((*args)[0]);
                        if (arg_val_0 != nullptr) {
                            arg_0->set_type(TYPE_SCALAR);
                            arg_0->mutable_scalar_value()->set_int32_t((*arg_val_0));
                        } else {
                            LOG(WARNING) << "argument 0 is null.";
                        }
                        break;
                    }
                    case details::HidlInstrumentor::CLIENT_API_EXIT:
                    case details::HidlInstrumentor::SERVER_API_EXIT:
                    case details::HidlInstrumentor::PASSTHROUGH_EXIT:
                    {
                        if ((*args).size() != 0) {
                            LOG(ERROR) << "Number of return values does not match. expect: 0, actual: " << (*args).size() << ", method name: requestBlockingRead, event type: " << event;
                            break;
                        }
                        break;
                    }
                    default:
                    {
                        LOG(WARNING) << "not supported. ";
                        break;
                    }
                }
            }
        }
        profiler.AddTraceEvent(event, package, version, interface, msg);
    }
    if (strcmp(method, "requestBlockingReadDefaultEventFlagBits") == 0) {
        FunctionSpecificationMessage msg;
        msg.set_name("requestBlockingReadDefaultEventFlagBits");
        if (profiling_for_args) {
            if (!args) {
                LOG(WARNING) << "no argument passed";
            } else {
                switch (event) {
                    case details::HidlInstrumentor::CLIENT_API_ENTRY:
                    case details::HidlInstrumentor::SERVER_API_ENTRY:
                    case details::HidlInstrumentor::PASSTHROUGH_ENTRY:
                    {
                        if ((*args).size() != 1) {
                            LOG(ERROR) << "Number of arguments does not match. expect: 1, actual: " << (*args).size() << ", method name: requestBlockingReadDefaultEventFlagBits, event type: " << event;
                            break;
                        }
                        auto *arg_0 __attribute__((__unused__)) = msg.add_arg();
                        int32_t *arg_val_0 __attribute__((__unused__)) = reinterpret_cast<int32_t*> ((*args)[0]);
                        if (arg_val_0 != nullptr) {
                            arg_0->set_type(TYPE_SCALAR);
                            arg_0->mutable_scalar_value()->set_int32_t((*arg_val_0));
                        } else {
                            LOG(WARNING) << "argument 0 is null.";
                        }
                        break;
                    }
                    case details::HidlInstrumentor::CLIENT_API_EXIT:
                    case details::HidlInstrumentor::SERVER_API_EXIT:
                    case details::HidlInstrumentor::PASSTHROUGH_EXIT:
                    {
                        if ((*args).size() != 0) {
                            LOG(ERROR) << "Number of return values does not match. expect: 0, actual: " << (*args).size() << ", method name: requestBlockingReadDefaultEventFlagBits, event type: " << event;
                            break;
                        }
                        break;
                    }
                    default:
                    {
                        LOG(WARNING) << "not supported. ";
                        break;
                    }
                }
            }
        }
        profiler.AddTraceEvent(event, package, version, interface, msg);
    }
    if (strcmp(method, "requestBlockingReadRepeat") == 0) {
        FunctionSpecificationMessage msg;
        msg.set_name("requestBlockingReadRepeat");
        if (profiling_for_args) {
            if (!args) {
                LOG(WARNING) << "no argument passed";
            } else {
                switch (event) {
                    case details::HidlInstrumentor::CLIENT_API_ENTRY:
                    case details::HidlInstrumentor::SERVER_API_ENTRY:
                    case details::HidlInstrumentor::PASSTHROUGH_ENTRY:
                    {
                        if ((*args).size() != 2) {
                            LOG(ERROR) << "Number of arguments does not match. expect: 2, actual: " << (*args).size() << ", method name: requestBlockingReadRepeat, event type: " << event;
                            break;
                        }
                        auto *arg_0 __attribute__((__unused__)) = msg.add_arg();
                        int32_t *arg_val_0 __attribute__((__unused__)) = reinterpret_cast<int32_t*> ((*args)[0]);
                        if (arg_val_0 != nullptr) {
                            arg_0->set_type(TYPE_SCALAR);
                            arg_0->mutable_scalar_value()->set_int32_t((*arg_val_0));
                        } else {
                            LOG(WARNING) << "argument 0 is null.";
                        }
                        auto *arg_1 __attribute__((__unused__)) = msg.add_arg();
                        int32_t *arg_val_1 __attribute__((__unused__)) = reinterpret_cast<int32_t*> ((*args)[1]);
                        if (arg_val_1 != nullptr) {
                            arg_1->set_type(TYPE_SCALAR);
                            arg_1->mutable_scalar_value()->set_int32_t((*arg_val_1));
                        } else {
                            LOG(WARNING) << "argument 1 is null.";
                        }
                        break;
                    }
                    case details::HidlInstrumentor::CLIENT_API_EXIT:
                    case details::HidlInstrumentor::SERVER_API_EXIT:
                    case details::HidlInstrumentor::PASSTHROUGH_EXIT:
                    {
                        if ((*args).size() != 0) {
                            LOG(ERROR) << "Number of return values does not match. expect: 0, actual: " << (*args).size() << ", method name: requestBlockingReadRepeat, event type: " << event;
                            break;
                        }
                        break;
                    }
                    default:
                    {
                        LOG(WARNING) << "not supported. ";
                        break;
                    }
                }
            }
        }
        profiler.AddTraceEvent(event, package, version, interface, msg);
    }
}

}  // namespace vts
}  // namespace android
