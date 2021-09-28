%% template file for generating NeuralNetworks.h.
%% see README.md.
/*
 * Copyright (C) 2017 The Android Open Source Project
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

/**
 * @addtogroup NeuralNetworks
 * @{
 */

/**
 * @file NeuralNetworks.h
 */

#ifndef ANDROID_FRAMEWORKS_ML_NN_RUNTIME_NEURAL_NETWORKS_H
#define ANDROID_FRAMEWORKS_ML_NN_RUNTIME_NEURAL_NETWORKS_H

/******************************************************************
 *
 * IMPORTANT NOTICE:
 *
 *   This file is part of Android's set of stable system headers
 *   exposed by the Android NDK (Native Development Kit).
 *
 *   Third-party source AND binary code relies on the definitions
 *   here to be FROZEN ON ALL UPCOMING PLATFORM RELEASES.
 *
 *   - DO NOT MODIFY ENUMS (EXCEPT IF YOU ADD NEW 32-BIT VALUES)
 *   - DO NOT MODIFY CONSTANTS OR FUNCTIONAL MACROS
 *   - DO NOT CHANGE THE SIGNATURE OF FUNCTIONS IN ANY WAY
 *   - DO NOT CHANGE THE LAYOUT OR SIZE OF STRUCTURES
 */

#include <android/hardware_buffer.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

%insert Operand_1.0_Comment
typedef enum {
%insert Operand_1.0
%insert Operand_1.2
%insert Operand_1.3
} OperandCode;

%insert Operation_1.0_Comment
typedef enum {
    // Operations below are available since API level 27.

%insert Operation_1.0

    // Operations below are available since API level 28.

%insert Operation_1.1

    // Operations below are available since API level 29.

%insert Operation_1.2

    // Operations below are available since API level 30.

%insert Operation_1.3
} OperationCode;

/**
 * Fused activation function types.
 *
 *
 * Available since API level 27.
 */
typedef enum {
    /** NO fused activation function. */
    ANEURALNETWORKS_FUSED_NONE = 0,
    /** Fused ReLU activation function. */
    ANEURALNETWORKS_FUSED_RELU = 1,
    /** Fused ReLU1 activation function. */
    ANEURALNETWORKS_FUSED_RELU1 = 2,
    /** Fused ReLU6 activation function. */
    ANEURALNETWORKS_FUSED_RELU6 = 3,
} FuseCode;

/**
 * Implicit padding algorithms.
 *
 *
 * Available since API level 27.
 */
typedef enum {
    /**
     * SAME padding.
     * Padding on both ends are the "same":
     *     padding_to_beginning =  total_padding / 2
     *     padding_to_end       = (total_padding + 1)/2.
     * i.e., for even number of padding, padding to both ends are exactly
     * the same; for odd number of padding, padding to the ending is bigger
     * than the padding to the beginning by 1.
     *
     * total_padding is a function of input, stride, dilation and filter size.
     * It could be computed as follows:
     *    out_size = (input + stride - 1) / stride
     *    effective_filter_size = (filter_size - 1) * dilation + 1
     *    needed_input = (out_size - 1) * stride + effective_filter_size
     *    total_padding = max(0, needed_input - input_size)
     *  The computation is the same for the horizontal and vertical directions.
     */
    ANEURALNETWORKS_PADDING_SAME = 1,

    /**
     * VALID padding.
     * No padding. When the input size is not evenly divisible by
     * the filter size, the input at the end that could not fill
     * the whole filter tile will simply be ignored.
     */
    ANEURALNETWORKS_PADDING_VALID = 2,
} PaddingCode;

/**
 * Execution preferences.
 *
 * Available since API level 27.
 */
typedef enum {
    /**
     * Prefer executing in a way that minimizes battery drain.
     * This is desirable for compilations that will be executed often.
     */
    ANEURALNETWORKS_PREFER_LOW_POWER = 0,
    /**
     * Prefer returning a single answer as fast as possible, even if this causes
     * more power consumption.
     */
    ANEURALNETWORKS_PREFER_FAST_SINGLE_ANSWER = 1,
    /**
     * Prefer maximizing the throughput of successive frames, for example when
     * processing successive frames coming from the camera.
     */
    ANEURALNETWORKS_PREFER_SUSTAINED_SPEED = 2,
} PreferenceCode;

/**
 * Device types.
 *
 * The type of NNAPI device.
 */
typedef enum {
    /** The device type cannot be provided. */
    ANEURALNETWORKS_DEVICE_UNKNOWN = 0,
    /** The device does not fall into any category below. */
    ANEURALNETWORKS_DEVICE_OTHER = 1,
    /** The device runs NNAPI models on single or multi-core CPU. */
    ANEURALNETWORKS_DEVICE_CPU = 2,
    /** The device can run NNAPI models and also accelerate graphics APIs such
     * as OpenGL ES and Vulkan. */
    ANEURALNETWORKS_DEVICE_GPU = 3,
    /** Dedicated accelerator for Machine Learning workloads. */
    ANEURALNETWORKS_DEVICE_ACCELERATOR = 4,
} DeviceTypeCode;

/**
 * Result codes.
 *
 * <p>Any NNAPI function can return any result code, including result codes not
 * currently documented. Any value other than {@link ANEURALNETWORKS_NO_ERROR}
 * indicates a failure of some kind.</p>
 *
 * <p>Additional information about the nature of a failure can be obtained from
 * the device log after enabling NNAPI debugging by setting the debug.nn.vlog
 * property to 1, e.g., by calling "adb shell setprop debug.nn.vlog 1".</p>
 *
 * Available since API level 27.
 */
typedef enum {
    /**
     * Operation was succesful.
     */
    ANEURALNETWORKS_NO_ERROR = 0,

    /**
     * Failure caused by not enough available memory.
     */
    ANEURALNETWORKS_OUT_OF_MEMORY = 1,

    ANEURALNETWORKS_INCOMPLETE = 2,

    /**
     * Failure caused by unexpected null argument.
     */
    ANEURALNETWORKS_UNEXPECTED_NULL = 3,

    /**
     * Failure caused by invalid function arguments, invalid model definition,
     * invalid execution definition or invalid data at execution time.
     */
    ANEURALNETWORKS_BAD_DATA = 4,

    /**
     * Failure caused by failed model execution.
     */
    ANEURALNETWORKS_OP_FAILED = 5,

    /**
     * Failure caused by object being in the wrong state.
     */
    ANEURALNETWORKS_BAD_STATE = 6,

    /**
     * Failure caused by not being able to map a file into memory.
     * This may be caused by a file descriptor not being mappable, or an AHardwareBuffer
     * not supported by the device.
     * Mitigate by reading its content into memory.
     */
    ANEURALNETWORKS_UNMAPPABLE = 7,

    /**
     * Failure caused by insufficient buffer size provided to a model output.
     */
    ANEURALNETWORKS_OUTPUT_INSUFFICIENT_SIZE = 8,

    /**
     * Failure caused by a device not being available.
     */
    ANEURALNETWORKS_UNAVAILABLE_DEVICE = 9,

    /**
     * Failure because a deadline could not be met for a task, but future
     * deadlines may still be met for the same task after a short delay.
     *
     * Available since API level 30.
     */
    ANEURALNETWORKS_MISSED_DEADLINE_TRANSIENT = 10,

    /**
     * Failure because a deadline could not be met for a task, and future
     * deadlines will likely also not be met for the same task even after a
     * short delay.
     *
     * Available since API level 30.
     */
    ANEURALNETWORKS_MISSED_DEADLINE_PERSISTENT = 11,

    /**
     * Failure because of a resource limitation within the driver, but future
     * calls for the same task may still succeed after a short delay.
     *
     * Available since API level 30.
     */
    ANEURALNETWORKS_RESOURCE_EXHAUSTED_TRANSIENT = 12,

    /**
     * Failure because of a resource limitation within the driver, and future
     * calls for the same task will likely also fail even after a short
     * delay.
     *
     * Available since API level 30.
     */
    ANEURALNETWORKS_RESOURCE_EXHAUSTED_PERSISTENT = 13,

    /**
     * Failure indicating an object is in a dead state.
     *
     * Available since API level 30.
     */
    ANEURALNETWORKS_DEAD_OBJECT = 14,
} ResultCode;

/**
 * For {@link ANeuralNetworksModel_setOperandValue}, values with a
 * length smaller or equal to this will be immediately copied into
 * the model. The size is in bytes.
 *
 * Available since API level 27.
 */
enum { ANEURALNETWORKS_MAX_SIZE_OF_IMMEDIATELY_COPIED_VALUES = 128 };

/**
 * For {@link ANeuralNetworksCompilation_setCaching}, specify the size
 * of the cache token required from the application. The size is in bytes.
 *
 * Available since API level 29.
 */
enum { ANEURALNETWORKS_BYTE_SIZE_OF_CACHE_TOKEN = 32 };

/**
 * Different duration measurements.
 *
 * Durations are measured in nanoseconds.
 *
 * Available since API level 29.
 */
typedef enum {
    // Execution time on hardware (not driver, which runs on host processor).
    ANEURALNETWORKS_DURATION_ON_HARDWARE = 0,
    // Execution time in driver (including time on hardware).  Excludes overhead
    // such as that of the runtime itself and the IPC needed for the runtime to
    // communicate with the driver.
    ANEURALNETWORKS_DURATION_IN_DRIVER = 1,
    // Execution time on hardware, after all dependencies have been signaled.
    // If no dependencies specified (for example, if the execution was scheduled other
    // than with {@link ANeuralNetworksExecution_startComputeWithDependencies}), the
    // reported time will be the same as ANEURALNETWORKS_DURATION_ON_HARDWARE.
    // Available since API level 30.
    ANEURALNETWORKS_FENCED_DURATION_ON_HARDWARE = 2,
    // Execution time in driver, after all dependencies have been signaled. Excludes
    // overhead such as that of the runtime itself and the IPC needed for the runtime
    // to communicate with the driver.
    // If no dependencies specified (for example, if the execution was scheduled other
    // than with {@link ANeuralNetworksExecution_startComputeWithDependencies}), the
    // reported time will be the same as ANEURALNETWORKS_DURATION_IN_DRIVER.
    // Available since API level 30.
    ANEURALNETWORKS_FENCED_DURATION_IN_DRIVER = 3,
} DurationCode;

/**
 * Relative execution priority.
 *
 * Available since API level 30.
 */
typedef enum {
    ANEURALNETWORKS_PRIORITY_LOW = 90,
    ANEURALNETWORKS_PRIORITY_MEDIUM = 100,
    ANEURALNETWORKS_PRIORITY_HIGH = 110,
    ANEURALNETWORKS_PRIORITY_DEFAULT = ANEURALNETWORKS_PRIORITY_MEDIUM,
} PriorityCode;

/**
 * ANeuralNetworksMemory is an opaque type that represents memory.
 *
 * This type is used to represent shared memory, memory mapped files,
 * and similar memories.
 *
 * By using shared memory, a program can efficiently communicate to the
 * runtime and drivers the tensors that define a model. See
 * {@link ANeuralNetworksModel_setOperandValueFromMemory}. An application
 * should typically create one shared memory object that contains every constant tensor
 * needed to define a model. {@link ANeuralNetworksMemory_createFromFd} can be used to
 * create shared memory from a file handle.
 * {@link ANeuralNetworksMemory_createFromAHardwareBuffer} can be used to
 * create shared memory from an AHardwareBuffer handle.
 *
 * Memory objects can also be used to specify the input and output arguments of
 * an execution. See {@link ANeuralNetworksExecution_setInputFromMemory}
 * and {@link ANeuralNetworksExecution_setOutputFromMemory}.
 *
 * When calling {@link ANeuralNetworksModel_setOperandValueFromMemory},
 * {@link ANeuralNetworksExecution_setInputFromMemory} and
 * {@link ANeuralNetworksExecution_setOutputFromMemory}, each operand in the shared
 * memory object must be aligned on a boundary of a byte size that is a multiple
 * of the element type byte size, e.g., a tensor with
 * {@link ANEURALNETWORKS_TENSOR_FLOAT32} type must be aligned on 4-byte boundary.
 *
 * It is the application's responsibility to ensure that there are no uses of
 * the memory after calling {@link ANeuralNetworksMemory_free}. This includes
 * any model which references this memory because of a call to
 * {@link ANeuralNetworksModel_setOperandValueFromMemory}, any compilation
 * created using such a model, any execution object or burst object created
 * using such a compilation, or any execution which references this memory
 * because of a call to {@link ANeuralNetworksExecution_setInputFromMemory} or
 * {@link ANeuralNetworksExecution_setOutputFromMemory}.
 *
 * Available since API level 27.
 *
 * Starting at API level 30, the application may request creation of device native memory from
 * {@link ANeuralNetworksMemoryDesc} to avoid potential memory copying and transformation
 * overhead between executions. See also {@link ANeuralNetworksMemoryDesc} and
 * {@link ANeuralNetworksMemory_createFromDesc}.
 */
typedef struct ANeuralNetworksMemory ANeuralNetworksMemory;

/**
 * ANeuralNetworksModel is an opaque type that contains a description of the
 * mathematical operations that constitute the model.
 *
 * <p>Build the model by calling<ul>
 * <li>{@link ANeuralNetworksModel_create}</li>
 * <li>{@link ANeuralNetworksModel_addOperation}</li>
 * <li>{@link ANeuralNetworksModel_addOperand}</li>
 * </ul>
 *
 * This forms a graph in which each operation and operand is a node, a
 * directed edge from an operand to an operation indicates that the
 * operand is an input to the operation, and a directed edge from an
 * operation to an operand indicates that the operand is an output
 * from the operation. This graph must be acyclic.
 *
 * A model is completed by calling {@link ANeuralNetworksModel_finish}.
 * A model is destroyed by calling {@link ANeuralNetworksModel_free}.
 *
 * <p>A model cannot be modified once {@link ANeuralNetworksModel_finish}
 * has been called on it.</p>
 *
 * <p>It is the application's responsibility to make sure that only one thread
 * modifies a model at a given time. It is however safe for more than one
 * thread to use the model once {@link ANeuralNetworksModel_finish} has returned.</p>
 *
 * <p>It is also the application's responsibility to ensure that there are no
 * other uses of the model after calling {@link ANeuralNetworksModel_free}.
 * This includes any compilation, execution object or burst object created using
 * the model.</p>
 *
 * Available since API level 27.
 */
typedef struct ANeuralNetworksModel ANeuralNetworksModel;

/**
 * ANeuralNetworksCompilation is an opaque type that can be used to compile
 * a machine learning model.
 *
 * <p>To use:<ul>
 *    <li>Create a new compilation instance by calling the
 *        {@link ANeuralNetworksCompilation_create} function or
 *        {@link ANeuralNetworksCompilation_createForDevices}.</li>
 *    <li>Set any desired properties on the compilation (for example,
 *        {@link ANeuralNetworksCompilation_setPreference}).</li>
 *    <li>Optionally, set the caching signature and the cache directory on the
 *        compilation by calling {@link ANeuralNetworksCompilation_setCaching}.</li>
 *    <li>Complete the compilation with {@link ANeuralNetworksCompilation_finish}.</li>
 *    <li>Use the compilation as many times as needed
 *        with {@link ANeuralNetworksExecution_create} and
 *        {@link ANeuralNetworksBurst_create}.</li>
 *    <li>Destroy the compilation with {@link ANeuralNetworksCompilation_free}
 *        once all executions using the compilation have completed.</li></ul></p>
 *
 * A compilation is completed by calling {@link ANeuralNetworksCompilation_finish}.
 * A compilation is destroyed by calling {@link ANeuralNetworksCompilation_free}.
 *
 * <p>A compilation cannot be modified once {@link ANeuralNetworksCompilation_finish}
 * has been called on it.</p>
 *
 * <p>It is the application's responsibility to make sure that only
 * one thread modifies a compilation at a given time. It is however
 * safe for more than one thread to use the compilation once
 * {@link ANeuralNetworksCompilation_finish} has returned.</p>
 *
 * <p>It is also the application's responsibility to ensure that there are no other
 * uses of the compilation after calling {@link ANeuralNetworksCompilation_free}.
 * This includes any execution object or burst object created using the compilation,
 * or any memory descriptor with the compilation as part of one of the roles specified by
 * {@link ANeuralNetworksMemoryDesc_addInputRole} or
 * {@link ANeuralNetworksMemoryDesc_addOutputRole}.</p>
 *
 * Available since API level 27.
 */
typedef struct ANeuralNetworksCompilation ANeuralNetworksCompilation;

/**
 * ANeuralNetworksExecution is an opaque type that can be used to apply a machine
 * learning model to a set of inputs.
 *
 * <p>To use:<ul>
 *    <li>Create a new execution instance by calling the
 *        {@link ANeuralNetworksExecution_create} function.</li>
 *    <li>Associate input buffers or memory regions to the model inputs with
 *        {@link ANeuralNetworksExecution_setInput} or
 *        {@link ANeuralNetworksExecution_setInputFromMemory}.</li>
 *    <li>Associate output buffers or memory regions to the model outputs with
 *        {@link ANeuralNetworksExecution_setOutput} or
 *        {@link ANeuralNetworksExecution_setOutputFromMemory}.</li>
 *    <li>Apply the model with one of the following:</li><ul>
 *        <li>Asynchronously with {@link ANeuralNetworksExecution_startCompute}
 *            or with {@link ANeuralNetworksExecution_startComputeWithDependencies},
 *            waiting for the execution to complete with
 *            {@link ANeuralNetworksEvent_wait}.</li>
 *        <li>Synchronously with {@link ANeuralNetworksExecution_compute}.</li>
 *        <li>Synchronously as part of an execution burst with
 *            {@link ANeuralNetworksExecution_burstCompute}.</li></ul>
 *    <li>Destroy the execution with
 *        {@link ANeuralNetworksExecution_free}.</li></ul></p>
 *
 * <p>An output buffer or memory region must not overlap with any
 * other output buffer or memory region, with an input buffer or
 * memory region, or with an operand value in a memory object
 * ({@link ANeuralNetworksModel_setOperandValueFromMemory}).</p>
 *
 * <p>An execution cannot be modified once
 * {@link ANeuralNetworksExecution_burstCompute},
 * {@link ANeuralNetworksExecution_compute},
 * {@link ANeuralNetworksExecution_startCompute} or
 * {@link ANeuralNetworksExecution_startComputeWithDependencies} has been called on it.</p>
 *
 * <p>An execution can be applied to a model with
 * {@link ANeuralNetworksExecution_burstCompute},
 * {@link ANeuralNetworksExecution_compute},
 * {@link ANeuralNetworksExecution_startCompute} or
 * {@link ANeuralNetworksExecution_startComputeWithDependencies} only once. Create new
 * executions to do new evaluations of the model.</p>
 *
 * <p>It is the application's responsibility to make sure that only one thread
 * modifies an execution at a given time. It is however safe for more than one
 * thread to use {@link ANeuralNetworksEvent_wait} at the same time.</p>
 *
 * <p>It is also the application's responsibility to ensure that the execution
 * either has never been scheduled or has completed (i.e., that
 * {@link ANeuralNetworksExecution_burstCompute},
 * {@link ANeuralNetworksExecution_compute}, or
 * {@link ANeuralNetworksEvent_wait} has returned) before calling
 * {@link ANeuralNetworksExecution_free}.</p>.
 *
 * <p>It is also the application's responsibility to ensure that there are no other
 * uses of the execution after calling {@link ANeuralNetworksExecution_free}.</p>
 *
 * <p>Multiple executions can be scheduled and evaluated concurrently, either by
 * means of {@link ANeuralNetworksExecution_compute} or
 * {@link ANeuralNetworksExecution_burstCompute} (which are synchronous) in
 * different threads, or by means of
 * {@link ANeuralNetworksExecution_startCompute} or
 * {@link ANeuralNetworksExecution_startComputeWithDependencies} (which are asynchronous).
 * (Concurrent uses of {@link ANeuralNetworksExecution_burstCompute} must be on
 * different burst objects.) The runtime makes no guarantee on the ordering of
 * completion of executions. If it's important to the application, the
 * application should enforce the ordering by ensuring that one execution
 * completes before the next is scheduled (for example, by scheduling all
 * executions synchronously within a single thread, or by scheduling all
 * executions asynchronously and using {@link ANeuralNetworksEvent_wait} between
 * calls to {@link ANeuralNetworksExecution_startCompute}); or by using
 * {@link ANeuralNetworksExecution_startComputeWithDependencies} to make the execution wait for a
 * list of events to be signaled before starting the actual evaluation.</p>
 *
 * Available since API level 27.
 */
typedef struct ANeuralNetworksExecution ANeuralNetworksExecution;

#if __ANDROID_API__ >= 29
/**
 * Parameters for ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL operand.
 */
typedef struct ANeuralNetworksSymmPerChannelQuantParams {
    /* The index of the channel dimension. */
    uint32_t channelDim;
    /** The size of the scale array. Should be equal to dimension[channelDim] of the Operand. */
    uint32_t scaleCount;
    /** The array of scaling values for each channel. Each value must be greater than zero. */
    const float* scales;
} ANeuralNetworksSymmPerChannelQuantParams;

/**
 * ANeuralNetworksBurst is an opaque type that can be used to reduce the latency
 * of a rapid sequence of executions. It will likely cause overhead if only used
 * for a single execution.
 *
 * ANeuralNetworksBurst serves as a context object for any number of inferences
 * using {@link ANeuralNetworksExecution} objects. An ANeuralNetworksBurst
 * object and the {@link ANeuralNetworksExecution} objects used with it must all
 * have been created from the same {@link ANeuralNetworksCompilation} object.
 *
 * This object is also used as a hint to drivers, providing insight to the
 * lifetime of a rapid sequence of executions. For example, a driver may choose
 * to increase the clock frequency of its accelerator for the lifetime of a
 * burst object.
 *
 * <p>To use:<ul>
 *    <li>Create a new burst object by calling the
 *        {@link ANeuralNetworksBurst_create} function.</li>
 *    <li>For each execution:</li><ul>
 *        <li>Create {@link ANeuralNetworksExecution} and configure its
 *            properties (see {@link ANeuralNetworksExecution} for details).</li>
 *        <li>Apply the model synchronously with
 *            {@link ANeuralNetworksExecution_burstCompute}, reusing the same
 *            {@link ANeuralNetworksBurst} with the new
 *            {@link ANeuralNetworksExecution}.</li>
 *        <li>Use and free the {@link ANeuralNetworksExecution}.</li></ul>
 *    <li>Destroy the burst with
 *        {@link ANeuralNetworksBurst_free}.</li></ul></p>
 *
 * Available since API level 29.
 */
typedef struct ANeuralNetworksBurst ANeuralNetworksBurst;
#endif  //  __ANDROID_API__ >= 29

/**
 * ANeuralNetworksOperandType describes the type of an operand.
 *
 * This structure is used to describe both scalars and tensors.
 *
 * A tensor operand type with all dimensions specified is "fully
 * specified".  Whenever possible (i.e., whenever the dimensions are
 * known at model construction time), a tensor operand type should be
 * (but is not required to be) fully specified, in order to enable the
 * best possible performance.
 *
 * If a tensor operand's type is not fully specified, the dimensions
 * of the operand are deduced from the operand types and values of the
 * operation for which that operand is an output or from the corresponding
 * {@link ANEURALNETWORKS_IF} or {@link ANEURALNETWORKS_WHILE} operation input
 * operand type in the case of referenced model input operands.
 *
 * <p>In the following situations, a tensor operand type must be fully
 * specified:<ul>
 *     <li>The operand has a constant value, set by
 *         {@link ANeuralNetworksModel_setOperandValue} (with a
 *         non-nullptr buffer) or
 *         {@link ANeuralNetworksModel_setOperandValueFromMemory}.</li>
 *     <li>The operand is a model input (see
 *         {@link ANeuralNetworksModel_identifyInputsAndOutputs}) of the main
 *         model within a compilation.  A fully specified tensor operand type
 *         must either be provided to {@link ANeuralNetworksModel_addOperand};
 *         or it must be provided to the corresponding
 *         {@link ANeuralNetworksExecution_setInput}, or
 *         {@link ANeuralNetworksExecution_setInputFromMemory}.
 *         EXCEPTION: If the input is optional and omitted
 *         (by passing nullptr for buffer to
 *         {@link ANeuralNetworksExecution_setInput}) then it need
 *         not have a fully specified tensor operand type.</li>
 *     <li>The operand is a model output (see
 *         {@link ANeuralNetworksModel_identifyInputsAndOutputs}) of the main
 *         model within a compilation and is to be used with {@link
 *         ANeuralNetworksExecution_startComputeWithDependencies}.
 *         A fully specified tensor operand type must either be provided
 *         to {@link ANeuralNetworksModel_addOperand}; or it must be
 *         provided to the corresponding
 *         {@link ANeuralNetworksExecution_setOutput}, or
 *         {@link ANeuralNetworksExecution_setOutputFromMemory}.</li></ul>
 *
 * A tensor operand type of specified rank but some number of
 * unspecified dimensions is represented by setting dimensionCount to
 * the rank and each unspecified dimension to 0.
 *
 * Available since API level 27.
 *
 * Starting at API level 29, a tensor operand type of unspecified rank is
 * represented by setting dimensionCount to 0 and dimensions to NULL (just as if
 * it were a scalar operand type).
 */
typedef struct ANeuralNetworksOperandType {
    /**
     * The data type, e.g ANEURALNETWORKS_FLOAT32.
     */
    int32_t type;

    /**
     * The number of dimensions (rank).
     *
     * Must be 0 for scalars.
     */
    uint32_t dimensionCount;

    /**
     * The dimensions of the tensor.
     *
     * Must be nullptr for scalars.
     */
    const uint32_t* dimensions;

    /**
     * The quantization scale.
     *
     * Must be 0 when not applicable to an operand type.
     *
     * See {@link OperandCode}.
     */
    float scale;

    /**
     * The quantization zero point.
     *
     * Must be 0 when not applicable to an operand type.
     *
     * See {@link OperandCode}.
     */
    int32_t zeroPoint;
} ANeuralNetworksOperandType;

typedef int32_t ANeuralNetworksOperationType;

/**
 * ANeuralNetworksEvent is an opaque type that represents an event
 * that will be signaled once an execution completes.
 *
 * Available since API level 27.
 */
typedef struct ANeuralNetworksEvent ANeuralNetworksEvent;

#if __ANDROID_API__ >= 29

/**
 * ANeuralNetworksDevice is an opaque type that represents a device.
 *
 * This type is used to query basic properties and supported operations of the corresponding
 * device, and control which device(s) a model is to be run on.
 *
 * Available since API level 29.
 */
typedef struct ANeuralNetworksDevice ANeuralNetworksDevice;

#endif  // __ANDROID_API__ >= 29

#if __ANDROID_API__ >= 30

/**
 * ANeuralNetworksMemoryDesc is an opaque type that represents a memory descriptor.
 *
 * A memory descriptor describes the properties of a memory object, and is used by
 * {@link ANeuralNetworksMemory_createFromDesc}.
 *
 * To use:
 *   - Create a new memory descriptor by calling {@link ANeuralNetworksMemoryDesc_create}.
 *   - Specify all of the intended input and output roles by calling
 *     {@link ANeuralNetworksMemoryDesc_addInputRole} and
 *     {@link ANeuralNetworksMemoryDesc_addOutputRole}.
 *   - Optionally, specify the memory dimensions by calling
 *     {@link ANeuralNetworksMemoryDesc_setDimensions}.
 *   - Complete the memory descriptor with {@link ANeuralNetworksMemoryDesc_finish}.
 *   - Use the memory descriptor as many times as needed with
 *     {@link ANeuralNetworksMemory_createFromDesc}.
 *   - Destroy the memory descriptor with {@link ANeuralNetworksMemoryDesc_free}.
 *
 * A memory descriptor is completed by calling {@link ANeuralNetworksMemoryDesc_finish}.
 * A memory descriptor is destroyed by calling {@link ANeuralNetworksMemoryDesc_free}.
 *
 * A memory descriptor must not be modified once {@link ANeuralNetworksMemoryDesc_finish}
 * has been called on it.
 *
 * It is the application's responsibility to make sure that only
 * one thread modifies a memory descriptor at a given time. It is however
 * safe for more than one thread to use the memory descriptor once
 * {@link ANeuralNetworksMemoryDesc_finish} has returned.
 *
 * It is also the application's responsibility to ensure that there are no other
 * uses of the memory descriptor after calling {@link ANeuralNetworksMemoryDesc_free}.
 * It is however safe to continue using a {@link ANeuralNetworksMemory} object created
 * from the memory descriptor.
 *
 * Available since API level 30.
 */
typedef struct ANeuralNetworksMemoryDesc ANeuralNetworksMemoryDesc;

/**
 * Create a {@link ANeuralNetworksMemoryDesc} with no properties.
 *
 * This only creates the memory descriptor. Its properties should be set with calls to
 * {@link ANeuralNetworksMemoryDesc_addInputRole},
 * {@link ANeuralNetworksMemoryDesc_addOutputRole}, and
 * {@link ANeuralNetworksMemoryDesc_setDimensions}.
 *
 * {@link ANeuralNetworksMemoryDesc_finish} must be called once all properties have been set.
 *
 * {@link ANeuralNetworksMemoryDesc_free} must be called once the memory descriptor
 * is no longer needed.
 *
 * Available since API level 30.
 *
 * @param desc The {@link ANeuralNetworksMemoryDesc} to be created.
 *             Set to NULL if unsuccessful.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksMemoryDesc_create(ANeuralNetworksMemoryDesc** desc) __INTRODUCED_IN(30);

/**
 * Destroy a memory descriptor.
 *
 * The memory descriptor need not have been finished by a call to
 * {@link ANeuralNetworksMemoryDesc_finish}.
 *
 * See {@link ANeuralNetworksMemoryDesc} for information on multithreaded usage.
 *
 * Available since API level 30.
 *
 * @param desc The memory descriptor to be destroyed. Passing NULL is acceptable and
 *             results in no operation.
 */
void ANeuralNetworksMemoryDesc_free(ANeuralNetworksMemoryDesc* desc) __INTRODUCED_IN(30);

/**
 * Specify that a memory object will be playing the role of an input to an execution created from a
 * particular compilation.
 *
 * The compilation and the input index fully specify an input operand. This function
 * may be invoked multiple times on the same memory descriptor with different input operands,
 * and the same input operand may be specified on multiple memory descriptors. However,
 * specifying the same input operand on the same memory descriptor more than once will
 * return an error.
 *
 * The dimensions of the corresponding model operands of all the roles specified by
 * {@link ANeuralNetworksMemoryDesc_addInputRole} and
 * {@link ANeuralNetworksMemoryDesc_addOutputRole} must be compatible with each other. Two
 * dimensions are incompatible if both ranks are fully specified but have different values, or if
 * there is at least one axis that is fully specified in both but has different values.
 *
 * At least one of {@link ANeuralNetworksMemoryDesc_addInputRole} and
 * {@link ANeuralNetworksMemoryDesc_addOutputRole} must be called on a memory descriptor
 * before invoking {@link ANeuralNetworksMemoryDesc_finish}.
 *
 * Attempting to modify a memory descriptor once {@link ANeuralNetworksMemoryDesc_finish} has been
 * called will return an error.
 *
 * See {@link ANeuralNetworksMemoryDesc} for information on multithreaded usage.
 *
 * Available since API level 30.
 *
 * @param desc The memory descriptor to be modified.
 * @param compilation The compilation object. It must already have been finished by calling
 *                    {@link ANeuralNetworksCompilation_finish}, and must outlive the memory
 *                    descriptor.
 * @param index The index of the input argument we are referencing from the compilation. It is
 *              an index into the inputs list passed to
 *              {@link ANeuralNetworksModel_identifyInputsAndOutputs}. It is not
 *              the index associated with {@link ANeuralNetworksModel_addOperand}.
 * @param frequency A floating-point value within the range (0.0, 1.0]. Describes how likely the
 *                  memory is to be used in the specified role. This is provided as a hint to
 *                  optimize the case when different roles prefer different memory locations or data
 *                  layouts.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksMemoryDesc_addInputRole(ANeuralNetworksMemoryDesc* desc,
                                           const ANeuralNetworksCompilation* compilation,
                                           uint32_t index, float frequency) __INTRODUCED_IN(30);

/**
 * Specify that a memory object will be playing the role of an output to an execution created from a
 * particular compilation.
 *
 * The compilation and the output index fully specify an output operand. This function
 * may be invoked multiple times on the same memory descriptor with different output operands,
 * and the same output operand may be specified on multiple memory descriptors. However,
 * specifying the same output operand on the same memory descriptor object more than once will
 * return an error.
 *
 * The dimensions of the corresponding model operands of all the roles specified by
 * {@link ANeuralNetworksMemoryDesc_addInputRole} and
 * {@link ANeuralNetworksMemoryDesc_addOutputRole} must be compatible with each other. Two
 * dimensions are incompatible if both ranks are fully specified but have different values, or if
 * there is at least one axis that is fully specified in both but has different values.
 *
 * At least one of {@link ANeuralNetworksMemoryDesc_addInputRole} and
 * {@link ANeuralNetworksMemoryDesc_addOutputRole} must be called on the memory descriptor
 * before invoking {@link ANeuralNetworksMemoryDesc_finish}.
 *
 * Attempting to modify a memory descriptor once {@link ANeuralNetworksMemoryDesc_finish} has been
 * called will return an error.
 *
 * See {@link ANeuralNetworksMemoryDesc} for information on multithreaded usage.
 *
 * Available since API level 30.
 *
 * @param desc The memory descriptor to be modified.
 * @param compilation The compilation object. It must already have been finished by calling
 *                    {@link ANeuralNetworksCompilation_finish}, and must outlive the memory
 *                    descriptor.
 * @param index The index of the output argument we are referencing from the compilation. It is
 *              an index into the outputs list passed to
 *              {@link ANeuralNetworksModel_identifyInputsAndOutputs}. It is not
 *              the index associated with {@link ANeuralNetworksModel_addOperand}.
 * @param frequency A floating-point value within the range (0.0, 1.0]. Describes how likely the
 *                  memory is to be used in the specified role. This is provided as a hint to
 *                  optimize the case when multiple roles prefer different memory locations or data
 *                  layouts.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksMemoryDesc_addOutputRole(ANeuralNetworksMemoryDesc* desc,
                                            const ANeuralNetworksCompilation* compilation,
                                            uint32_t index, float frequency) __INTRODUCED_IN(30);

/**
 * Set the dimensional information of the memory descriptor.
 *
 * The specified dimensions must be compatible with the dimensions of the corresponding model
 * operands of all the roles specified by {@link ANeuralNetworksMemoryDesc_addInputRole} and
 * {@link ANeuralNetworksMemoryDesc_addOutputRole}. Two dimensions are incompatible if both ranks
 * are fully specified but have different values, or if there is at least one axis that is fully
 * specified in both but has different values.
 *
 * Attempting to modify a memory descriptor once {@link ANeuralNetworksMemoryDesc_finish} has been
 * called will return an error.
 *
 * See {@link ANeuralNetworksMemoryDesc} for information on multithreaded usage.
 *
 * Available since API level 30.
 *
 * @param desc The memory descriptor to be modified.
 * @param rank The number of dimensions. Must be 0 for scalars.
 * @param dimensions An array of dimensions. An entry with the value 0 indicates that the
 *                   corresponding axis has an unknown size.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksMemoryDesc_setDimensions(ANeuralNetworksMemoryDesc* desc, uint32_t rank,
                                            const uint32_t* dimensions) __INTRODUCED_IN(30);

/**
 * Indicate that we have finished modifying a memory descriptor. Required before calling
 * {@link ANeuralNetworksMemory_createFromDesc}.
 *
 * This function must only be called once for a given memory descriptor.
 *
 * See {@link ANeuralNetworksMemoryDesc} for information on multithreaded usage.
 *
 * Available since API level 30.
 *
 * @param desc The memory descriptor to be finished.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksMemoryDesc_finish(ANeuralNetworksMemoryDesc* desc) __INTRODUCED_IN(30);

/**
 * Creates a memory object from a memory descriptor.
 *
 * The memory object is created with an uninitialized buffer. A memory object with an uninitialized
 * buffer may only be used according to the roles specified by {@link
 * ANeuralNetworksMemoryDesc_addOutputRole}, or as the destination memory in {@link
 * ANeuralNetworksMemory_copy}. The buffer of a memory object is initialized after the memory object
 * is used as an output in a successful execution, or used as the destination memory in a successful
 * {@link ANeuralNetworksMemory_copy}. A memory object with an initialized buffer may be used
 * according to all roles specified in {@link ANeuralNetworksMemoryDesc}, or as the source or
 * destination memory in {@link ANeuralNetworksMemory_copy}. The buffer of a memory object will
 * return to the uninitialized state if the memory object is used as an output in a failed
 * execution, or used as the destination memory in a failed {@link ANeuralNetworksMemory_copy}.
 *
 * The dimensions of the memory descriptor are deduced from the dimensions of the corresponding
 * model operands of all the roles specified by {@link ANeuralNetworksMemoryDesc_addInputRole} and
 * {@link ANeuralNetworksMemoryDesc_addOutputRole}, as well as the dimensions set by the call to
 * {@link ANeuralNetworksMemoryDesc_setDimensions}, if any. The memory descriptor may have
 * unspecified dimensions or rank. In such a case, the same memory object may be used with different
 * shapes of outputs in different executions. When the memory is used as an input, the input shape
 * must be the same as the output shape from the last execution using this memory object as an
 * output, or the last {@link ANeuralNetworkMemory_copy} using this memory object as the destination
 * memory. Creating a memory object with unspecified dimensions or rank may fail for certain sets of
 * roles.
 *
 * Using the memory in roles or shapes that are not compatible with the rules specified above will
 * return an error.
 *
 * When calling {@link ANeuralNetworksExecution_setInputFromMemory} or
 * {@link ANeuralNetworksExecution_setOutputFromMemory} with the memory object,
 * both offset and length must be set to zero and the entire memory region will be
 * associated with the specified input or output operand.
 *
 * Calling {@link ANeuralNetworksModel_setOperandValueFromMemory} with the memory created from this
 * function will return an error.
 *
 * {@link ANeuralNetworksMemory_free} must be called once the memory is no longer needed.
 *
 * Attempting to create memory from an unfinished memory descriptor will return an error.
 *
 * The provided {@link ANeuralNetworksMemoryDesc} need not outlive the {@link ANeuralNetworksMemory}
 * object.
 *
 * Available since API level 30.
 *
 * @param desc The memory descriptor.
 * @param memory The memory object to be created.
 *               Set to NULL if unsuccessful.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful; ANEURALNETWORKS_OP_FAILED if the memory is
 *         created with unspecified dimensions or rank and it is not supported for this set of
 *         roles.
 */
int ANeuralNetworksMemory_createFromDesc(const ANeuralNetworksMemoryDesc* desc,
                                         ANeuralNetworksMemory** memory) __INTRODUCED_IN(30);

/**
 * Copies data from one memory object to another.
 *
 * If at most one of the src and dst is created from {@link ANeuralNetworksMemory_createFromDesc},
 * the src and dst must have the same logical size:
 * - If the memory is created from {@link ANeuralNetworksMemory_createFromFd}, or if it is created
 *   from {@link ANeuralNetworksMemory_createFromAHardwareBuffer} with format of
 *   AHARDWAREBUFFER_FORMAT_BLOB, the logical size equals the size of the memory.
 * - If the memory is created from {@link ANeuralNetworksMemory_createFromAHardwareBuffer} with a
 *   format other than AHARDWAREBUFFER_FORMAT_BLOB, the logical size equals the size when there is
 *   no padding and the data is tightly packed. This function may fail if the AHardwareBuffer
 *   cannot be accessed.
 * - If the memory is created from {@link ANeuralNetworksMemory_createFromDesc}, the logical size
 *   equals the size indicated by the {@link OperandCode} multiplied by the number of elements. This
 *   function will fail if the number of elements is unknown.
 *
 * If both src and dst are created from {@link ANeuralNetworksMemory_createFromDesc}, they must have
 * compatible dimensions. Two dimensions are incompatible if both ranks are fully specified but
 * have different values, or if there is at least one axis that is fully specified in both but has
 * different values. The dst may have unspecified dimensions or rank. In such a case, the dimensions
 * of dst will get updated according to the dimensions of the src.
 *
 * In both cases, if the src is created from {@link ANeuralNetworksMemory_createFromDesc}, it must
 * have been used as an output in a successful execution, or used as the destination memory in a
 * successful {@link ANeuralNetworksMemory_copy}.
 *
 * The src and dst may have different data layout, in which case the data copying is performed
 * logically with data layout transformation.
 *
 * Available since API level 30.
 *
 * @param src The source memory object.
 * @param dst The destination memory object.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksMemory_copy(const ANeuralNetworksMemory* src, const ANeuralNetworksMemory* dst)
        __INTRODUCED_IN(30);

#endif  // __ANDROID_API__ >= 30

#if __ANDROID_API__ >= 29

/**
 * Get the number of available devices.
 *
 * @param numDevices Used to return the number of devices.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 29.
 */
int ANeuralNetworks_getDeviceCount(uint32_t* numDevices) __INTRODUCED_IN(29);

/**
 * Get the representation of the specified device.
 *
 * @param devIndex The index of the specified device. Must be less than the
                   number of available devices.
 * @param device The representation of the specified device.
 *               The same representation will always be returned for the specified
 *               device.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 29.
 */
int ANeuralNetworks_getDevice(uint32_t devIndex, ANeuralNetworksDevice** device)
        __INTRODUCED_IN(29);

/**
 * Get the name of the specified device.
 *
 * @param device The representation of the specified device.
 * @param name   The returned name of the specified device. The name will be in UTF-8
 *               and will be null-terminated. It will be recognizable as a known device name
 *               rather than a cryptic string. For devices with feature level reported by
 *               {@link ANeuralNetworksDevice_getFeatureLevel} that is 29 and above, the
 *               format of the name is {VENDOR}-{DEVICE}. For devices with feature level 28
 *               or lower, the format of the name is undefined.
 *               The name will remain valid for the duration of the application.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 29.
 */
int ANeuralNetworksDevice_getName(const ANeuralNetworksDevice* device, const char** name)
        __INTRODUCED_IN(29);

/**
 * Get the type of a given device.
 *
 * The device type can be used to help application developers to distribute Machine Learning
 * workloads and other workloads such as graphical rendering.
 * E.g., for an app which renders AR scenes based on real time object detection results,
 * the developer could choose an ACCELERATOR type device for ML workloads, and reserve GPU
 * for graphical rendering.
 *
 * @param device The representation of the specified device.
 * @param type The returned {@link DeviceTypeCode} of the specified device.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 29.
 */
int ANeuralNetworksDevice_getType(const ANeuralNetworksDevice* device, int32_t* type)
        __INTRODUCED_IN(29);

/**
 * Get the version of the driver implementation of the specified device.
 *
 * It’s the responsibility of the driver implementor to insure that this version string
 * uniquely distinguishes this implementation from all previous implementations.
 *
 * This version string must not be confused with the feature level which is solely defined
 * by {@link ANeuralNetworksDevice_getFeatureLevel}. There is no implicit ordering of the versions.
 * For example, it is not possible to filter all drivers older than a certain version.
 *
 * Application developers may use this version string to avoid or prefer specific driver
 * implementations. For example, an application may want to do so because:
 *     - A specific version of the driver does not provide the required performance,
 *       perhaps because of a performance regression.
 *     - A specific version of the driver has a bug or returns results that don’t match
 *       the minimum precision requirement for the application.
 *
 * @param device The representation of the specified device.
 * @param version The returned version string of the driver for the specified device. The
 *                string will be in UTF-8 and will be null-terminated. For devices with feature
 *                level 28 or lower, "UNKNOWN" will be returned. The version string will remain
 *                valid for the duration of the application.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 29.
 */
int ANeuralNetworksDevice_getVersion(const ANeuralNetworksDevice* device, const char** version)
        __INTRODUCED_IN(29);

/**
 * Get the supported NNAPI version of the specified device.
 *
 * Each device has a supported feature level, which is the most advanced feature this driver
 * implements. For example, if the driver implements the features introduced in Android P,
 * but does not implement the features introduced after Android P, the value would be 28.
 * Developers could decide whether or not the specified device should be used for a Model that
 * has certain feature requirements.
 *
 * @param device The representation of the specified device.
 * @param featureLevel The API level of the most advanced feature this driver implements.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 29.
 */
int ANeuralNetworksDevice_getFeatureLevel(const ANeuralNetworksDevice* device,
                                          int64_t* featureLevel) __INTRODUCED_IN(29);

#if __ANDROID_API__ >= 30

/**
 * Wait until the device is in a live state.
 *
 * A device may encounter internal errors and temporarily enter a dead state. A
 * call that uses a device in such a state will return with the error
 * {@link ANEURALNETWORKS_DEAD_OBJECT}. ANeuralNetworksDevice_wait will block until
 * the device is in a live state.
 *
 * @param device The representation of the specified device.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 30.
 */
int ANeuralNetworksDevice_wait(const ANeuralNetworksDevice* device) __INTRODUCED_IN(30);

#endif  // __ANDROID_API__ >= 30

/**
 * Get the supported operations for a specified set of devices. If multiple devices
 * are selected, the supported operation list is a union of supported operations of all
 * selected devices.
 *
 * @param model The model to be queried.
 * @param devices The set of devices. Must not contain duplicates.
 * @param numDevices The number of devices in the set.
 * @param supportedOps The boolean array to be filled. True means supported. The size of the
 *                     boolean array must be at least as large as the number of operations
 *                     in the model. The order of elements in the supportedOps array matches
 *                     the order in which the corresponding operations were added to the model.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 29.
 */
int ANeuralNetworksModel_getSupportedOperationsForDevices(
        const ANeuralNetworksModel* model, const ANeuralNetworksDevice* const* devices,
        uint32_t numDevices, bool* supportedOps) __INTRODUCED_IN(29);

/**
 * Create a {@link ANeuralNetworksCompilation} to compile the given model for a specified set
 * of devices. If more than one device is specified, the compilation will
 * distribute the workload automatically across the devices. The model must be fully
 * supported by the specified set of devices. This means that
 * ANeuralNetworksModel_getSupportedOperationsForDevices() must have returned true for every
 * operation for that model/devices pair.
 *
 * The user must handle all compilation and execution failures from the
 * specified set of devices. This is in contrast to a use of {@link
 * ANeuralNetworksCompilation_create}, where the runtime will attempt to recover
 * from such failures.
 *
 * The model passed to this function is termed the "main model" of the
 * compilation, to distinguish it from other models referred to by an Operand
 * of type {@link ANEURALNETWORKS_MODEL} within this compilation.
 *
 * @param model The {@link ANeuralNetworksModel} to be compiled.
 * @param devices The set of devices. Must not contain duplicates.
 * @param numDevices The number of devices in the set.
 * @param compilation The newly created object or NULL if unsuccessful.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful, ANEURALNETWORKS_BAD_DATA
 *         if the model is invalid.
 *
 * Available since API level 29.
 */
int ANeuralNetworksCompilation_createForDevices(ANeuralNetworksModel* model,
                                                const ANeuralNetworksDevice* const* devices,
                                                uint32_t numDevices,
                                                ANeuralNetworksCompilation** compilation)
        __INTRODUCED_IN(29);

/**
 * Sets the compilation caching signature and the cache directory.
 *
 * Provides optional caching information to the runtime for faster repeated
 * compilation.
 *
 * See {@link ANeuralNetworksCompilation} for information on multithreaded usage.
 *
 * @param compilation The compilation to be modified.
 * @param cacheDir The cache directory for the runtime to store and retrieve caching
 *                 data. It is recommended to use the code cache directory provided
 *                 by the Android runtime. If not using the code cache directory, the
 *                 user should choose a directory local to the application, and is
 *                 responsible for managing the cache entries.
 * @param token The token provided by the user to specify a model must be of length
 *              ANEURALNETWORKS_BYTE_SIZE_OF_CACHE_TOKEN. The user should ensure that
 *              the token is unique to a model within the application. The NNAPI
 *              runtime cannot detect token collisions; a collision will result in a
 *              failed execution or in a successful execution that produces incorrect
 *              output values.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 29.
 */
int ANeuralNetworksCompilation_setCaching(ANeuralNetworksCompilation* compilation,
                                          const char* cacheDir, const uint8_t* token)
        __INTRODUCED_IN(29);

/**
 * Schedule synchronous evaluation of the execution.
 *
 * <p>Schedules synchronous evaluation of the execution. Returns once the
 * execution has completed and the outputs are ready to be consumed.
 * </p>
 *
 * If {@link ANeuralNetworksExecution_setTimeout} was called on this execution,
 * and the execution is not able to complete before the timeout duration is
 * exceeded, then execution may be aborted, in which case
 * {@link ANEURALNETWORKS_MISSED_DEADLINE_*} will be returned. If the device has
 * a feature level reported by {@link ANeuralNetworksDevice_getFeatureLevel}
 * that is lower than 30, then the timeout duration hint will be ignored.
 *
 * If this execution contains a {@link ANEURALNETWORKS_WHILE} operation, and
 * the condition model does not output false within the loop timeout duration,
 * then execution will be aborted and {@link ANEURALNETWORKS_MISSED_DEADLINE_*}
 * will be returned.
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * See {@link ANeuralNetworksExecution_burstCompute} for burst synchronous execution.
 * See {@link ANeuralNetworksExecution_startCompute} for regular asynchronous execution.
 * See {@link ANeuralNetworksExecution_startComputeWithDependencies} for
 * asynchronous execution with dependencies.
 *
 * Available since API level 29.
 *
 * @param execution The execution to be scheduled and executed.
 *
 * @return ANEURALNETWORKS_NO_ERROR if the execution completed normally.
 *         ANEURALNETWORKS_UNMAPPABLE if the execution input or output memory cannot
 *         be properly mapped.
 */
int ANeuralNetworksExecution_compute(ANeuralNetworksExecution* execution) __INTRODUCED_IN(29);

/**
 * Get the dimensional information of the specified output operand of the model of the
 * {@link ANeuralNetworksExecution}.
 *
 * The execution must have completed.  On asynchronous execution initiated by
 * {@link ANeuralNetworksExecution_startCompute} or
 * {@link ANeuralNetworksExecution_startComputeWithDependencies},
 * {@link ANeuralNetworksEvent_wait} must be called prior to this function.
 *
 * @param execution The execution to be queried.
 * @param index The index of the output argument we are querying. It is
 *              an index into the lists passed to
 *              {@link ANeuralNetworksModel_identifyInputsAndOutputs}. It is not
 *              the index associated with {@link ANeuralNetworksModel_addOperand}.
 * @param rank The rank of the output operand.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful, ANEURALNETWORKS_OUTPUT_INSUFFICIENT_SIZE
 *         if the target output is provided an insufficient buffer at execution time,
 *         ANEURALNETWORKS_BAD_DATA if the index is invalid.
 *
 * Available since API level 29.
 */
int ANeuralNetworksExecution_getOutputOperandRank(ANeuralNetworksExecution* execution,
                                                  int32_t index, uint32_t* rank)
        __INTRODUCED_IN(29);

/**
 * Get the dimensional information of the specified output operand of the model of the
 * {@link ANeuralNetworksExecution}. The target output operand cannot be a scalar.
 *
 * The execution must have completed.  On asynchronous execution initiated by
 * {@link ANeuralNetworksExecution_startCompute} or
 * {@link ANeuralNetworksExecution_startComputeWithDependencies},
 * {@link ANeuralNetworksEvent_wait} must be called prior to this function.
 *
 * @param execution The execution to be queried.
 * @param index The index of the output argument we are querying. It is an index into the lists
 *              passed to {@link ANeuralNetworksModel_identifyInputsAndOutputs}. It is not
 *              the index associated with {@link ANeuralNetworksModel_addOperand}.
 * @param dimensions The dimension array to be filled. The size of the array must be exactly as
 *                   large as the rank of the output operand to be queried in the model.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful, ANEURALNETWORKS_OUTPUT_INSUFFICIENT_SIZE
 *         if the target output is provided an insufficient buffer at execution time,
 *         ANEURALNETWORKS_BAD_DATA if the index is invalid or if the target is a scalar.
 *
 * Available since API level 29.
 */
int ANeuralNetworksExecution_getOutputOperandDimensions(ANeuralNetworksExecution* execution,
                                                        int32_t index, uint32_t* dimensions)
        __INTRODUCED_IN(29);

/**
 * Create a {@link ANeuralNetworksBurst} to apply the given compilation.
 * This only creates the burst object. Computation is only performed once
 * {@link ANeuralNetworksExecution_burstCompute} is invoked with a valid
 * {@link ANeuralNetworksExecution} and {@link ANeuralNetworksBurst}.
 *
 * <p>The provided compilation must outlive the burst object.</p>
 *
 * Available since API level 29.
 *
 * @param compilation The {@link ANeuralNetworksCompilation} to be evaluated.
 * @param burst The newly created object or NULL if unsuccessful.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful, ANEURALNETWORKS_BAD_DATA
 *         if the compilation is invalid.
 */
int ANeuralNetworksBurst_create(ANeuralNetworksCompilation* compilation,
                                ANeuralNetworksBurst** burst) __INTRODUCED_IN(29);

/**
 * Destroys the burst object.
 *
 * Available since API level 29.
 *
 * @param burst The burst object to be destroyed. Passing NULL is acceptable and
 *              results in no operation.
 */
void ANeuralNetworksBurst_free(ANeuralNetworksBurst* burst) __INTRODUCED_IN(29);

/**
 * Schedule synchronous evaluation of the execution on a burst object.
 *
 * <p>Schedules synchronous evaluation of the execution. Returns once the
 * execution has completed and the outputs are ready to be consumed.</p>
 *
 * If {@link ANeuralNetworksExecution_setTimeout} was called on the execution,
 * and the execution is not able to complete before the timeout duration is
 * exceeded, then execution may be aborted, in which case
 * {@link ANEURALNETWORKS_MISSED_DEADLINE_*} will be returned.
 *
 * If the execution contains a {@link ANEURALNETWORKS_WHILE} operation, and
 * the condition model does not output false within the loop timeout duration,
 * then execution will be aborted and {@link ANEURALNETWORKS_MISSED_DEADLINE_*}
 * will be returned. If the device has a feature level reported by
 * {@link ANeuralNetworksDevice_getFeatureLevel} that is lower than 30, then the
 * timeout duration hint will be ignored.
 *
 * <p>There must be at most one {@link ANeuralNetworksExecution} processing at
 * any given time for any given burst object. Any
 * {@link ANeuralNetworksExecution} launched before the previous has finished
 * will result in ANEURALNETWORKS_BAD_STATE.</p>
 *
 * See {@link ANeuralNetworksExecution_compute} for synchronous execution.
 * See {@link ANeuralNetworksExecution_startCompute} for regular asynchronous execution.
 * See {@link ANeuralNetworksExecution_startComputeWithDependencies} for
 * asynchronous execution with dependencies.
 *
 * Available since API level 29.
 *
 * @param burst The burst object to execute on.
 * @param execution The execution to be scheduled and executed. The execution
 *                  must be created from the same {@link
 *                  ANeuralNetworksCompilation} as the burst object.
 *
 * @return ANEURALNETWORKS_NO_ERROR if the execution completed normally.
 */
int ANeuralNetworksExecution_burstCompute(ANeuralNetworksExecution* execution,
                                          ANeuralNetworksBurst* burst) __INTRODUCED_IN(29);

/**
 * Creates a shared memory object from an AHardwareBuffer handle.
 *
 * If the shared memory is backed by an AHardwareBuffer of AHARDWAREBUFFER_FORMAT_BLOB
 * format, it can be used the same way as shared memory created from a file handle. See
 * {@link ANeuralNetworksMemory} for a description on how to use this shared memory.
 *
 * If the shared memory is backed by an AHardwareBuffer of a format other than
 * AHARDWAREBUFFER_FORMAT_BLOB, it can only be used for Model inputs and outputs.
 * When calling {@link ANeuralNetworksExecution_setInputFromMemory} or
 * {@link ANeuralNetworksExecution_setOutputFromMemory} with the shared memory, both
 * offset and length must be set to zero and the entire memory region will be
 * associated with the specified input or output operand. There is no guarantee
 * that an arbitrary AHardwareBuffer_Format and AHardwareBuffer_UsageFlags combination
 * can be used by arbitrary devices. The execution will fail if the selected set of
 * devices cannot consume the buffer.
 *
 * Calling {@link ANeuralNetworksModel_setOperandValueFromMemory} with shared memory
 * backed by an AHardwareBuffer of a format other than AHARDWAREBUFFER_FORMAT_BLOB is
 * disallowed.
 *
 * The provided AHardwareBuffer must outlive the ANeuralNetworksMemory object.
 *
 * Available since API level 29.
 *
 * @param ahwb The AHardwareBuffer handle.
 * @param memory The memory object to be created.
 *               Set to NULL if unsuccessful.
 *
 * @return ANEURALNETWORKS_NO_ERROR if the request completed normally.
 *
 * @see AHardwareBuffer
 */
int ANeuralNetworksMemory_createFromAHardwareBuffer(const AHardwareBuffer* ahwb,
                                                    ANeuralNetworksMemory** memory)
        __INTRODUCED_IN(29);

/**

 * Specifies whether duration of the {@link ANeuralNetworksExecution} is to be
 * measured. Evaluation of the execution must not have been scheduled.
 *
 * By default, duration is not measured.
 *
 * The {@link ANeuralNetworksExecution} must have been created from an
 * {@link ANeuralNetworksCompilation} which in turn was created from
 * {@link ANeuralNetworksCompilation_createForDevices} with numDevices = 1.
 * If the device has a feature level reported by
 * {@link ANeuralNetworksDevice_getFeatureLevel} that is lower than 29, then the
 * duration will not be measured.
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * Available since API level 29.
 *
 * @param execution The execution to be modified.
 * @param measure 'true' if duration is to be measured, 'false' if not.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksExecution_setMeasureTiming(ANeuralNetworksExecution* execution, bool measure)
        __INTRODUCED_IN(29);

/**
 * Get the time spent in the specified {@link ANeuralNetworksExecution}, in nanoseconds.
 *
 * The execution must have completed.  On asynchronous execution initiated by
 * {@link ANeuralNetworksExecution_startCompute} or
 * {@link ANeuralNetworksExecution_startComputeWithDependencies},
 * {@link ANeuralNetworksEvent_wait} must be called prior to this function.
 *
 * @param execution The execution to be queried.
 * @param durationCode The measurement to be queried, specified by {@link DurationCode}.
 * @param duration The returned duration. If no measurement was requested by
 *                 {@link ANeuralNetworksExecution_setMeasureTiming}, if the
 *                 device is has a feature level reported by
 *                 {@link ANeuralNetworksDevice_getFeatureLevel} that is lower
 *                 than 29, or for some other reason the duration is not
 *                 available, UINT64_MAX will be returned. A particular device
 *                 need not support any given measurement.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 29.
 */
int ANeuralNetworksExecution_getDuration(const ANeuralNetworksExecution* execution,
                                         int32_t durationCode, uint64_t* duration)
        __INTRODUCED_IN(29);

#endif  // __ANDROID_API__ >= 29

#if __ANDROID_API__ >= 27

/**
 * Creates a shared memory object from a file descriptor.
 *
 * The shared memory is backed by a file descriptor via mmap.
 * See {@link ANeuralNetworksMemory} for a description on how to use
 * this shared memory.
 *
 * Available since API level 27.
 *
 * @param size The requested size in bytes.
 *             Must not be larger than the file size.
 * @param prot The desired memory protection for the mapping.
 *             It is either PROT_NONE or the bitwise OR of one or
 *             more of the following flags: PROT_READ, PROT_WRITE.
 * @param fd The requested file descriptor.
 *           The file descriptor has to be mmap-able. The file
 *           descriptor will be duplicated.
 * @param offset The offset to the beginning of the file of the area to map.
 *               The offset has to be aligned to a page size.
 * @param memory The memory object to be created.
 *               Set to NULL if unsuccessful.
 *
 * @return ANEURALNETWORKS_NO_ERROR if the request completed normally.
 */
int ANeuralNetworksMemory_createFromFd(size_t size, int protect, int fd, size_t offset,
                                       ANeuralNetworksMemory** memory) __INTRODUCED_IN(27);

/**
 * Delete a memory object.
 *
 * Destroys the object used by the run time to keep track of the memory.
 * This will free the underlying actual memory if no other code has open
 * handles to this memory.
 *
 * Available since API level 27.
 *
 * @param memory The memory object to be freed. Passing NULL is acceptable and
 *               results in no operation.
 */
void ANeuralNetworksMemory_free(ANeuralNetworksMemory* memory) __INTRODUCED_IN(27);

/**
 * Create an empty {@link ANeuralNetworksModel}.
 *
 * <p>This only creates the object. Computation is performed once
 * {@link ANeuralNetworksExecution_burstCompute},
 * {@link ANeuralNetworksExecution_compute},
 * {@link ANeuralNetworksExecution_startCompute} or
 * {@link ANeuralNetworksExecution_startComputeWithDependencies} is invoked.
 *
 * The model should be constructed with calls to
 * {@link ANeuralNetworksModel_addOperation} and
 * {@link ANeuralNetworksModel_addOperand}
 *
 * <p>{@link ANeuralNetworksModel_finish} should be called once the model
 * has been fully constructed.</p>
 *
 * <p>{@link ANeuralNetworksModel_free} should be called once the model
 * is no longer needed.</p>
 *
 * Available since API level 27.
 *
 * @param model The {@link ANeuralNetworksModel} to be created.
 *              Set to NULL if unsuccessful.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksModel_create(ANeuralNetworksModel** model) __INTRODUCED_IN(27);

/**
 * Destroy a model.
 *
 * The model need not have been finished by a call to
 * {@link ANeuralNetworksModel_finish}.
 *
 * See {@link ANeuralNetworksModel} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param model The model to be destroyed. Passing NULL is acceptable and
 *              results in no operation.
 */
void ANeuralNetworksModel_free(ANeuralNetworksModel* model) __INTRODUCED_IN(27);

/**
 * Indicate that we have finished modifying a model. Required before
 * calling {@link ANeuralNetworksCompilation_create} and
 * {@link ANeuralNetworksCompilation_createForDevices}.
 *
 * An application must ensure that no other thread uses the model at the same
 * time.
 *
 * This function must only be called once for a given model.
 *
 * See {@link ANeuralNetworksModel} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param model The model to be finished.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksModel_finish(ANeuralNetworksModel* model) __INTRODUCED_IN(27);

/**
 * Add an operand to a model.
 *
 * The order in which the operands are added is important. The first one added
 * to a model will have the index value 0, the second 1, etc. These indexes are
 * used as operand identifiers in
 * {@link ANeuralNetworksModel_addOperation},
 * {@link ANeuralNetworksModel_identifyInputsAndOutputs},
 * {@link ANeuralNetworksModel_setOperandValue},
 * {@link ANeuralNetworksModel_setOperandValueFromMemory},
 * {@link ANeuralNetworksExecution_setInput},
 * {@link ANeuralNetworksExecution_setInputFromMemory},
 * {@link ANeuralNetworksExecution_setOutput},
 * {@link ANeuralNetworksExecution_setOutputFromMemory} and
 * {@link ANeuralNetworksExecution_setOperandValue}.
 *
 * <p>Every operand must be referenced in exactly one of the following
 * ways:<ul>
 *    <li>It is identified as a model input with
 *        {@link ANeuralNetworksModel_identifyInputsAndOutputs}.</li>
 *    <li>It is identified as a constant with
 *        {@link ANeuralNetworksModel_setOperandValue} or
 *        {@link ANeuralNetworksModel_setOperandValueFromMemory}.</li>
 *    <li>It is identified as an output of exactly one operation with
 *        {@link ANeuralNetworksModel_addOperation}.</li></p>
 * <p>An operand that is identified as a model input or as a constant
 * must not also be identified as a model output with
 * {@link ANeuralNetworksModel_identifyInputsAndOutputs}.</p>
 *
 * To build a model that can accommodate inputs of various sizes, as
 * you may want to do for a CNN, leave unspecified the dimensions that
 * will vary at run time.  If you do so, fully specify dimensions
 * when calling {@link ANeuralNetworksExecution_setInput} or
 * {@link ANeuralNetworksExecution_setInputFromMemory}.
 *
 * Attempting to modify a model once {@link ANeuralNetworksModel_finish} has been
 * called will return an error.
 *
 * See {@link ANeuralNetworksModel} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param model The model to be modified.
 * @param type The {@link ANeuralNetworksOperandType} that describes the shape
 *             of the operand.  Neither the {@link ANeuralNetworksOperandType}
 *             nor the dimensions it points to need to outlive the call to
 *             {@link ANeuralNetworksModel_addOperand}.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksModel_addOperand(ANeuralNetworksModel* model,
                                    const ANeuralNetworksOperandType* type) __INTRODUCED_IN(27);

/**
 * Sets an operand to a constant value.
 *
 * Values of length smaller or equal to
 * {@link ANEURALNETWORKS_MAX_SIZE_OF_IMMEDIATELY_COPIED_VALUES}
 * are immediately copied into the model.
 *
 * For values of length greater than
 * {@link ANEURALNETWORKS_MAX_SIZE_OF_IMMEDIATELY_COPIED_VALUES}, a pointer to
 * the buffer is stored within the model. The application must not change the
 * content of this region until all executions using this model have
 * completed. As the data may be copied during processing, modifying the data
 * after this call yields undefined results. The provided buffer must outlive
 * this model.
 *
 * For large tensors, using {@link ANeuralNetworksModel_setOperandValueFromMemory}
 * is likely to be more efficient.
 *
 * To indicate that an optional operand should be considered missing,
 * pass nullptr for buffer and 0 for length.
 *
 * Attempting to modify a model once {@link ANeuralNetworksModel_finish} has been
 * called will return an error.
 *
 * See {@link ANeuralNetworksModel} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param model The model to be modified.
 * @param index The index of the model operand we're setting.
 * @param buffer A pointer to the data to use.
 * @param length The size in bytes of the data value.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksModel_setOperandValue(ANeuralNetworksModel* model, int32_t index,
                                         const void* buffer, size_t length) __INTRODUCED_IN(27);

#if __ANDROID_API__ >= 29

/**
 * Sets an operand's per channel quantization parameters.
 *
 * Sets parameters required by a tensor of type
 * {@link ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL}.
 * This function must be called for every tensor of type
 * {@link ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL} before
 * calling {@link ANeuralNetworksModel_finish}.
 *
 * Available since API level 29.
 *
 * @param model The model to be modified.
 * @param index The index of the model operand we're setting.
 * @param channelQuant The per channel quantization parameters for the operand.
 *                    No memory in this struct needs to outlive the call to
 *                    this function.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksModel_setOperandSymmPerChannelQuantParams(
        ANeuralNetworksModel* model, int32_t index,
        const ANeuralNetworksSymmPerChannelQuantParams* channelQuant) __INTRODUCED_IN(29);

#endif  // __ANDROID_API__ >= 29

/**
 * Sets an operand to a value stored in a memory object.
 *
 * The content of the memory is not copied. A reference to that memory is stored
 * inside the model. The application must not change the content of the memory
 * region until all executions using this model have completed.  As the data may
 * be copied during processing, modifying the data after this call yields
 * undefined results.
 *
 * <p>The provided memory must outlive this model.</p>
 *
 * To indicate that an optional operand should be considered missing,
 * use {@link ANeuralNetworksModel_setOperandValue} instead, passing nullptr for buffer.
 *
 * It is disallowed to set an operand value with shared memory backed by an AHardwareBuffer
 * of a format other than AHARDWAREBUFFER_FORMAT_BLOB.
 *
 * It is disallowed to set an operand value with memory created from
 * {@link ANeuralNetworksMemory_createFromDesc}.
 *
 * Attempting to modify a model once {@link ANeuralNetworksModel_finish} has been
 * called will return an error.
 *
 * See {@link ANeuralNetworksModel} for information on multithreaded usage.
 * See {@link ANeuralNetworksMemory_createFromAHardwareBuffer} for information on
 * AHardwareBuffer usage.
 *
 * Available since API level 27.
 *
 * @param model The model to be modified.
 * @param index The index of the model operand we're setting.
 * @param buffer A pointer to the data to use.
 * @param memory The memory containing the data.
 * @param offset This specifies the location of the data within the memory.
 *               The offset is in bytes from the start of memory.
 * @param length The size in bytes of the data value.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksModel_setOperandValueFromMemory(ANeuralNetworksModel* model, int32_t index,
                                                   const ANeuralNetworksMemory* memory,
                                                   size_t offset, size_t length)
        __INTRODUCED_IN(27);

#if __ANDROID_API__ >= 30

/**
 * Sets an operand to a value that is a reference to another NNAPI model.
 *
 * The referenced model must already have been finished by a call to
 * {@link ANeuralNetworksModel_finish}.
 *
 * The {@link ANeuralNetworksModel_relaxComputationFloat32toFloat16} setting of
 * referenced models is overridden by that setting of the main model of a
 * compilation.
 *
 * The referenced model must outlive the model referring to it.
 *
 * Attempting to modify a model once {@link ANeuralNetworksModel_finish} has
 * been called will return an error.
 *
 * See {@link ANeuralNetworksModel} for information on multithreaded usage.
 *
 * Available since API level 30.
 *
 * @param model The model to be modified.
 * @param index The index of the model operand we're setting.
 * @param value The model to be referenced.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksModel_setOperandValueFromModel(ANeuralNetworksModel* model, int32_t index,
                                                  const ANeuralNetworksModel* value)
        __INTRODUCED_IN(30);

#endif  // __ANDROID_API__ >= 30

/**
 * Add an operation to a model.
 *
 * @param model The model to be modified.
 * @param type The {@link ANeuralNetworksOperationType} of the operation.
 * @param inputCount The number of entries in the inputs array.
 * @param inputs An array of indexes identifying each operand.
 * @param outputCount The number of entries in the outputs array.
 * @param outputs An array of indexes identifying each operand.
 *
 * The operands specified by inputs and outputs must have been
 * previously added by calls to {@link ANeuralNetworksModel_addOperand}.
 *
 * Attempting to modify a model once {@link ANeuralNetworksModel_finish} has been
 * called will return an error.
 *
 * See {@link ANeuralNetworksModel} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksModel_addOperation(ANeuralNetworksModel* model,
                                      ANeuralNetworksOperationType type, uint32_t inputCount,
                                      const uint32_t* inputs, uint32_t outputCount,
                                      const uint32_t* outputs) __INTRODUCED_IN(27);

/**
 * Specifies which operands will be the model's inputs and
 * outputs. Every model must have at least one input and one output.
 *
 * An operand cannot be used for both input and output. Doing so will
 * return an error.
 *
 * @param model The model to be modified.
 * @param inputCount The number of entries in the inputs array.
 * @param inputs An array of indexes identifying the input operands.
 * @param outputCount The number of entries in the outputs array.
 * @param outputs An array of indexes identifying the output operands.
 *
 * The operands specified by inputs and outputs must have been
 * previously added by calls to {@link ANeuralNetworksModel_addOperand}.
 *
 * Attempting to modify a model once {@link ANeuralNetworksModel_finish} has been
 * called will return an error.
 *
 * See {@link ANeuralNetworksModel} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 */
int ANeuralNetworksModel_identifyInputsAndOutputs(ANeuralNetworksModel* model, uint32_t inputCount,
                                                  const uint32_t* inputs, uint32_t outputCount,
                                                  const uint32_t* outputs) __INTRODUCED_IN(27);

#if __ANDROID_API__ >= 28

/**
 * Specifies whether {@link ANEURALNETWORKS_TENSOR_FLOAT32} is allowed to be
 * calculated with range and/or precision as low as that of the IEEE 754 16-bit
 * floating-point format. By default, {@link ANEURALNETWORKS_TENSOR_FLOAT32}
 * must be calculated using at least the range and precision of the IEEE 754
 * 32-bit floating-point format.
 *
 * The relaxComputationFloat32toFloat16 setting of the main model of
 * a compilation overrides the values of the referenced models.
 *
 * @param model The model to be modified.
 * @param allow 'true' indicates {@link ANEURALNETWORKS_TENSOR_FLOAT32} may be
 *              calculated with range and/or precision as low as that of the
 *              IEEE 754 16-bit floating point format. 'false' indicates
 *              {@link ANEURALNETWORKS_TENSOR_FLOAT32} must be calculated using
 *              at least the range and precision of the IEEE 754 32-bit floating
 *              point format.
 *
 * Attempting to modify a model once {@link ANeuralNetworksModel_finish} has been
 * called will return an error.
 *
 * Available since API level 28.
 *
 * See {@link ANeuralNetworksModel} for information on multithreaded usage.
 */
int ANeuralNetworksModel_relaxComputationFloat32toFloat16(ANeuralNetworksModel* model, bool allow)
        __INTRODUCED_IN(28);

#endif  // __ANDROID_API__ >= 28

/**
 * Create a {@link ANeuralNetworksCompilation} to compile the given model.
 *
 * The model passed to this function is termed the "main model" of the
 * compilation, to distinguish it from other models referred to by an Operand
 * of type {@link ANEURALNETWORKS_MODEL} within this compilation.
 *
 * <p>This function only creates the object. Compilation is only performed once
 * {@link ANeuralNetworksCompilation_finish} is invoked.</p>
 *
 * <p>{@link ANeuralNetworksCompilation_finish} should be called once
 * all desired properties have been set on the compilation.</p>
 *
 * <p>{@link ANeuralNetworksModel_free} should be called once the compilation
 * is no longer needed.</p>
 *
 * <p>The provided model must outlive the compilation.</p>
 *
 * The model must already have been finished by a call to
 * {@link ANeuralNetworksModel_finish}.
 *
 * See {@link ANeuralNetworksCompilation} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param model The {@link ANeuralNetworksModel} to be compiled.
 * @param compilation The newly created object or NULL if unsuccessful.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful, ANEURALNETWORKS_BAD_DATA
 *         if the model is invalid.
 */
int ANeuralNetworksCompilation_create(ANeuralNetworksModel* model,
                                      ANeuralNetworksCompilation** compilation) __INTRODUCED_IN(27);

/**
 * Destroy a compilation.
 *
 * The compilation need not have been finished by a call to
 * {@link ANeuralNetworksCompilation_finish}.
 *
 * See {@link ANeuralNetworksCompilation} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param compilation The compilation to be destroyed. Passing NULL is acceptable and
 *                    results in no operation.
 */
void ANeuralNetworksCompilation_free(ANeuralNetworksCompilation* compilation) __INTRODUCED_IN(27);

/**
 * Sets the execution preference.
 *
 * <p>Provides guidance to the runtime when trade-offs are possible. By default the runtime
 * uses PREFER_SINGLE_FAST_ANSWER</p>
 *
 * See {@link ANeuralNetworksCompilation} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param compilation The compilation to be modified.
 * @param preference Either {@link PREFER_LOW_POWER},
 *                  {@link PREFER_SINGLE_FAST_ANSWER}, or
 *                  {@link PREFER_SUSTAINED_SPEED}.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksCompilation_setPreference(ANeuralNetworksCompilation* compilation,
                                             int32_t preference) __INTRODUCED_IN(27);

/**
 * Indicate that we have finished modifying a compilation. Required before
 * calling {@link ANeuralNetworksBurst_create} or
 * {@link ANeuralNetworksExecution_create}.
 *
 * An application must ensure that no other thread uses the compilation at the
 * same time.
 *
 * This function must only be called once for a given compilation.
 *
 * If {@link ANeuralNetworksCompilation_setTimeout} was called on this
 * compilation, and the compilation is not able to be finished before the
 * timeout duration is exceeded, then compilation may be aborted, in which case
 * {@link ANEURALNETWORKS_MISSED_DEADLINE_*} will be returned.
 *
 * See {@link ANeuralNetworksCompilation} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param compilation The compilation to be finished.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksCompilation_finish(ANeuralNetworksCompilation* compilation) __INTRODUCED_IN(27);

#if __ANDROID_API__ >= 30

/**
 * Set the execution priority.
 *
 * Execution priorities are relative to other executions created by the same
 * application (specifically same uid) for the same device. Specifically,
 * priorities of executions from one application will not affect executions from
 * another application. Similarly, priorities of executions on one device will
 * not affect executions on another device.
 *
 * Higher priority executions may use more compute resources than lower priority
 * executions, and may preempt or starve lower priority executions.
 *
 * See {@link ANeuralNetworksCompilation} for information on multithreaded usage.
 *
 * Available since API level 30.
 *
 * @param compilation The compilation to be modified.
 * @param priority The relative priority of the execution compared to other
 *     executions created by the application. Must be one of
 *     ANEURALNETWORKS_PRIORITY_*.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 */
int ANeuralNetworksCompilation_setPriority(ANeuralNetworksCompilation* compilation, int priority)
        __INTRODUCED_IN(30);

/**
 * Set the maximum expected duration for compiling the model.
 *
 * If the device is not able to complete the compilation within the specified
 * duration, the compilation may be aborted. The timeout duration begins at the
 * call to {@link ANeuralNetworksCompilation_finish}.
 *
 * This timeout duration acts as a hint to drivers, and can be used to both free
 * up compute resources within the driver and return control back to the
 * application quicker than is possible without the hint. It enables drivers
 * that are able to estimate how long a compilation will take to abort the
 * compilation before it has even started if the driver believes the compilation
 * cannot be completed within the timeout duration. Similarly, it enables
 * drivers to abort an ongoing compilation if it is taking too long. However,
 * this call does not guarantee that the compilation will complete or abort
 * within the timeout duration.
 *
 * By default (i.e., unless ANeuralNetworksCompilation_setTimeout is called),
 * the timeout duration for compiling the model is considered infinite.
 *
 * The {@link ANeuralNetworksCompilation} must have been created with
 * {@link ANeuralNetworksCompilation_createForDevices} with numDevices = 1,
 * otherwise this function will fail with ANEURALNETWORKS_BAD_DATA. If the
 * device has a feature level reported by
 * {@link ANeuralNetworksDevice_getFeatureLevel} that is lower than 30, then the
 * timeout duration hint will be ignored.
 *
 * See {@link ANeuralNetworksCompilation} for information on multithreaded usage.
 *
 * @param compilation The compilation to be modified.
 * @param duration The maximum amount of time in nanoseconds that is expected to
 *     be spent finishing a compilation. If this duration is exceeded, the
 *     compilation may be aborted. If set to 0, the timeout duration is
 *     considered infinite.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 30.
 */
int ANeuralNetworksCompilation_setTimeout(ANeuralNetworksCompilation* compilation,
                                          uint64_t duration) __INTRODUCED_IN(30);

#endif  // __ANDROID_API__ >= 30

/**
 * Create a {@link ANeuralNetworksExecution} to apply the given compilation.
 * This only creates the object. Computation is only performed once
 * {@link ANeuralNetworksExecution_burstCompute},
 * {@link ANeuralNetworksExecution_compute},
 * {@link ANeuralNetworksExecution_startCompute} or
 * {@link ANeuralNetworksExecution_startComputeWithDependencies} is invoked.
 *
 * <p>The provided compilation must outlive the execution.</p>
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param compilation The {@link ANeuralNetworksCompilation} to be evaluated.
 * @param execution The newly created object or NULL if unsuccessful.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful, ANEURALNETWORKS_BAD_DATA
 *         if the compilation is invalid.
 */
int ANeuralNetworksExecution_create(ANeuralNetworksCompilation* compilation,
                                    ANeuralNetworksExecution** execution) __INTRODUCED_IN(27);

/**
 * Destroy an execution.
 *
 * <p>The execution need not have been scheduled by a call to
 * {@link ANeuralNetworksExecution_burstCompute},
 * {@link ANeuralNetworksExecution_compute},
 * {@link ANeuralNetworksExecution_startCompute} or
 * {@link ANeuralNetworksExecution_startComputeWithDependencies}; but if it has been scheduled,
 * then the application must not call {@link ANeuralNetworksExecution_free}
 * until the execution has completed (i.e.,
 * {@link ANeuralNetworksExecution_burstCompute},
 * {@link ANeuralNetworksExecution_compute}, or
 * {@link ANeuralNetworksEvent_wait} has returned).
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param execution The execution to be destroyed. Passing NULL is acceptable and
 *                  results in no operation.
 */
void ANeuralNetworksExecution_free(ANeuralNetworksExecution* execution) __INTRODUCED_IN(27);

/**
 * Associate a user buffer with an input of the model of the
 * {@link ANeuralNetworksExecution}. Evaluation of the execution must not have
 * been scheduled. Once evaluation of the execution has been scheduled, the
 * application must not change the content of the buffer until the execution has
 * completed. Evaluation of the execution will not change the content of the
 * buffer.
 *
 * <p>The provided buffer must outlive the execution.</p>
 *
 * If the input is optional, you can indicate that it is omitted by
 * passing nullptr for buffer and 0 for length.
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param execution The execution to be modified.
 * @param index The index of the input argument we are setting. It is
 *              an index into the lists passed to
 *              {@link ANeuralNetworksModel_identifyInputsAndOutputs}. It is not
 *              the index associated with
 *              {@link ANeuralNetworksModel_addOperand}.
 * @param type The {@link ANeuralNetworksOperandType} of the
 *             operand. Unless the input is omitted, this should be
 *             used to specify the dimensions that were left
 *             unspecified when the operand was added to the
 *             model. All other properties of the type must be the
 *             same as specified in the model. If the type is the same
 *             as specified when the model was built, NULL can be
 *             passed. Neither the {@link ANeuralNetworksOperandType}
 *             nor the dimensions it points to need to outlive the call
 *             to {@link ANeuralNetworksExecution_setInput}.
 * @param buffer The buffer containing the data.
 * @param length The length in bytes of the buffer.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful, ANEURALNETWORKS_BAD_DATA if the
 *         name is not recognized or the buffer is too small for the input.
 */
int ANeuralNetworksExecution_setInput(ANeuralNetworksExecution* execution, int32_t index,
                                      const ANeuralNetworksOperandType* type, const void* buffer,
                                      size_t length) __INTRODUCED_IN(27);

/**
 * Associate a region of a memory object with an input of the model of the
 * {@link ANeuralNetworksExecution}. Evaluation of the execution must not have
 * been scheduled. Once evaluation of the execution has been scheduled, the
 * application must not change the content of the region until the execution has
 * completed. Evaluation of the execution will not change the content of the
 * region.
 *
 * <p>The provided memory must outlive the execution.</p>
 *
 * If the input is optional, you can indicate that it is omitted by
 * using {@link ANeuralNetworksExecution_setInput} instead, passing nullptr for
 * buffer and 0 for length.
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 * See {@link ANeuralNetworksMemory_createFromAHardwareBuffer} for information on
 * AHardwareBuffer usage.
 * See {@link ANeuralNetworksMemory_createFromDesc} for information on usage of memory objects
 * created from memory descriptors.
 *
 * Available since API level 27.
 *
 * @param execution The execution to be modified.
 * @param index The index of the input argument we are setting. It is
 *              an index into the lists passed to
 *              {@link ANeuralNetworksModel_identifyInputsAndOutputs}. It is not
 *              the index associated with {@link ANeuralNetworksModel_addOperand}.
 * @param type The {@link ANeuralNetworksOperandType} of the
 *             operand. This should be used to specify the dimensions
 *             that were left unspecified when the operand was added
 *             to the model. All other properties of the type must be
 *             the same as specified in the model. If the type is the
 *             same as specified when the model was built, NULL can be
 *             passed. Neither the {@link ANeuralNetworksOperandType}
 *             nor the dimensions it points to need to outlive the call
 *             to {@link ANeuralNetworksExecution_setInputFromMemory}.
 * @param memory The memory containing the data.
 * @param offset This specifies the location of the data within the memory.
 *               The offset is in bytes from the start of memory.
 * @param length The size in bytes of the data value.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful, ANEURALNETWORKS_BAD_DATA if the
 *         name is not recognized or the buffer is too small for the input.
 */
int ANeuralNetworksExecution_setInputFromMemory(ANeuralNetworksExecution* execution, int32_t index,
                                                const ANeuralNetworksOperandType* type,
                                                const ANeuralNetworksMemory* memory, size_t offset,
                                                size_t length) __INTRODUCED_IN(27);

/**
 * Associate a user buffer with an output of the model of the
 * {@link ANeuralNetworksExecution}. Evaluation of the execution must not have
 * been scheduled. Once evaluation of the execution has been scheduled, the
 * application must not change the content of the buffer until the execution has
 * completed.
 *
 * If the output is optional, you can indicate that it is omitted by
 * passing nullptr for buffer and 0 for length.
 *
 * <p>The provided buffer must outlive the execution.</p>
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param execution The execution to be modified.
 * @param index The index of the output argument we are setting. It is
 *              an index into the lists passed to
 *              {@link ANeuralNetworksModel_identifyInputsAndOutputs}. It is not
 *              the index associated with {@link ANeuralNetworksModel_addOperand}.
 * @param type The {@link ANeuralNetworksOperandType} of the
 *             operand. Unless the output is omitted, this should be
 *             used to specify the dimensions that were left
 *             unspecified when the operand was added to the
 *             model. All other properties of the type must be the
 *             same as specified in the model. If the type is the same
 *             as specified when the model was built, NULL can be
 *             passed. Neither the {@link ANeuralNetworksOperandType}
 *             nor the dimensions it points to need to outlive the call
 *             to {@link ANeuralNetworksExecution_setOutput}.
 *             Since API level 29, the output operand can have unspecified
 *             dimensions or rank to be deduced dynamically during the execution.
 *             However, the user must provide a large enough buffer. The user
 *             can retrieve the output dimensional information after the execution
 *             by {@link ANeuralNetworksExecution_getOutputOperandRank} and
 *             {@link ANeuralNetworksExecution_getOutputOperandDimensions}.
 * @param buffer The buffer where the data is to be written.
 * @param length The length in bytes of the buffer.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful, ANEURALNETWORKS_BAD_DATA if the
 *         name is not recognized or the buffer is too small for the output.
 */
int ANeuralNetworksExecution_setOutput(ANeuralNetworksExecution* execution, int32_t index,
                                       const ANeuralNetworksOperandType* type, void* buffer,
                                       size_t length) __INTRODUCED_IN(27);

/**
 * Associate a region of a memory object with an output of the model of the
 * {@link ANeuralNetworksExecution}. Evaluation of the execution must not have
 * been scheduled. Once evaluation of the execution has been scheduled, the
 * application must not change the content of the region until the execution has
 * completed.
 *
 * If the output is optional, you can indicate that it is omitted by
 * using {@link ANeuralNetworksExecution_setOutput} instead, passing nullptr for
 * buffer and 0 for length.
 *
 * <p>The provided memory must outlive the execution.</p>
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 * See {@link ANeuralNetworksMemory_createFromAHardwareBuffer} for information on
 * AHardwareBuffer usage.
 * See {@link ANeuralNetworksMemory_createFromDesc} for information on usage of memory objects
 * created from memory descriptors.
 *
 * Available since API level 27.
 *
 * @param execution The execution to be modified.
 * @param index The index of the output argument we are setting. It is
 *              an index into the lists passed to
 *              {@link ANeuralNetworksModel_identifyInputsAndOutputs}. It is not
 *              the index associated with {@link ANeuralNetworksModel_addOperand}.
 * @param type The {@link ANeuralNetworksOperandType} of the operand. This should be
 *             used to specify the dimensions that were left
 *             unspecified when the operand was added to the
 *             model. All other properties of the type must be the
 *             same as specified in the model. If the type is the same
 *             as specified when the model was built, NULL can be
 *             passed. Neither the {@link ANeuralNetworksOperandType}
 *             nor the dimensions it points to need to outlive the call
 *             to {@link ANeuralNetworksExecution_setOutputFromMemory}.
 *             Since API level 29, the output operand can have unspecified
 *             dimensions or rank to be deduced dynamically during the execution.
 *             However, the user must provide a large enough memory. The user
 *             can retrieve the output dimensional information after the execution
 *             by {@link ANeuralNetworksExecution_getOutputOperandRank} and
 *             {@link ANeuralNetworksExecution_getOutputOperandDimensions}.
 * @param memory The memory where the data is to be stored.
 * @param offset This specifies the location of the data within the memory.
 *               The offset is in bytes from the start of memory.
 * @param length The length in bytes of the data value.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful, ANEURALNETWORKS_BAD_DATA if the
 *         name is not recognized or the buffer is too small for the output.
 */
int ANeuralNetworksExecution_setOutputFromMemory(ANeuralNetworksExecution* execution, int32_t index,
                                                 const ANeuralNetworksOperandType* type,
                                                 const ANeuralNetworksMemory* memory, size_t offset,
                                                 size_t length) __INTRODUCED_IN(27);

/**
 * Schedule asynchronous evaluation of the execution.
 *
 * <p>Schedules asynchronous evaluation of the execution. Once the execution
 * has completed and the outputs are ready to be consumed, the returned event
 * will be signaled. Use {@link ANeuralNetworksEvent_wait} to wait for that
 * event.
 * </p>
 *
 * ANeuralNetworksEvent_wait must be called to recuperate the resources used
 * by the execution.
 *
 * If {@link ANeuralNetworksExecution_setTimeout} was called on this execution,
 * and the execution is not able to complete before the timeout duration is
 * exceeded, then execution may be aborted, in which case
 * {@link ANEURALNETWORKS_MISSED_DEADLINE_*} will be returned through
 * {@link ANeuralNetworksExecution_startCompute} or
 * {@link ANeuralNetworksEvent_wait} on the event object. If the device has a
 * feature level reported by {@link ANeuralNetworksDevice_getFeatureLevel} that
 * is lower than 30, then the timeout duration hint will be ignored.
 *
 * If this execution contains a {@link ANEURALNETWORKS_WHILE} operation, and
 * the condition model does not output false within the loop timeout duration,
 * then execution will be aborted and {@link ANEURALNETWORKS_MISSED_DEADLINE_*}
 * will be returned through {@link ANeuralNetworksEvent_wait} on the event
 * object.
 *
 * If the device can detect before the execution has started that the execution
 * will not complete within the timeout duration, the device may choose to skip
 * the execution and instead return {@link ANEURALNETWORKS_MISSED_DEADLINE_*}.
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * See {@link ANeuralNetworksExecution_compute} for synchronous execution.
 * See {@link ANeuralNetworksExecution_burstCompute} for burst synchronous execution.
 * See {@link ANeuralNetworksExecution_startComputeWithDependencies} for
 * asynchronous execution with dependencies.
 *
 * Available since API level 27.
 *
 * @param execution The execution to be scheduled and executed.
 * @param event The event that will be signaled on completion. event is set to
 *              NULL if there's an error.
 *
 * @return ANEURALNETWORKS_NO_ERROR if the evaluation is successfully scheduled.
 */
int ANeuralNetworksExecution_startCompute(ANeuralNetworksExecution* execution,
                                          ANeuralNetworksEvent** event) __INTRODUCED_IN(27);

#if __ANDROID_API__ >= 30

/**
 * Set the maximum expected duration of the specified execution.
 *
 * If the device is not able to complete the execution within the specified
 * duration, the execution may be aborted. The timeout duration begins at a
 * call to one of:
 * - {@link ANeuralNetworksExecution_burstCompute}
 * - {@link ANeuralNetworksExecution_compute}
 * - {@link ANeuralNetworksExecution_startCompute}
 * - {@link ANeuralNetworksExecution_startComputeWithDependencies}
 *
 * This timeout duration acts as a hint to drivers, and can be used to both free
 * up compute resources within the driver and return control back to the
 * application quicker than is possible without the hint. It enables drivers
 * that are able to estimate how long an execution will take to abort the
 * execution before it has even started if the driver believes the execution
 * cannot be completed within the timeout duration. Similarly, it enables
 * drivers to abort an ongoing execution if it is taking too long. However, this
 * call does not guarantee that the execution will complete or abort within the
 * timeout duration.
 *
 * By default (i.e., unless ANeuralNetworksExecution_setTimeout is called),
 * the timeout duration for execution is considered infinite.
 *
 * The {@link ANeuralNetworksExecution} must have been created from an
 * {@link ANeuralNetworksCompilation} which in turn was created from
 * {@link ANeuralNetworksCompilation_createForDevices} with numDevices = 1,
 * otherwise this function will fail with ANEURALNETWORKS_BAD_DATA. If the
 * device has a feature level reported by
 * {@link ANeuralNetworksDevice_getFeatureLevel} that is lower than 30, then the
 * timeout duration hint will be ignored.
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * @param execution The execution to be modified.
 * @param duration The maximum amount of time in nanoseconds that is expected to
 *     be spent executing a model. If this duration is exceeded, the execution
 *     may be aborted. If set to 0, the timeout duration is considered infinite.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 30.
 */
int ANeuralNetworksExecution_setTimeout(ANeuralNetworksExecution* execution, uint64_t duration)
        __INTRODUCED_IN(30);

/**
 * Set the maximum duration of WHILE loops in the specified execution.
 *
 * This is a fuzzy per-loop timeout intended to prevent infinite loops.
 *
 * If a WHILE loop condition model does not output false within the specified
 * duration, the execution will be aborted.
 *
 * See {@link ANeuralNetworks_getDefaultLoopTimeout} and
 * {@link ANeuralNetworks_getMaximumLoopTimeout} for the default
 * and maximum timeout values.
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * @param execution The execution to be modified.
 * @param duration The maximum amount of time in nanoseconds that can be spent
 *     executing a WHILE loop. If the specified duration value exceeds the value
 *     produced by {@link ANeuralNetworks_getMaximumLoopTimeout}, it will be
 *     overridden by that value.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *         ANEURALNETWORKS_BAD_STATE if execution has started.
 *         ANEURALNETWORKS_UNEXPECTED_NULL if execution is NULL.
 *
 * Available since API level 30.
 */
int ANeuralNetworksExecution_setLoopTimeout(ANeuralNetworksExecution* execution, uint64_t duration)
        __INTRODUCED_IN(30);

/**
 * Get the default timeout value for WHILE loops.
 *
 * @return The default timeout value in nanoseconds.
 *
 * Available since API level 30.
 */
uint64_t ANeuralNetworks_getDefaultLoopTimeout() __INTRODUCED_IN(30);

/**
 * Get the maximum timeout value for WHILE loops.
 *
 * @return The maximum timeout value in nanoseconds.
 *
 * Available since API level 30.
 */
uint64_t ANeuralNetworks_getMaximumLoopTimeout() __INTRODUCED_IN(30);

#endif  // __ANDROID_API__ >= 30

/**
 * Waits until the execution completes.
 *
 * More than one thread can wait on an event. When the execution completes,
 * all threads will be released.
 *
 * If {@link ANeuralNetworksExecution_setTimeout} was called on the execution
 * corresponding to this event, and the execution is not able to complete
 * before the duration is exceeded, the execution may be aborted, in which case
 * {@link ANEURALNETWORKS_MISSED_DEADLINE_*} will be returned here.
 *
 * If the execution contains a {@link ANEURALNETWORKS_WHILE} operation, and
 * the condition model does not output false within the loop timeout duration,
 * the execution will be aborted, and {@link ANEURALNETWORKS_MISSED_DEADLINE_*}
 * will be returned here.
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param event The event that will be signaled on completion.
 * @return ANEURALNETWORKS_NO_ERROR if the execution completed normally.
 *         ANEURALNETWORKS_UNMAPPABLE if the execution input or output memory cannot
 *         be properly mapped.
 */
int ANeuralNetworksEvent_wait(ANeuralNetworksEvent* event) __INTRODUCED_IN(27);

/**
 * Destroys the event.
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * Available since API level 27.
 *
 * @param event The event object to be destroyed. Passing NULL is acceptable and
 *              results in no operation.
 */
void ANeuralNetworksEvent_free(ANeuralNetworksEvent* event) __INTRODUCED_IN(27);

#endif  // __ANDROID_API__ >= 27

#if __ANDROID_API__ >= 30
/**
 * Create a {@link ANeuralNetworksEvent} from a sync_fence file descriptor.
 *
 * The newly created ANeuralNetworksEvent does not take ownership of the provided sync_fence_fd,
 * it will instead dup the provided sync_fence_fd and own the duplicate.
 *
 * @param sync_fence_fd The sync_fence file descriptor.
 * @param event The newly created object or NULL if unsuccessful.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 30.
 */
int ANeuralNetworksEvent_createFromSyncFenceFd(int sync_fence_fd, ANeuralNetworksEvent** event)
        __INTRODUCED_IN(30);

/**
 * Get sync_fence file descriptor from the event.
 *
 * If the ANeuralNetworksEvent is not backed by a sync fence, the sync_fence_fd
 * will be set to -1, and ANEURALNETWORKS_BAD_DATA will be returned.
 *
 * See {@link ANeuralNetworksEvent_createFromSyncFenceFd} and
 * {@link ANeuralNetworksExecution_startComputeWithDependencies} to see how to create
 * an event backed by a sync fence.
 *
 * The user takes ownership of the returned fd, and must close the returned file descriptor when
 * it is no longer needed.
 *
 * @param event An event that is backed by a sync fence.
 * @param sync_fence_fd The sync_fence file descriptor. The file descriptor will
 *                      be set to -1 if there is an error.
 *
 * @return ANEURALNETWORKS_NO_ERROR if successful.
 *
 * Available since API level 30.
 */
int ANeuralNetworksEvent_getSyncFenceFd(const ANeuralNetworksEvent* event, int* sync_fence_fd)
        __INTRODUCED_IN(30);

/**
 * Schedule asynchronous evaluation of the execution with dependencies.
 *
 * The execution will wait for all the depending events to be signaled before
 * starting the evaluation. Once the execution has completed and the outputs
 * are ready to be consumed, the returned event will be signaled. Depending on which
 * devices are handling the execution, the event could be backed by a sync fence.
 * Use {@link ANeuralNetworksEvent_wait} to wait for that event.
 *
 * ANeuralNetworksEvent_wait must be called to recurperate the resources used
 * by the execution.
 *
 * If parts of the execution are scheduled on devices that do not support fenced execution,
 * the function call may wait for such parts to finish before returning.
 *
 * The function will return an error if any of the events in dependencies is already in a bad
 * state. After the execution is scheduled, if any of the events in dependencies does not complete
 * normally, the execution will fail, and {@link ANeuralNetworksEvent_wait} on the returned
 * event will return an error.
 *
 * The function will return an error if any of the execution outputs has a tensor operand type
 * that is not fully specified.
 *
 * The function can be passed a timeout duration in nanoseconds. This timeout
 * duration acts as a hint to drivers in the same way that the timeout durations
 * in {@link ANeuralNetworksCompilation_setTimeout} and {@link
 * ANeuralNetworksExecution_setTimeout} act as hints to drivers. The duration
 * begins when all waitFor sync fences have been signaled, and can be used
 * together with {@link ANeuralNetworksExecution_setTimeout} which specifies the
 * maximum timeout duration beginning at the call to
 * {@link ANeuralNetworksExecution_startComputeWithDependencies}.
 * If the duration is non-zero, the {@link ANeuralNetworksExecution} must have been created
 * from an {@link ANeuralNetworksCompilation} which in turn was created from
 * {@link ANeuralNetworksCompilation_createForDevices} with numDevices = 1,
 * otherwise this function will fail with ANEURALNETWORKS_BAD_DATA. If either
 * the timeout duration from {@link ANeuralNetworksExecution_setTimeout} or the
 * timeout duration passed to this call is exceeded, the execution may be
 * aborted, in which case {@link ANEURALNETWORKS_MISSED_DEADLINE_*} will be
 * returned through {@link ANeuralNetworksExecution_startComputeWithDependencies}
 * or {@link ANeuralNetworksEvent_wait} on the event object. If the device has a
 * feature level reported by {@link ANeuralNetworksDevice_getFeatureLevel} that
 * is lower than 30, then the timeout duration hints will be ignored.
 *
 * If this execution contains a {@link ANEURALNETWORKS_WHILE} operation, and
 * the condition model does not output false within the loop timeout duration,
 * then execution will be aborted and {@link ANEURALNETWORKS_MISSED_DEADLINE_*}
 * will be returned through {@link ANeuralNetworksEvent_wait} on the event
 * object.
 *
 * See {@link ANeuralNetworksExecution} for information on multithreaded usage.
 *
 * See {@link ANeuralNetworksExecution_compute} for synchronous execution.
 * See {@link ANeuralNetworksExecution_burstCompute} for burst synchronous execution.
 * See {@link ANeuralNetworksExecution_startCompute} for regular asynchronous execution.
 *
 * @param execution The execution to be scheduled and executed.
 * @param dependencies A set of depending events. The actual evaluation will not start
 *                     until all the events are signaled.
 * @param num_dependencies The number of events in the dependencies set.
 * @param duration The maximum amount of time in nanoseconds that is expected to
 *                 be spent executing the model after all dependencies are
 *                 signaled. If set to 0, the timeout duration is considered
 *                 infinite.
 * @param event The event that will be signaled on completion. event is set to
 *              NULL if there's an error.
 *
 * @return ANEURALNETWORKS_NO_ERROR if the evaluation is successfully scheduled.
 *
 * Available since API level 30.
 */
int ANeuralNetworksExecution_startComputeWithDependencies(
        ANeuralNetworksExecution* execution, const ANeuralNetworksEvent* const* dependencies,
        uint32_t num_dependencies, uint64_t duration, ANeuralNetworksEvent** event)
        __INTRODUCED_IN(30);

#endif  // __ANDROID_API__ >= 30

__END_DECLS

#endif  // ANDROID_FRAMEWORKS_ML_NN_RUNTIME_NEURAL_NETWORKS_H

/** @} */
