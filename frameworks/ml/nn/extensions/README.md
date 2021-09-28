# Vendor Extensions

Neural Networks API (NNAPI) vendor extensions, introduced in Android 10, are
collections of vendor-defined operations and data types. On devices running NN
HAL 1.2 or higher, drivers can provide custom hardware-accelerated operations by
supporting corresponding vendor extensions. Vendor extensions don't modify the
behavior of existing operations.

Vendor extensions provide a more structured alternative to OEM operation and
data types, which were deprecated in Android 10.
For more information, see the last section.

## Extensions usage allowlist

Vendor extensions can only be used by explicitly specified Android apps and
native binaries on the `/product`, `/vendor`, `/odm`, and `/data` partitions.
Apps and native binaries located on the `/system` partition can't use vendor
extensions.

A list of Android apps and binaries permitted to use NNAPI vendor extensions is
stored in `/vendor/etc/nnapi_extensions_app_allowlist`. Each line of the file
contains a new entry. An entry can be a native binary path that is prefixed with
a slash (/), for example, `/data/foo`, or a name of an Android app package, for
example, `com.foo.bar`.

The allowlist is enforced from the NNAPI runtime shared library. This library
protects against accidental usage but not against deliberate circumvention by
an app directly using the NNAPI driver HAL interface.

## Vendor extension definition

The vendor creates and maintains a header file with the extension definition. A
complete example of an extension definition can be found in
`example/fibonacci/FibonacciExtension.h`.

Each extension must have a unique name that starts with the reverse domain name
of the vendor.

Note: Names of extension operation and operand types must be qualified with a
vendor name. The vendor name is represented by `EXAMPLE` in the code samples on
this page.

```
const char EXAMPLE_EXTENSION_NAME[] = "com.example.my_extension";
```

The name acts as a namespace for operations and data types. The NNAPI uses this
name to distinguish between vendor extensions.

Operations and data types are declared in a way similar to those in
`runtime/include/NeuralNetworks.h`.

```
enum {
    /**
     * A custom scalar type.
     */
    EXAMPLE_SCALAR = 0,

    /**
     * A custom tensor type.
     *
     * Attached to this tensor is {@link ExampleTensorParams}.
     */
    EXAMPLE_TENSOR = 1,
};

enum {
    /**
     * Computes example function.
     *
     * Inputs:
     * * 0: A scalar of {@link EXAMPLE_SCALAR}.
     *
     * Outputs:
     * * 0: A tensor of {@link EXAMPLE_TENSOR}.
     */
    EXAMPLE_FUNCTION = 0,
};
```

An extension operation can use any operand type, including nonextension operand
types and operand types from other extensions. When using an operand type from
another extension, the driver must support the other extension.

Extensions can also declare custom structures to accompany extension operands.

```
/**
 * Quantization parameters for {@link EXAMPLE_TENSOR}.
 */
typedef struct ExampleTensorParams {
    double scale;
    int64_t zeroPoint;
} ExampleTensorParams;
```

## Using extensions in NNAPI clients

The `runtime/include/NeuralNetworksExtensions.h` (C API) file provides runtime
extension support. This section provides an overview of the C API.

To check whether a device supports an extension, use
`ANeuralNetworksDevice_getExtensionSupport`.

```
bool isExtensionSupported;
CHECK_EQ(ANeuralNetworksDevice_getExtensionSupport(device, EXAMPLE_EXTENSION_NAME, &isExtensionSupported),
         ANEURALNETWORKS_NO_ERROR);
if (isExtensionSupported) {
    // The device supports the extension.
    ...
}
```

To build a model with an extension operand, use `ANeuralNetworksModel_getExtensionOperandType`
to obtain the operand type and call `ANeuralNetworksModel_addOperand`.

```
int32_t type;
CHECK_EQ(ANeuralNetworksModel_getExtensionOperandType(model, EXAMPLE_EXTENSION_NAME, EXAMPLE_TENSOR, &type),
         ANEURALNETWORKS_NO_ERROR);
ANeuralNetworksOperandType operandType{
        .type = type,
        .dimensionCount = dimensionCount,
        .dimensions = dimensions,
};
CHECK_EQ(ANeuralNetworksModel_addOperand(model, &operandType), ANEURALNETWORKS_NO_ERROR);
```

Optionally, use
`ANeuralNetworksModel_setOperandExtensionData`
to associate additional data with an extension operand.

```
ExampleTensorParams params{
        .scale = 0.5,
        .zeroPoint = 128,
};
CHECK_EQ(ANeuralNetworksModel_setOperandExtensionData(model, operandIndex, &params, sizeof(params)),
         ANEURALNETWORKS_NO_ERROR);
```

To build a model with an extension operation, use
`ANeuralNetworksModel_getExtensionOperationType`
to obtain the operation type and call
`ANeuralNetworksModel_addOperation`.

```
ANeuralNetworksOperationType type;
CHECK_EQ(ANeuralNetworksModel_getExtensionOperationType(model, EXAMPLE_EXTENSION_NAME, EXAMPLE_FUNCTION, &type),
         ANEURALNETWORKS_NO_ERROR);
CHECK_EQ(ANeuralNetworksModel_addOperation(model, type, inputCount, inputs, outputCount, outputs),
         ANEURALNETWORKS_NO_ERROR);
```

## Adding extension support to an NNAPI driver

Drivers report supported extensions through the `IDevice::getSupportedExtensions`
method. The returned list must contain an entry describing each supported
extension.

```
Extension {
    .name = EXAMPLE_EXTENSION_NAME,
    .operandTypes = {
        {
            .type = EXAMPLE_SCALAR,
            .isTensor = false,
            .byteSize = 8,
        },
        {
            .type = EXAMPLE_TENSOR,
            .isTensor = true,
            .byteSize = 8,
        },
    },
}
```

Of the 32 bits used to identify types and operations, the high
`ExtensionTypeEncoding::HIGH_BITS_PREFIX` bits is the extension
_prefix_ and the low `ExtensionTypeEncoding::LOW_BITS_TYPE`
bits represents the type or operation of the extension.

When handling an operation or operand type, the driver must check the extension
prefix. If the extension prefix has a nonzero value, the operation or operand
type is an extension type. If the value is `0`, the operation or operand type
isn't an extension type.

To map the prefix to an extension name, look it up in `model.extensionNameToPrefix`.
The mapping from the prefix to the extension name is a one-to-one correspondence
(bijection) for a given model. Different prefix values might correspond to the
same extension name in different models.

The driver must validate extension operations and data types because the NNAPI
runtime can't validate particular extension operations and data types.

Extension operands can have associated data in `operand.extraParams.extension`,
which the runtime treats as a raw data blob of arbitrary size.

## OEM operation and data types

Note: OEM operation and data types have been deprecated. For devices running
Android 10 or higher, use vendor extensions instead.

NNAPI has an OEM operation and OEM data types to allow
device manufacturers to provide custom, driver-specific functionality. These
operation and data types are only used by OEM applications. The semantics of OEM
operation and data types are OEM-specific and can change at any time. The OEM
operation and data types are encoded using `OperationType::OEM_OPERATION`,
`OperandType::OEM`, and `OperandType::TENSOR_OEM_BYTE`.
