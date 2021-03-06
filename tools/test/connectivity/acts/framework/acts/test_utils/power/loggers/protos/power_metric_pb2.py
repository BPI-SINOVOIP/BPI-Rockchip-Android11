# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: power_metric.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
from google.protobuf import descriptor_pb2
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='power_metric.proto',
  package='wireless.android.platform.testing.power.metrics',
  syntax='proto2',
  serialized_pb=_b('\n\x12power_metric.proto\x12/wireless.android.platform.testing.power.metrics\"\xc2\x01\n\x0bPowerMetric\x12\x11\n\tavg_power\x18\x01 \x01(\x02\x12\x0f\n\x07testbed\x18\x02 \x01(\t\x12]\n\x0f\x63\x65llular_metric\x18\x03 \x01(\x0b\x32\x44.wireless.android.platform.testing.power.metrics.PowerCellularMetric\x12\x0e\n\x06\x62ranch\x18\x04 \x01(\t\x12\x10\n\x08\x62uild_id\x18\x05 \x01(\t\x12\x0e\n\x06target\x18\x06 \x01(\t\"?\n\x13PowerCellularMetric\x12\x13\n\x0b\x61vg_dl_tput\x18\x01 \x01(\x02\x12\x13\n\x0b\x61vg_ul_tput\x18\x02 \x01(\x02')
)




_POWERMETRIC = _descriptor.Descriptor(
  name='PowerMetric',
  full_name='wireless.android.platform.testing.power.metrics.PowerMetric',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='avg_power', full_name='wireless.android.platform.testing.power.metrics.PowerMetric.avg_power', index=0,
      number=1, type=2, cpp_type=6, label=1,
      has_default_value=False, default_value=float(0),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='testbed', full_name='wireless.android.platform.testing.power.metrics.PowerMetric.testbed', index=1,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='cellular_metric', full_name='wireless.android.platform.testing.power.metrics.PowerMetric.cellular_metric', index=2,
      number=3, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='branch', full_name='wireless.android.platform.testing.power.metrics.PowerMetric.branch', index=3,
      number=4, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='build_id', full_name='wireless.android.platform.testing.power.metrics.PowerMetric.build_id', index=4,
      number=5, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='target', full_name='wireless.android.platform.testing.power.metrics.PowerMetric.target', index=5,
      number=6, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto2',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=72,
  serialized_end=266,
)


_POWERCELLULARMETRIC = _descriptor.Descriptor(
  name='PowerCellularMetric',
  full_name='wireless.android.platform.testing.power.metrics.PowerCellularMetric',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='avg_dl_tput', full_name='wireless.android.platform.testing.power.metrics.PowerCellularMetric.avg_dl_tput', index=0,
      number=1, type=2, cpp_type=6, label=1,
      has_default_value=False, default_value=float(0),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='avg_ul_tput', full_name='wireless.android.platform.testing.power.metrics.PowerCellularMetric.avg_ul_tput', index=1,
      number=2, type=2, cpp_type=6, label=1,
      has_default_value=False, default_value=float(0),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto2',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=268,
  serialized_end=331,
)

_POWERMETRIC.fields_by_name['cellular_metric'].message_type = _POWERCELLULARMETRIC
DESCRIPTOR.message_types_by_name['PowerMetric'] = _POWERMETRIC
DESCRIPTOR.message_types_by_name['PowerCellularMetric'] = _POWERCELLULARMETRIC
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

PowerMetric = _reflection.GeneratedProtocolMessageType('PowerMetric', (_message.Message,), dict(
  DESCRIPTOR = _POWERMETRIC,
  __module__ = 'power_metric_pb2'
  # @@protoc_insertion_point(class_scope:wireless.android.platform.testing.power.metrics.PowerMetric)
  ))
_sym_db.RegisterMessage(PowerMetric)

PowerCellularMetric = _reflection.GeneratedProtocolMessageType('PowerCellularMetric', (_message.Message,), dict(
  DESCRIPTOR = _POWERCELLULARMETRIC,
  __module__ = 'power_metric_pb2'
  # @@protoc_insertion_point(class_scope:wireless.android.platform.testing.power.metrics.PowerCellularMetric)
  ))
_sym_db.RegisterMessage(PowerCellularMetric)


# @@protoc_insertion_point(module_scope)
