This folder contains FlatBuffers schema used in the communications protocol
between the power test nanoapp and the host (applications processor) test code.

Use the included update.sh script to generate the header files used in CHRE,
which requires that the FlatBuffers compiler `flatc` be available in $PATH.

FlatBuffers compiler version 1.6.0 must be used since some modifications are
made to the version of flatbuffers header used by the generated code.

For more information on FlatBuffers, see https://github.com/google/flatbuffers/
