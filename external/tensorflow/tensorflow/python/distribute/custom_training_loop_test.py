# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Tests for custom training loops."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os

from absl.testing import parameterized
import numpy as np

from tensorflow.python import keras
from tensorflow.python import tf2
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.distribute import combinations
from tensorflow.python.distribute import reduce_util
from tensorflow.python.distribute import strategy_combinations
from tensorflow.python.eager import backprop
from tensorflow.python.eager import def_function
from tensorflow.python.eager import test
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import variables
from tensorflow.python.util import nest


def get_dataset_from_tensor_slices(inp_array):
  dataset = dataset_ops.DatasetV2.from_tensor_slices(inp_array)
  # TODO(b/138326910): Remove Dataset V1 version once bug resolved.
  if not tf2.enabled():
    dataset = dataset_ops.Dataset.from_tensor_slices(inp_array)
  return dataset


class AssertFlattenedMixin(object):
  """Mixin for specialized asserts."""

  def assert_equal_flattened(self, expected_results, actual_results):
    """Asserts that flattened results are equal.

    Due to the number of replicas in the strategy, the output may have a
    different structure and needs to be flattened for comparison.

    Args:
      expected_results: The results expected as a result of a computation.
      actual_results: The actual results of a computation.
    """
    self.assertEqual(len(expected_results), len(actual_results))

    for i, expected_result in enumerate(expected_results):
      final_result = []
      actual_result = actual_results[i]
      for val in actual_result:
        final_result.extend(val.numpy())
      self.assertAllEqual(expected_result, final_result)


class InputIterationTest(test.TestCase, parameterized.TestCase,
                         AssertFlattenedMixin):

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testConstantNumpyInput(self, distribution):

    @def_function.function
    def run(x):

      def computation(x):
        return math_ops.square(x)

      outputs = distribution.experimental_local_results(
          distribution.experimental_run_v2(computation, args=(x,)))
      return outputs

    self.assertAllEqual(
        constant_op.constant(4., shape=(distribution.num_replicas_in_sync)),
        run(2.))

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testStatefulExperimentalRunAlwaysExecute(self, distribution):
    with distribution.scope():
      v = variables.Variable(
          0.0, aggregation=variables.VariableAggregation.MEAN)

    @def_function.function
    def train_step():

      def assign_add():
        v.assign_add(1.0)

      distribution.experimental_run_v2(assign_add)
      return array_ops.zeros([])

    train_step()
    self.assertAllEqual(1.0, v.numpy())

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.strategies_minus_tpu,
          mode=["eager"]))
  def testFullEager(self, distribution):
    dataset = get_dataset_from_tensor_slices([5., 6., 7., 8.]).batch(2)

    def train_step(data):
      return math_ops.square(data)

    dist_dataset = distribution.experimental_distribute_dataset(dataset)
    results = []
    for x in dist_dataset:
      output = distribution.experimental_local_results(
          distribution.experimental_run_v2(train_step, args=(x,)))
      results.append(output)
    self.assert_equal_flattened([[25., 36.], [49., 64.]], results)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testStepInFunction(self, distribution):
    dataset = get_dataset_from_tensor_slices([5., 6., 7., 8.]).batch(2)

    @def_function.function
    def train_step(data):
      return math_ops.square(data)

    dist_dataset = distribution.experimental_distribute_dataset(dataset)
    results = []
    for x in dist_dataset:
      output = distribution.experimental_local_results(
          distribution.experimental_run_v2(train_step, args=(x,)))
      results.append(output)
    self.assert_equal_flattened([[25., 36.], [49., 64.]], results)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testRunInFunction(self, distribution):
    dataset = get_dataset_from_tensor_slices([5., 6., 7., 8.]).batch(2)

    def train_step(data):
      return math_ops.square(data)

    @def_function.function
    def f_train_step(input_data):
      return distribution.experimental_local_results(
          distribution.experimental_run_v2(train_step, args=(input_data,)))

    dist_dataset = distribution.experimental_distribute_dataset(dataset)
    results = []
    for x in dist_dataset:
      output = f_train_step(x)
      results.append(output)
    self.assert_equal_flattened([[25., 36.], [49., 64.]], results)

  @combinations.generate(
      combinations.combine(
          distribution=[
              strategy_combinations.mirrored_strategy_with_gpu_and_cpu,
              strategy_combinations.tpu_strategy
          ],
          mode=["eager"]))
  def testNestedOutput(self, distribution):
    dataset = get_dataset_from_tensor_slices([0, 1, 2, 3]).batch(2)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    @def_function.function
    def run(iterator):

      def computation(x):
        return [{
            "a": x - 1,
            "b": x + 1
        }]

      inputs = next(iterator)
      outputs = distribution.experimental_run_v2(computation, args=(inputs,))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    results = run(input_iterator)
    for replica in range(distribution.num_replicas_in_sync):
      # The input dataset is range(4), so the replica id is same as input.
      self.assertAllEqual(results[0]["a"][replica], [replica - 1])
      self.assertAllEqual(results[0]["b"][replica], [replica + 1])

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testRunInFunctionAutoGraphApplication(self, distribution):
    dataset = get_dataset_from_tensor_slices([5., 6., 7., 8.]).batch(2)

    def train_step(data):
      return math_ops.square(data)

    @def_function.function
    def f_train_step(input_data):
      return distribution.experimental_local_results(
          distribution.experimental_run_v2(train_step, args=(input_data,)))

    dist_dataset = distribution.experimental_distribute_dataset(dataset)
    results = []
    for x in dist_dataset:
      output = f_train_step(x)
      results.append(output)
    self.assert_equal_flattened([[25., 36.], [49., 64.]], results)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testDatasetIterationInFunction(self, distribution):
    with distribution.scope():
      a = variables.Variable(
          1.0, aggregation=variables.VariableAggregation.ONLY_FIRST_REPLICA)

    def train_step(_):
      a.assign_add(1.0)

    @def_function.function
    def f_train_step(dist_dataset):
      number_of_steps = constant_op.constant(0.0)
      product_of_means = constant_op.constant(2.0)
      for x in dist_dataset:  # loop with values modified each iteration
        number_of_steps += 1
        product_of_means *= math_ops.cast(
            distribution.reduce("MEAN", x, axis=0), product_of_means.dtype)

      for y in dist_dataset:  # loop with no intermediate state
        distribution.experimental_run_v2(train_step, args=(y,))

      return number_of_steps, product_of_means

    dataset = get_dataset_from_tensor_slices([5., 6., 7., 8.]).batch(2)
    dist_dataset = distribution.experimental_distribute_dataset(dataset)

    number_of_steps, product_of_means = f_train_step(dist_dataset)
    self.assertEqual(2, number_of_steps.numpy())
    self.assertNear((2 * (5+6)/2 * (7+8)/2), product_of_means.numpy(), 1e-3)

    # We set the initial value of `a` to 1 and iterate through the dataset 2
    # times(4/2 where 4 is the number of dataset elements and 2 is the batch
    # size). Hence the final result is 3.
    self.assertEqual(3.0, (a.numpy()))

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testDatasetAssertWithDynamicBatch(self, distribution):
    # Regression test for github issue 33517.
    def step_fn(data):
      assert_op = control_flow_ops.Assert(math_ops.less_equal(
          math_ops.reduce_max(data), 100.), [data])
      with ops.control_dependencies([assert_op]):
        return math_ops.square(data)

    @def_function.function
    def train(dataset):
      results = []
      iterator = iter(dataset)
      # we iterate through the loop 5 times since we have 3 elements and a
      # global batch of 2.
      for _ in range(2):
        elem = next(iterator)
        output = distribution.experimental_local_results(
            distribution.experimental_run_v2(step_fn, args=(elem,)))
        results.append(output)
      return results

    dataset = dataset_ops.DatasetV2.from_tensor_slices([5., 6., 7.,]).batch(2)
    # TODO(b/138326910): Remove Dataset V1 version once bug resolved.
    if not tf2.enabled():
      dataset = dataset_ops.Dataset.from_tensor_slices([5., 6., 7.,]).batch(2)
    dist_dataset = distribution.experimental_distribute_dataset(dataset)
    results = train(dist_dataset)

    expected_results = [[25., 36.], [49.]]
    self.assertEqual(len(expected_results), len(results))

    # Need to expand results since output will be grouped differently depending
    # on the number of replicas.
    for i, expected_result in enumerate(expected_results):
      final_result = []
      actual_result = results[i]
      for val in actual_result:
        final_result.extend(val.numpy())
      self.assertAllEqual(expected_result, final_result)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.multidevice_strategies,
          mode=["eager"]
      ))
  def testDynamicShapes(self, distribution):
    dataset = get_dataset_from_tensor_slices([5., 6., 7.]).batch(4)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    @def_function.function
    def run(iterator):
      def computation(x):
        return math_ops.reduce_mean(x)
      inputs = next(iterator)
      outputs = distribution.experimental_local_results(
          distribution.experimental_run_v2(computation, args=(inputs,)))
      return outputs

    # This assumes that there are exactly 2 replicas
    self.assertAllEqual([5.5, 7.], run(input_iterator))

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.multidevice_strategies,
          mode=["eager"]
      ))
  def testDynamicShapesWithGetNextOutsideFunction(self, distribution):
    dataset = get_dataset_from_tensor_slices([5., 6., 7.]).batch(4)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    @def_function.function
    def run(inputs):
      def computation(x):
        return math_ops.reduce_mean(x)
      outputs = distribution.experimental_local_results(
          distribution.experimental_run_v2(computation, args=(inputs,)))
      return outputs

    # This assumes that there are exactly 2 replicas
    self.assertAllEqual([5.5, 7.], run(next(input_iterator)))

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.multidevice_strategies,
          mode=["eager"]
      ))
  def testStrategyReduceWithDynamicShapes(self, distribution):
    dataset = get_dataset_from_tensor_slices([5., 6., 7.]).batch(4)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    @def_function.function
    def run(iterator):
      inputs = next(iterator)
      return distribution.reduce(reduce_util.ReduceOp.MEAN, inputs, axis=0)

    self.assertAllEqual(6., run(input_iterator))

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.multidevice_strategies,
          mode=["eager"]
      ))
  def testStrategyReduceWithDynamicShapesRank2(self, distribution):
    dataset = get_dataset_from_tensor_slices(
        [[1., 1.], [1., 1.], [1., 1.]]).batch(4)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    @def_function.function
    def run(iterator):
      inputs = next(iterator)
      return distribution.reduce(reduce_util.ReduceOp.MEAN, inputs, axis=0)

    self.assertAllEqual([1., 1.], run(input_iterator))

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.multidevice_strategies,
          mode=["eager"]
      ))
  def testDynamicShapesWithSizeOp(self, distribution):
    dataset = get_dataset_from_tensor_slices([5., 6., 7.]).batch(4)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    @def_function.function
    def run(inputs):
      def computation(x):
        return array_ops.size_v2(x)
      outputs = distribution.experimental_local_results(
          distribution.experimental_run_v2(computation, args=(inputs,)))
      return outputs

    # This assumes that there are exactly 2 replicas
    self.assertAllEqual([2, 1], run(next(input_iterator)))

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.multidevice_strategies,
          mode=["eager"]
      ))
  def testDynamicShapesWithFirstReplicaNotMaximumShape(self, distribution):
    def dataset_fn(_):
      dataset1 = get_dataset_from_tensor_slices([[1., 2.], [1., 2.]])
      dataset2 = get_dataset_from_tensor_slices([[1., 2., 3.],
                                                 [1., 2., 3.]])
      dataset = dataset1.concatenate(dataset2)
      dataset = dataset.batch(2, drop_remainder=True)
      return dataset

    input_iterator = iter(
        distribution.experimental_distribute_datasets_from_function(dataset_fn))

    @def_function.function
    def run(inputs):
      def computation(x):
        return math_ops.reduce_mean(x)
      outputs = distribution.experimental_local_results(
          distribution.experimental_run_v2(computation, args=(inputs,)))
      return outputs

    # This assumes that there are exactly 2 replicas
    self.assertAllEqual([1.5, 2.], run(next(input_iterator)))

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testDatasetDistributeEvenlyDivisibleDrop(self, distribution):
    # If the batch size is evenly divisible by the number of workers and we set
    # drop_remainder=True on the dataset, then DistributedIterator will use a
    # different (and more efficient) code path which avoids some control flow
    # ops.
    dataset = get_dataset_from_tensor_slices([5., 6.]).batch(
        2, drop_remainder=True)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    data = next(input_iterator)

    expected_result = [5., 6.]
    final_result = []
    actual_result = distribution.experimental_local_results(data)
    for val in actual_result:
      final_result.extend(val)
    self.assertAllEqual(expected_result, final_result)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testDatasetDistributeNotDivisibleDrop(self, distribution):
    # If each batch is not evenly divisible by the number of workers,
    # the remainder will be dropped.
    dataset = get_dataset_from_tensor_slices([5., 6.]).batch(
        1, drop_remainder=True)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    data = next(input_iterator)

    expected_result = [5.]
    final_result = []
    actual_result = distribution.experimental_local_results(data)
    for val in actual_result:
      final_result.extend(val)
    self.assertAllEqual(expected_result, final_result)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testDatasetDistributeEvenlyDivisibleNoDrop(self, distribution):
    # Setting drop_remainder=False on the dataset causes DistributedIterator
    # to use get_next_as_optional(), even if the batched dataset is evenly
    # divisible by the number of workers.
    dataset = get_dataset_from_tensor_slices([5., 6.]).batch(
        2, drop_remainder=False)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    data = next(input_iterator)

    expected_result = [5., 6.]
    final_result = []
    actual_result = distribution.experimental_local_results(data)
    for val in actual_result:
      final_result.extend(val)
    self.assertAllEqual(expected_result, final_result)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testDatasetPartialBatchWithMixedOutputs(self, distribution):
    # Dynamic output size with a mix of static and dynamic outputs
    dataset = get_dataset_from_tensor_slices([5.]).batch(2)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    @def_function.function
    def run(iterator):

      def computation(x):
        # Fixed size output with a dynamic sized output.
        return array_ops.zeros([3]), math_ops.square(x)

      return distribution.experimental_run_v2(
          computation, args=(next(iterator),))

    results = run(input_iterator)

    # First result is fixed for all replicas.
    for replica_id in range(distribution.num_replicas_in_sync):
      self.assertAllEqual([0., 0., 0.],
                          distribution.experimental_local_results(
                              results[0])[replica_id])
    # Only first replica has distributed dataset computation.
    self.assertAllEqual([25.],
                        distribution.experimental_local_results(results[1])[0])
    # Other replicas have no distributed dataset computation.
    for replica_id in range(1, distribution.num_replicas_in_sync):
      self.assertAllEqual([],
                          distribution.experimental_local_results(
                              results[1])[replica_id])

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testIterationInsideFunction(self, distribution):

    def step_fn(data):
      return math_ops.square(data)

    @def_function.function
    def train(dataset):
      results = []
      iterator = iter(dataset)
      # we iterate through the loop 2 times since we have 4 elements and a
      # global batch of 2.
      for _ in range(2):
        elem = next(iterator)
        output = distribution.experimental_local_results(
            distribution.experimental_run_v2(step_fn, args=(elem,)))
        results.append(output)
      return results

    dataset = get_dataset_from_tensor_slices([5., 6., 7., 8.]).batch(2)
    dist_dataset = distribution.experimental_distribute_dataset(dataset)
    results = train(dist_dataset)
    self.assert_equal_flattened([[25., 36.], [49., 64.]], results)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testIterationOutsideFunction(self, distribution):

    def train_step(data):
      return math_ops.square(data)

    @def_function.function
    def f_train_step(input_data):
      return distribution.experimental_local_results(
          distribution.experimental_run_v2(train_step, args=(input_data,)))

    dataset = get_dataset_from_tensor_slices([5., 6., 7., 8.]).batch(2)
    dist_dataset = distribution.experimental_distribute_dataset(dataset)
    iterator = iter(dist_dataset)
    results = []
    # we iterate through the loop 2 times since we have 4 elements and a
    # global batch of 2.
    for _ in range(2):
      output = f_train_step(next(iterator))
      results.append(output)
    self.assert_equal_flattened([[25., 36.], [49., 64.]], results)


class GradientTapeTest(test.TestCase, parameterized.TestCase,
                       AssertFlattenedMixin):

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testStepInFunctionGradient(self, distribution):
    dataset = get_dataset_from_tensor_slices([5., 6., 7., 8.]).batch(2)

    @def_function.function
    def train_step(x):
      def computation(x):
        return math_ops.square(x)
      with backprop.GradientTape() as tape:
        tape.watch(x)  # Manually watch non-variable tensors.
        y = computation(x)
      grads = tape.gradient(y, x)
      return grads

    dist_dataset = distribution.experimental_distribute_dataset(dataset)
    results = []
    for x in dist_dataset:
      output = distribution.experimental_local_results(
          distribution.experimental_run_v2(train_step, args=(x,)))
      results.append(output)
    self.assert_equal_flattened([[10., 12.], [14., 16.]], results)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def testRunInFunctionGradient(self, distribution):
    dataset = get_dataset_from_tensor_slices([5., 6., 7., 8.]).batch(2)

    @def_function.function
    def run(x):
      def train_step(x):
        def computation(x):
          return math_ops.square(x)
        with backprop.GradientTape() as tape:
          tape.watch(x)  # Manually watch non-variable tensors.
          y = computation(x)
        grads = tape.gradient(y, x)
        return grads
      return distribution.experimental_local_results(
          distribution.experimental_run_v2(train_step, args=(x,)))

    dist_dataset = distribution.experimental_distribute_dataset(dataset)
    results = []
    for x in dist_dataset:
      output = run(x)
      results.append(output)
    self.assert_equal_flattened([[10., 12.], [14., 16.]], results)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"],
          model_in_tf_function=[True, False]
      ))
  def testNestedFunction(self, distribution, model_in_tf_function):
    def model(x):
      return x * x

    if model_in_tf_function:
      model = def_function.function(model)

    with distribution.scope():
      x = variables.Variable(1.0)

      @def_function.function
      def train_step():
        def replica_step():
          with backprop.GradientTape() as tape:
            y = model(x)
          return tape.gradient(y, x)
        return distribution.experimental_run_v2(replica_step)

      grads = distribution.experimental_local_results(train_step())
      self.assertLen(grads, distribution.num_replicas_in_sync)
      self.assertTrue(all(g is not None for g in grads))


class KerasModelsTest(test.TestCase, parameterized.TestCase):

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_single_keras_layer_experimental_run(self, distribution):
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = keras.layers.Dense(4, name="dense")

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        return grads

      outputs = distribution.experimental_run_v2(
          step_fn, args=(next(iterator),))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_keras_model_creation_experimental_run(self, distribution):
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = self._get_model()

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        return grads

      outputs = distribution.experimental_run_v2(
          step_fn, args=(next(iterator),))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_keras_model_optimizer_experimental_run(self, distribution):
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = self._get_model()
      optimizer = keras.optimizer_v2.rmsprop.RMSprop()

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))
        return loss

      outputs = distribution.experimental_run_v2(
          step_fn, args=(next(iterator),))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_keras_subclass_model_optimizer_experimental_run(self, distribution):
    def get_subclass_model():

      class KerasSubclassModel(keras.Model):

        def __init__(self):
          super(KerasSubclassModel, self).__init__()
          self.l = keras.layers.Dense(4, name="dense")

        def call(self, x):
          return self.l(x)

      return KerasSubclassModel()
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = get_subclass_model()
      optimizer = keras.optimizer_v2.rmsprop.RMSprop()

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))
        return loss

      outputs = distribution.experimental_run_v2(
          step_fn, args=(next(iterator),))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_keras_model_optimizer_experimental_run_loop(self, distribution):
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = self._get_model()
      optimizer = keras.optimizer_v2.rmsprop.RMSprop()

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))
        return loss

      for _ in range(5):
        distribution.experimental_run_v2(step_fn, args=(next(iterator),))

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_lstm(self, distribution):

    batch_size = 32

    def create_lstm_model():
      model = keras.models.Sequential()
      # We only have LSTM variables so we can detect no gradient issues more
      # easily.
      model.add(
          keras.layers.LSTM(1, return_sequences=False, input_shape=(10, 1)))
      return model

    def create_lstm_data():
      seq_length = 10

      x_train = np.random.rand(batch_size, seq_length, 1).astype("float32")
      y_train = np.random.rand(batch_size, 1).astype("float32")
      return x_train, y_train

    x, y = create_lstm_data()
    dataset = dataset_ops.Dataset.from_tensor_slices((x, y))
    dataset = dataset.batch(batch_size, drop_remainder=True)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = create_lstm_model()
      optimizer = keras.optimizer_v2.gradient_descent.SGD()

    @def_function.function
    def train_step(input_iterator):

      def step_fn(inputs):
        inps, targ = inputs
        with backprop.GradientTape() as tape:
          output = model(inps)
          loss = math_ops.reduce_mean(
              keras.losses.binary_crossentropy(
                  y_true=targ, y_pred=output, from_logits=False))
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))
        return loss

      outputs = distribution.experimental_run_v2(
          step_fn, args=(next(input_iterator),))
      return distribution.experimental_local_results(outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies, mode=["eager"]))
  def test_nested_tf_functions(self, distribution):
    # The test builds two computations with keras layers, one with nested
    # tf.function, and the other without nested tf.function. We run these
    # computations independently on the model with same weights, and make sure
    # the variables are still the same after one training step.

    inputs = np.random.random((10, 3)).astype(np.float32)
    targets = np.ones((10, 4), dtype=np.float32)
    dataset = dataset_ops.Dataset.from_tensor_slices((inputs, targets)).repeat()
    dataset = dataset.batch(10, drop_remainder=True)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    def get_model():
      x = keras.layers.Input(shape=(3,), name="input")
      y = keras.layers.Dense(4, name="dense")(x)
      model = keras.Model(x, y)
      return model

    with distribution.scope():
      model = get_model()
      optimizer = keras.optimizer_v2.gradient_descent.SGD(0.1, momentum=0.01)
      weights_file = os.path.join(self.get_temp_dir(), ".h5")
      model.save_weights(weights_file)
      model2 = get_model()
      model2.load_weights(weights_file)

    # Make sure model and model2 variables are in sync when initialized.
    for model_v, model2_v in zip(model.variables, model2.variables):
      self.assertAllClose(model_v.numpy(), model2_v.numpy())

    def compute_loss(images, targets):
      outputs = model(images)
      return math_ops.reduce_sum(outputs - targets)

    @def_function.function
    def train_step_without_nested_tf_function(inputs):

      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          loss = compute_loss(images, targets)
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))

      distribution.experimental_run_v2(step_fn, args=(inputs,))

    @def_function.function
    def compute_loss2(images, targets):
      outputs = model2(images)
      return math_ops.reduce_sum(outputs - targets)

    @def_function.function
    def train_step_with_nested_tf_function(inputs):

      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          loss = compute_loss2(images, targets)
        grads = tape.gradient(loss, model2.variables)
        optimizer.apply_gradients(zip(grads, model2.variables))

      distribution.experimental_run_v2(step_fn, args=(inputs,))

    inputs = next(input_iterator)

    train_step_without_nested_tf_function(inputs)
    train_step_with_nested_tf_function(inputs)

    # Make sure model and model2 variables are still in sync.
    for model_v, model2_v in zip(model.variables, model2.variables):
      self.assertAllClose(model_v.numpy(), model2_v.numpy())

  def _get_dataset(self):
    inputs = np.zeros((10, 3), dtype=np.float32)
    targets = np.zeros((10, 4), dtype=np.float32)
    dataset = dataset_ops.Dataset.from_tensor_slices((inputs, targets))
    dataset = dataset.repeat(100)
    dataset = dataset.batch(10, drop_remainder=True)
    return dataset

  def _get_model(self):
    x = keras.layers.Input(shape=(3,), name="input")
    y = keras.layers.Dense(4, name="dense")(x)
    model = keras.Model(x, y)
    return model


if __name__ == "__main__":
  test.main()
