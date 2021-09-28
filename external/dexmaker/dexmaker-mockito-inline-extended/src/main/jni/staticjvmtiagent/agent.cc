/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdlib>
#include <sstream>
#include <cstring>
#include <cassert>
#include <cstdarg>
#include <algorithm>

#include <jni.h>

#include "jvmti.h"

#include <slicer/dex_ir.h>
#include <slicer/code_ir.h>
#include <slicer/dex_ir_builder.h>
#include <slicer/dex_utf8.h>
#include <slicer/writer.h>
#include <slicer/reader.h>
#include <slicer/instrumentation.h>

using namespace dex;
using namespace lir;

namespace com_android_dx_mockito_inline {
    static jvmtiEnv* localJvmtiEnv;

    static jobject sTransformer;

    // Converts a class name to a type descriptor
    // (ex. "java.lang.String" to "Ljava/lang/String;")
    static std::string
    ClassNameToDescriptor(const char* class_name) {
        std::stringstream ss;
        ss << "L";
        for (auto p = class_name; *p != '\0'; ++p) {
            ss << (*p == '.' ? '/' : *p);
        }
        ss << ";";
        return ss.str();
    }

    // Takes the full dex file for class 'classBeingRedefined'
    // - isolates the dex code for the class out of the dex file
    // - calls sTransformer.runTransformers on the isolated dex code
    // - send the transformed code back to the runtime
    static void
    Transform(jvmtiEnv* jvmti_env,
              JNIEnv* env,
              jclass classBeingRedefined,
              jobject loader,
              const char* name,
              jobject protectionDomain,
              jint classDataLen,
              const unsigned char* classData,
              jint* newClassDataLen,
              unsigned char** newClassData) {
        if (sTransformer != nullptr) {
            // Even reading the classData array is expensive as the data is only generated when the
            // memory is touched. Hence call JvmtiAgent#shouldTransform to check if we need to transform
            // the class.
            jclass cls = env->GetObjectClass(sTransformer);
            jmethodID shouldTransformMethod = env->GetMethodID(cls, "shouldTransform",
                                                               "(Ljava/lang/Class;)Z");

            jboolean shouldTransform = env->CallBooleanMethod(sTransformer, shouldTransformMethod,
                                                              classBeingRedefined);
            if (!shouldTransform) {
                return;
            }

            // Isolate byte code of class class. This is needed as Android usually gives us more
            // than the class we need.
            Reader reader(classData, classDataLen);

            u4 index = reader.FindClassIndex(ClassNameToDescriptor(name).c_str());
            reader.CreateClassIr(index);
            std::shared_ptr<ir::DexFile> ir = reader.GetIr();

            struct Allocator : public Writer::Allocator {
                virtual void* Allocate(size_t size) {return ::malloc(size);}
                virtual void Free(void* ptr) {::free(ptr);}
            };

            Allocator allocator;
            Writer writer(ir);
            size_t isolatedClassLen = 0;
            std::shared_ptr<jbyte> isolatedClass((jbyte*)writer.CreateImage(&allocator,
                                                                            &isolatedClassLen));

            // Create jbyteArray with isolated byte code of class
            jbyteArray isolatedClassArr = env->NewByteArray(isolatedClassLen);
            env->SetByteArrayRegion(isolatedClassArr, 0, isolatedClassLen,
                                    isolatedClass.get());

            jstring nameStr = env->NewStringUTF(name);

            // Call JvmtiAgent#runTransformers
            jmethodID runTransformersMethod = env->GetMethodID(cls, "runTransformers",
                                                               "(Ljava/lang/ClassLoader;"
                                                               "Ljava/lang/String;"
                                                               "Ljava/lang/Class;"
                                                               "Ljava/security/ProtectionDomain;"
                                                               "[B)[B");

            jbyteArray transformedArr = (jbyteArray) env->CallObjectMethod(sTransformer,
                                                                           runTransformersMethod,
                                                                           loader, nameStr,
                                                                           classBeingRedefined,
                                                                           protectionDomain,
                                                                           isolatedClassArr);

            // Set transformed byte code
            if (!env->ExceptionOccurred() && transformedArr != nullptr) {
                *newClassDataLen = env->GetArrayLength(transformedArr);

                jbyte* transformed = env->GetByteArrayElements(transformedArr, 0);

                jvmti_env->Allocate(*newClassDataLen, newClassData);
                std::memcpy(*newClassData, transformed, *newClassDataLen);

                env->ReleaseByteArrayElements(transformedArr, transformed, 0);
            }
        }
    }

    // Add a label before instructionAfter
    static void
    addLabel(CodeIr& c,
             lir::Instruction* instructionAfter,
             Label* returnTrueLabel) {
        c.instructions.InsertBefore(instructionAfter, returnTrueLabel);
    }

    // Add a byte code before instructionAfter
    static void
    addInstr(CodeIr& c,
             lir::Instruction* instructionAfter,
             Opcode opcode,
             const std::list<Operand*>& operands) {
        auto instruction = c.Alloc<Bytecode>();

        instruction->opcode = opcode;

        for (auto it = operands.begin(); it != operands.end(); it++) {
            instruction->operands.push_back(*it);
        }

        c.instructions.InsertBefore(instructionAfter, instruction);
    }

    // Add a method call byte code before instructionAfter
    static void
    addCall(ir::Builder& b,
            CodeIr& c,
            lir::Instruction* instructionAfter,
            Opcode opcode,
            ir::Type* type,
            const char* methodName,
            ir::Type* returnType,
            const std::vector<ir::Type*>& types,
            const std::list<int>& regs) {
        auto proto = b.GetProto(returnType, b.GetTypeList(types));
        auto method = b.GetMethodDecl(b.GetAsciiString(methodName), proto, type);

        VRegList* param_regs = c.Alloc<VRegList>();
        for (auto it = regs.begin(); it != regs.end(); it++) {
            param_regs->registers.push_back(*it);
        }

        addInstr(c, instructionAfter, opcode, {param_regs, c.Alloc<Method>(method,
                                                                           method->orig_index)});
    }

    typedef struct {
        ir::Type* boxedType;
        ir::Type* scalarType;
        std::string unboxMethod;
    } BoxingInfo;

    // Get boxing / unboxing info for a type
    static BoxingInfo
    getBoxingInfo(ir::Builder &b,
                  char typeCode) {
        BoxingInfo boxingInfo;

        if (typeCode != 'L' && typeCode !=  '[') {
            std::stringstream tmp;
            tmp << typeCode;
            boxingInfo.scalarType = b.GetType(tmp.str().c_str());
        }

        switch (typeCode) {
            case 'B':
                boxingInfo.boxedType = b.GetType("Ljava/lang/Byte;");
                boxingInfo.unboxMethod = "byteValue";
                break;
            case 'S':
                boxingInfo.boxedType = b.GetType("Ljava/lang/Short;");
                boxingInfo.unboxMethod = "shortValue";
                break;
            case 'I':
                boxingInfo.boxedType = b.GetType("Ljava/lang/Integer;");
                boxingInfo.unboxMethod = "intValue";
                break;
            case 'C':
                boxingInfo.boxedType = b.GetType("Ljava/lang/Character;");
                boxingInfo.unboxMethod = "charValue";
                break;
            case 'F':
                boxingInfo.boxedType = b.GetType("Ljava/lang/Float;");
                boxingInfo.unboxMethod = "floatValue";
                break;
            case 'Z':
                boxingInfo.boxedType = b.GetType("Ljava/lang/Boolean;");
                boxingInfo.unboxMethod = "booleanValue";
                break;
            case 'J':
                boxingInfo.boxedType = b.GetType("Ljava/lang/Long;");
                boxingInfo.unboxMethod = "longValue";
                break;
            case 'D':
                boxingInfo.boxedType = b.GetType("Ljava/lang/Double;");
                boxingInfo.unboxMethod = "doubleValue";
                break;
            default:
                // real object
                break;
        }

        return boxingInfo;
    }

    static size_t
    getNumParams(ir::EncodedMethod *method) {
        if (method->decl->prototype->param_types == nullptr) {
            return 0;
        }

        return method->decl->prototype->param_types->types.size();
    }

    static bool
    canBeTransformed(ir::EncodedMethod *method) {
        std::string type = method->decl->parent->Decl();
        ir::String* methodName = method->decl->name;

        return ((method->access_flags & kAccStatic) != 0)
               && !(((method->access_flags & (kAccPrivate | kAccBridge | kAccNative)) != 0)
                    || (Utf8Cmp(methodName->c_str(), "<clinit>") == 0)
                    || (strncmp(type.c_str(), "java.", 5) == 0
                        && (method->access_flags & (kAccPrivate | kAccPublic | kAccProtected))
                           == 0));
    }

    // Transforms the classes to add the mockito hooks
    // - equals and hashcode are handled in a special way
    extern "C" JNIEXPORT jbyteArray JNICALL
    Java_com_android_dx_mockito_inline_StaticClassTransformer_nativeRedefine(JNIEnv* env,
                                                                           jobject generator,
                                                                           jstring idStr,
                                                                           jbyteArray originalArr) {
        unsigned char* original = (unsigned char*)env->GetByteArrayElements(originalArr, 0);

        Reader reader(original, env->GetArrayLength(originalArr));
        reader.CreateClassIr(0);
        std::shared_ptr<ir::DexFile> dex_ir = reader.GetIr();
        ir::Builder b(dex_ir);

        ir::Type* objectT = b.GetType("Ljava/lang/Object;");
        ir::Type* objectArrayT = b.GetType("[Ljava/lang/Object;");
        ir::Type* stringT = b.GetType("Ljava/lang/String;");
        ir::Type* methodT = b.GetType("Ljava/lang/reflect/Method;");
        ir::Type* callableT = b.GetType("Ljava/util/concurrent/Callable;");
        ir::Type* dispatcherT = b.GetType("Lcom/android/dx/mockito/inline/MockMethodDispatcher;");

        // Add id to dex file
        const char* idNative = env->GetStringUTFChars(idStr, 0);
        ir::String* id = b.GetAsciiString(idNative);
        env->ReleaseStringUTFChars(idStr, idNative);

        for (auto& method : dex_ir->encoded_methods) {
            if (!canBeTransformed(method.get())) {
                continue;
            }
            /*
            static long method_original(int param1, long param2, String param3) {
                foo();
                return bar();
            }

            static long method_transformed(int param1, long param2, String param3) {
                // MockMethodDispatcher dispatcher = MockMethodDispatcher.get(idStr, this);
                const-string v0, "65463hg34t"
                const v1, 0
                invoke-static {v0, v1}, MockMethodDispatcher.get(String, Object):MockMethodDispatcher
                move-result-object v0

                // if (dispatcher == null) {
                //    goto original_method;
                // }
                if-eqz v0, original_method

                // Method origin = dispatcher.getOrigin(this, methodDesc);
                const-string v1 "fully.qualified.ClassName#original_method(int, long, String)"
                const v2, 0
                invoke-virtual {v0, v2, v1}, MockMethodDispatcher.getOrigin(Object, String):Method
                move-result-object v1

                // if (origin == null) {
                //     goto original_method;
                // }
                if-eqz v1, original_method

                // Create an array with Objects of all parameters.

                //     Object[] arguments = new Object[3]
                const v3, 3
                new-array v2, v3, Object[]

                //     Integer param1Integer = Integer.valueOf(param1)
                move-from16 v3, ARG1     # this is necessary as invoke-static cannot deal with high
                                         # registers and ARG1 might be high
                invoke-static {v3}, Integer.valueOf(int):Integer
                move-result-object v3

                //     arguments[0] = param1Integer
                const v4, 0
                aput-object v3, v2, v4

                //     Long param2Long = Long.valueOf(param2)
                move-widefrom16 v3:v4, ARG2.1:ARG2.2 # this is necessary as invoke-static cannot
                                                     # deal with high registers and ARG2 might be
                                                     # high
                invoke-static {v3, v4}, Long.valueOf(long):Long
                move-result-object v3

                //     arguments[1] = param2Long
                const v4, 1
                aput-object v3, v2, v4

                //     arguments[2] = param3
                const v4, 2
                move-objectfrom16 v3, ARG3     # this is necessary as aput-object cannot deal with
                                               # high registers and ARG3 might be high
                aput-object v3, v2, v4

                // Callable<?> mocked = dispatcher.handle(methodDesc --as this parameter--,
                //                                        origin, arguments);
                const-string v3 "fully.qualified.ClassName#original_method(int, long, String)"
                invoke-virtual {v0,v3,v1,v2}, MockMethodDispatcher.handle(Object, Method,
                                                                          Object[]):Callable
                move-result-object v0

                //  if (mocked != null) {
                if-eqz v0, original_method

                //      Object ret = mocked.call();
                invoke-interface {v0}, Callable.call():Object
                move-result-object v0

                //      Long retLong = (Long)ret
                check-cast v0, Long

                //      long retlong = retLong.longValue();
                invoke-virtual {v0}, Long.longValue():long
                move-result-wide v0:v1

                //      return retlong;
                return-wide v0:v1

                //  }

            original_method:
                // Move all method arguments down so that they match what the original code expects.
                // Let's assume three arguments, one int, one long, one String and the and used to
                // use 4 registers
                move16 v5, v6             # ARG1
                move-wide16 v6:v7, v7:v8  # ARG2 (overlapping moves are allowed)
                move-object16 v8, v9      # ARG3

                // foo();
                // return bar();
                unmodified original byte code
            }
            */

            CodeIr c(method.get(), dex_ir);

            // Make sure there are at least 5 local registers to use
            int originalNumRegisters = method->code->registers - method->code->ins_count;
            int numAdditionalRegs = std::max(0, 5 - originalNumRegisters);
            int firstArg = originalNumRegisters + numAdditionalRegs;

            if (numAdditionalRegs > 0) {
                c.ir_method->code->registers += numAdditionalRegs;
            }

            lir::Instruction* fi = *(c.instructions.begin());

            // Add methodDesc to dex file
            std::stringstream ss;
            ss << method->decl->parent->Decl() << "#" << method->decl->name->c_str() << "(" ;
            bool first = true;
            if (method->decl->prototype->param_types != nullptr) {
                for (const auto& type : method->decl->prototype->param_types->types) {
                    if (first) {
                        first = false;
                    } else {
                        ss << ",";
                    }

                    ss << type->Decl().c_str();
                }
            }
            ss << ")";
            std::string methodDescStr = ss.str();
            ir::String* methodDesc = b.GetAsciiString(methodDescStr.c_str());

            size_t numParams = getNumParams(method.get());

            Label* originalMethodLabel = c.Alloc<Label>(0);
            CodeLocation* originalMethod = c.Alloc<CodeLocation>(originalMethodLabel);
            VReg* v0 = c.Alloc<VReg>(0);
            VReg* v1 = c.Alloc<VReg>(1);
            VReg* v2 = c.Alloc<VReg>(2);
            VReg* v3 = c.Alloc<VReg>(3);
            VReg* v4 = c.Alloc<VReg>(4);

            addInstr(c, fi, OP_CONST_STRING, {v0, c.Alloc<String>(id, id->orig_index)});
            addInstr(c, fi, OP_CONST, {v1, c.Alloc<Const32>(0)});
            addCall(b, c, fi, OP_INVOKE_STATIC, dispatcherT, "get", dispatcherT, {stringT, objectT},
                    {0, 1});
            addInstr(c, fi, OP_MOVE_RESULT_OBJECT, {v0});
            addInstr(c, fi, OP_IF_EQZ, {v0, originalMethod});
            addInstr(c, fi, OP_CONST_STRING,
                     {v1, c.Alloc<String>(methodDesc, methodDesc->orig_index)});
            addInstr(c, fi, OP_CONST, {v2, c.Alloc<Const32>(0)});
            addCall(b, c, fi, OP_INVOKE_VIRTUAL, dispatcherT, "getOrigin", methodT,
                    {objectT, stringT}, {0, 2, 1});
            addInstr(c, fi, OP_MOVE_RESULT_OBJECT, {v1});
            addInstr(c, fi, OP_IF_EQZ, {v1, originalMethod});
            addInstr(c, fi, OP_CONST, {v3, c.Alloc<Const32>(numParams)});
            addInstr(c, fi, OP_NEW_ARRAY, {v2, v3, c.Alloc<Type>(objectArrayT,
                                                                 objectArrayT->orig_index)});

            if (numParams > 0) {
                int argReg = firstArg;

                for (int argNum = 0; argNum < numParams; argNum++) {
                    const auto& type = method->decl->prototype->param_types->types[argNum];
                    BoxingInfo boxingInfo = getBoxingInfo(b, type->descriptor->c_str()[0]);

                    switch (type->GetCategory()) {
                        case ir::Type::Category::Scalar:
                            addInstr(c, fi, OP_MOVE_FROM16, {v3, c.Alloc<VReg>(argReg)});
                            addCall(b, c, fi, OP_INVOKE_STATIC, boxingInfo.boxedType, "valueOf",
                                    boxingInfo.boxedType, {type}, {3});
                            addInstr(c, fi, OP_MOVE_RESULT_OBJECT, {v3});

                            argReg++;
                            break;
                        case ir::Type::Category::WideScalar: {
                            VRegPair* v3v4 = c.Alloc<VRegPair>(3);
                            VRegPair* argRegPair = c.Alloc<VRegPair>(argReg);

                            addInstr(c, fi, OP_MOVE_WIDE_FROM16, {v3v4, argRegPair});
                            addCall(b, c, fi, OP_INVOKE_STATIC, boxingInfo.boxedType, "valueOf",
                                    boxingInfo.boxedType, {type}, {3, 4});
                            addInstr(c, fi, OP_MOVE_RESULT_OBJECT, {v3});

                            argReg += 2;
                            break;
                        }
                        case ir::Type::Category::Reference:
                            addInstr(c, fi, OP_MOVE_OBJECT_FROM16, {v3, c.Alloc<VReg>(argReg)});

                            argReg++;
                            break;
                        case ir::Type::Category::Void:
                            assert(false);
                    }

                    addInstr(c, fi, OP_CONST, {v4, c.Alloc<Const32>(argNum)});
                    addInstr(c, fi, OP_APUT_OBJECT, {v3, v2, v4});
                }
            }

            // NASTY Hack: Push in method name as "mock"
            addInstr(c, fi, OP_CONST_STRING,
                     {v3, c.Alloc<String>(methodDesc, methodDesc->orig_index)});
            addCall(b, c, fi, OP_INVOKE_VIRTUAL, dispatcherT, "handle", callableT,
                    {objectT, methodT, objectArrayT}, {0, 3, 1, 2});
            addInstr(c, fi, OP_MOVE_RESULT_OBJECT, {v0});
            addInstr(c, fi, OP_IF_EQZ, {v0, originalMethod});
            addCall(b, c, fi, OP_INVOKE_INTERFACE, callableT, "call", objectT, {}, {0});
            addInstr(c, fi, OP_MOVE_RESULT_OBJECT, {v0});

            ir::Type *returnType = method->decl->prototype->return_type;
            BoxingInfo boxingInfo = getBoxingInfo(b, returnType->descriptor->c_str()[0]);

            switch (returnType->GetCategory()) {
                case ir::Type::Category::Scalar:
                    addInstr(c, fi, OP_CHECK_CAST, {v0,
                                                    c.Alloc<Type>(boxingInfo.boxedType, boxingInfo.boxedType->orig_index)});
                    addCall(b, c, fi, OP_INVOKE_VIRTUAL, boxingInfo.boxedType,
                            boxingInfo.unboxMethod.c_str(), returnType, {}, {0});
                    addInstr(c, fi, OP_MOVE_RESULT, {v0});
                    addInstr(c, fi, OP_RETURN, {v0});
                    break;
                case ir::Type::Category::WideScalar: {
                    VRegPair* v0v1 = c.Alloc<VRegPair>(0);

                    addInstr(c, fi, OP_CHECK_CAST, {v0,
                                                    c.Alloc<Type>(boxingInfo.boxedType, boxingInfo.boxedType->orig_index)});
                    addCall(b, c, fi, OP_INVOKE_VIRTUAL, boxingInfo.boxedType,
                            boxingInfo.unboxMethod.c_str(), returnType, {}, {0});
                    addInstr(c, fi, OP_MOVE_RESULT_WIDE, {v0v1});
                    addInstr(c, fi, OP_RETURN_WIDE, {v0v1});
                    break;
                }
                case ir::Type::Category::Reference:
                    addInstr(c, fi, OP_CHECK_CAST, {v0, c.Alloc<Type>(returnType,
                                                                      returnType->orig_index)});
                    addInstr(c, fi, OP_RETURN_OBJECT, {v0});
                    break;
                case ir::Type::Category::Void:
                    addInstr(c, fi, OP_RETURN_VOID, {});
                    break;
            }

            addLabel(c, fi, originalMethodLabel);

            if (numParams > 0) {
                int argReg = firstArg;

                for (int argNum = 0; argNum < numParams; argNum++) {
                    const auto& type = method->decl->prototype->param_types->types[argNum];
                    int origReg = argReg - numAdditionalRegs;
                    switch (type->GetCategory()) {
                        case ir::Type::Category::Scalar:
                            addInstr(c, fi, OP_MOVE_16, {c.Alloc<VReg>(origReg),
                                                         c.Alloc<VReg>(argReg)});
                            argReg++;
                            break;
                        case ir::Type::Category::WideScalar:
                            addInstr(c, fi, OP_MOVE_WIDE_16,{c.Alloc<VRegPair>(origReg),
                                                             c.Alloc<VRegPair>(argReg)});
                            argReg +=2;
                            break;
                        case ir::Type::Category::Reference:
                            addInstr(c, fi, OP_MOVE_OBJECT_16, {c.Alloc<VReg>(origReg),
                                                                c.Alloc<VReg>(argReg)});
                            argReg++;
                            break;
                    }
                }
            }

            c.Assemble();
        }

        struct Allocator : public Writer::Allocator {
            virtual void* Allocate(size_t size) {return ::malloc(size);}
            virtual void Free(void* ptr) {::free(ptr);}
        };

        Allocator allocator;
        Writer writer(dex_ir);
        size_t transformedLen = 0;
        std::shared_ptr<jbyte> transformed((jbyte*)writer.CreateImage(&allocator, &transformedLen));

        jbyteArray transformedArr = env->NewByteArray(transformedLen);
        env->SetByteArrayRegion(transformedArr, 0, transformedLen, transformed.get());

        return transformedArr;
    }

    // Initializes the agent
    extern "C" jint Agent_OnAttach(JavaVM* vm,
                                   char* options,
                                   void* reserved) {
        jint jvmError = vm->GetEnv(reinterpret_cast<void**>(&localJvmtiEnv), JVMTI_VERSION_1_2);
        if (jvmError != JNI_OK) {
            return jvmError;
        }

        jvmtiCapabilities caps;
        memset(&caps, 0, sizeof(caps));
        caps.can_retransform_classes = 1;

        jvmtiError error = localJvmtiEnv->AddCapabilities(&caps);
        if (error != JVMTI_ERROR_NONE) {
            return error;
        }

        jvmtiEventCallbacks cb;
        memset(&cb, 0, sizeof(cb));
        cb.ClassFileLoadHook = Transform;

        error = localJvmtiEnv->SetEventCallbacks(&cb, sizeof(cb));
        if (error != JVMTI_ERROR_NONE) {
            return error;
        }

        error = localJvmtiEnv->SetEventNotificationMode(JVMTI_ENABLE,
                                                        JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
        if (error != JVMTI_ERROR_NONE) {
            return error;
        }

        return JVMTI_ERROR_NONE;
    }

    // Throw runtime exception
    static void throwRuntimeExpection(JNIEnv* env, const char* fmt, ...) {
        char msgBuf[512];

        va_list args;
        va_start (args, fmt);
        vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
        va_end (args);

        jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exceptionClass, msgBuf);
    }

    // Register transformer hook
    extern "C" JNIEXPORT void JNICALL
    Java_com_android_dx_mockito_inline_StaticJvmtiAgent_nativeRegisterTransformerHook(JNIEnv* env,
                                                                                     jobject thiz) {
        sTransformer = env->NewGlobalRef(thiz);
    }

    // Unregister transformer hook
    extern "C" JNIEXPORT void JNICALL
    Java_com_android_dx_mockito_inline_StaticJvmtiAgent_nativeUnregisterTransformerHook(JNIEnv* env,
                                                                                     jobject thiz) {
        env->DeleteGlobalRef(sTransformer);
        sTransformer = nullptr;
    }

    // Triggers retransformation of classes via this file's Transform method
    extern "C" JNIEXPORT void JNICALL
    Java_com_android_dx_mockito_inline_StaticJvmtiAgent_nativeRetransformClasses(JNIEnv* env,
                                                                             jobject thiz,
                                                                             jobjectArray classes) {
        jsize numTransformedClasses = env->GetArrayLength(classes);
        jclass *transformedClasses = (jclass*) malloc(numTransformedClasses * sizeof(jclass));
        for (int i = 0; i < numTransformedClasses; i++) {
            transformedClasses[i] = (jclass) env->NewGlobalRef(env->GetObjectArrayElement(classes, i));
        }

        jvmtiError error = localJvmtiEnv->RetransformClasses(numTransformedClasses,
                                                             transformedClasses);

        for (int i = 0; i < numTransformedClasses; i++) {
            env->DeleteGlobalRef(transformedClasses[i]);
        }
        free(transformedClasses);

        if (error != JVMTI_ERROR_NONE) {
            throwRuntimeExpection(env, "Could not retransform classes: %d", error);
        }
    }

    static jvmtiFrameInfo* frameToInspect;
    static std::string calledClass;

    // Takes the full dex file for class 'classBeingRedefined'
    // - isolates the dex code for the class out of the dex file
    // - calls sTransformer.runTransformers on the isolated dex code
    // - send the transformed code back to the runtime
    static void
    InspectClass(jvmtiEnv* jvmtiEnv,
                 JNIEnv* env,
                 jclass classBeingRedefined,
                 jobject loader,
                 const char* name,
                 jobject protectionDomain,
                 jint classDataLen,
                 const unsigned char* classData,
                 jint* newClassDataLen,
                 unsigned char** newClassData) {
        calledClass = "none";

        Reader reader(classData, classDataLen);

        char *calledMethodName;
        char *calledMethodSignature;
        jvmtiError error = jvmtiEnv->GetMethodName(frameToInspect->method, &calledMethodName,
                                                   &calledMethodSignature, nullptr);
        if (error != JVMTI_ERROR_NONE) {
            return;
        }

        u4 index = reader.FindClassIndex(ClassNameToDescriptor(name).c_str());
        reader.CreateClassIr(index);
        std::shared_ptr<ir::DexFile> class_ir = reader.GetIr();

        for (auto& method : class_ir->encoded_methods) {
            if (Utf8Cmp(method->decl->name->c_str(), calledMethodName) == 0
                && Utf8Cmp(method->decl->prototype->Signature().c_str(), calledMethodSignature) == 0) {
                CodeIr method_ir(method.get(), class_ir);

                for (auto instruction : method_ir.instructions) {
                    Bytecode* bytecode = dynamic_cast<Bytecode*>(instruction);
                    if (bytecode != nullptr && bytecode->offset == frameToInspect->location) {
                        Method *method = bytecode->CastOperand<Method>(1);
                        calledClass = method->ir_method->parent->Decl().c_str();

                        goto exit;
                    }
                }
            }
        }

        exit:
        free(calledMethodName);
        free(calledMethodSignature);
    }

#define GOTO_ON_ERROR(label) \
    if (error != JVMTI_ERROR_NONE) { \
        goto label; \
    }

// stack frame of the caller if method was called directly
#define DIRECT_CALL_STACK_FRAME (6)

// stack frame of the caller if method was called as 'real method'
#define REALMETHOD_CALL_STACK_FRAME (23)

    extern "C" JNIEXPORT jstring JNICALL
    Java_com_android_dx_mockito_inline_StaticMockMethodAdvice_nativeGetCalledClassName(JNIEnv* env,
                                                                            jclass klass,
                                                                            jthread currentThread) {

        JavaVM *vm;
        jint jvmError = env->GetJavaVM(&vm);
        if (jvmError != JNI_OK) {
            return nullptr;
        }

        jvmtiEnv *jvmtiEnv;
        jvmError = vm->GetEnv(reinterpret_cast<void**>(&jvmtiEnv), JVMTI_VERSION_1_2);
        if (jvmError != JNI_OK) {
            return nullptr;
        }

        jvmtiCapabilities caps;
        memset(&caps, 0, sizeof(caps));
        caps.can_retransform_classes = 1;

        jvmtiError error = jvmtiEnv->AddCapabilities(&caps);
        GOTO_ON_ERROR(unregister_env_and_exit);

        jvmtiEventCallbacks cb;
        memset(&cb, 0, sizeof(cb));
        cb.ClassFileLoadHook = InspectClass;

        jvmtiFrameInfo frameInfo[REALMETHOD_CALL_STACK_FRAME + 1];
        jint numFrames;
        error = jvmtiEnv->GetStackTrace(nullptr, 0, REALMETHOD_CALL_STACK_FRAME + 1, frameInfo,
                                        &numFrames);
        GOTO_ON_ERROR(unregister_env_and_exit);

        // Method might be called directly or as 'real method' (see
        // StaticMockMethodAdvice.SuperMethodCall#invoke). Hence the real caller might be in stack
        // frame DIRECT_CALL_STACK_FRAME for a direct call or REALMETHOD_CALL_STACK_FRAME for a
        // call through the 'real method' mechanism.
        int callingFrameNum;
        if (numFrames < REALMETHOD_CALL_STACK_FRAME) {
            callingFrameNum = DIRECT_CALL_STACK_FRAME;
        } else {
            char *directCallMethodName;

            jvmtiEnv->GetMethodName(frameInfo[DIRECT_CALL_STACK_FRAME].method,
                                    &directCallMethodName, nullptr, nullptr);
            if (strcmp(directCallMethodName, "invoke") == 0) {
                callingFrameNum = REALMETHOD_CALL_STACK_FRAME;
            } else {
                callingFrameNum = DIRECT_CALL_STACK_FRAME;
            }
        }

        jclass callingClass;
        error = jvmtiEnv->GetMethodDeclaringClass(frameInfo[callingFrameNum].method, &callingClass);
        GOTO_ON_ERROR(unregister_env_and_exit);

        error = jvmtiEnv->SetEventCallbacks(&cb, sizeof(cb));
        GOTO_ON_ERROR(unregister_env_and_exit);

        error = jvmtiEnv->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,
                                                   currentThread);
        GOTO_ON_ERROR(unset_cb_and_exit);

        frameToInspect = &frameInfo[callingFrameNum];
        error = jvmtiEnv->RetransformClasses(1, &callingClass);
        GOTO_ON_ERROR(disable_hook_and_exit);

        disable_hook_and_exit:
        jvmtiEnv->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,
                                           currentThread);

        unset_cb_and_exit:
        memset(&cb, 0, sizeof(cb));
        jvmtiEnv->SetEventCallbacks(&cb, sizeof(cb));

        unregister_env_and_exit:
        jvmtiEnv->DisposeEnvironment();

        if (error != JVMTI_ERROR_NONE) {
            return nullptr;
        }

        return env->NewStringUTF(calledClass.c_str());
    }

}  // namespace com_android_dx_mockito_inline

