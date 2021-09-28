# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import threading


class MultithreadedProcessor():
    """
    An instance of this class allows to execute a given function many times.
    Function's calls are parallelized by using Python threads. The function
    must take exactly one parameter: unique task id.
    There is a simple example in the file test_multithreaded_processor.py.
    Please note that there is no Python implementation that allows for "real"
    multithreading. Employing this class makes sense only if the given function
    stalls on some I/O operations and/or significant amount of execution time
    is beyond Python interpreter.

    """

    def __init__(self, number_of_threads):
        """
        @param number_of_threads: number of threads used for parallelization.
                This value must be larger than 0 and is usually NOT directly
                related to the number of available cores.

        """
        assert number_of_threads > 0
        self._number_of_threads = number_of_threads
        self._critical_section = threading.Lock()


    def run(self, function_task, number_of_tasks):
        """
        This method calls a given function many times. Each call may be
        executed in separate thread (depending on the number of threads
        set in __init__(...)). This is blocking method; it exits only if
        all threads finish. It also stops on a first error (exception raised
        in any task). In this case the function cancels all unstarted tasks,
        waits for all threads to finish (started tasks cannot be stopped) and
        raises an exception.

        @param function_task: a function to execute, it must take exactly
                one parameter: a task_id. It is an unique number from
                the range [0,number_of_tasks). Each call of the function
                corresponds to a single task. Tasks can be executed in
                parallel and in any order, but every task is executed
                exactly once.
        @param number_of_tasks: a number of tasks to execute

        @return: An list of outputs from all tasks, an index of every element
                coresponds to task id.

        @throws Exception if at least one of the tasks threw any Exception.

        """
        self._tasks_ids = range(number_of_tasks) # list of tasks ids to process
        self._outputs = [None]*number_of_tasks
        self._error = None

        # creating and starting threads
        threads = []
        while len(threads) < self._number_of_threads:
            thread = threading.Thread(target=self._thread, args=[function_task])
            threads.append(thread)
            thread.start()

        # waiting for all threads to finish
        for thread in threads:
            thread.join()

        # the self._error is set <=> at least one of the tasks failed
        if self._error is not None:
            message = 'One of threads failed with the following error: '
            message += self._error
            raise Exception(message)

        # no errors - the list of outputs is returned
        return self._outputs


    def _thread(self, function_task):
        """
        An internal method representing single thread. It processes available
        tasks. It exits when there is no more tasks to process or when a task
        threw an exception. This method is not supposed to throw any
        exceptions.

        @param function_task: a task function to execute, it must take exactly
                one parameter: a task_id. These identifiers are taken from
                the list self._tasks_ids.

        """
        try:

            while True:
                # critical section is used here to synchronize access to
                # shared variables
                with self._critical_section:
                    # exit if there is no more tasks to process
                    if len(self._tasks_ids) == 0:
                        return
                    # otherwise take task id to process
                    task_id = self._tasks_ids.pop()
                # run task with assigned task id
                self._outputs[task_id] = function_task(task_id)

        except BaseException as exception:
            # probably the task being processed raised an exception
            # critical section is used to synchronized access to shared fields
            with self._critical_section:
                # if this is the first error ...
                if self._error is None:
                    # ... we cancel all unassigned tasks ...
                    self._tasks_ids = []
                    # ... and saved the error as string
                    self._error = str(exception)
                # exit on the first spotted error
                return
