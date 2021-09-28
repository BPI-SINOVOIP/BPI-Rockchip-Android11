/*
 * Copyright (C) 2019 The Android Open Source Project
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
package android.automotive.computepipe.runner;

import android.automotive.computepipe.runner.PipeDescriptor;
import android.automotive.computepipe.runner.IPipeStateCallback;
import android.automotive.computepipe.runner.IPipeStream;
import android.automotive.computepipe.runner.IPipeDebugger;

@VintfStability
interface IPipeRunner {
    /**
     * Init the runner
     *
     * @param statecb state handler to notify client of different state
     * transitions in the runner. The runner deletes this callback after release
     * is invoked. This is the first call that should be invoked by the client
     */
    void init(in IPipeStateCallback statecb);

    /**
     * Returns the descriptor for the associated mediapipe
     *
     * @param out A descriptor that describes the input options, offload options
     * and the outputstreams of a computepipe instance.
     */
    PipeDescriptor getPipeDescriptor();

    /**
     * Set the input source for the computepipe graph.
     * This should be done prior to invoking startPipe.
     *
     * @param configId id selected from the available input options.
     * @param out if selection of input source was supported returns OK
     */
    void setPipeInputSource(in int configId);

    /**
     * Set the offload options for a graph.
     * This should be a subset of the supported offload options present in the
     * descriptor. This should be done prior to invoking startPipe
     *
     * @param configID offload option id from the advertised offload options.
     * @param out if offload option was set then returns OK.
     */
    void setPipeOffloadOptions(in int configId);

    /**
     * Set the termination options for a graph.
     * This should be a subset of the supported termination options present in the
     * descriptor. This should be done prior to invoking startPipe.
     *
     * @param terminationId id of the supported termination option as advertized
     * in the pipe descriptor
     * @param out if termination criteria was supported then returns OK.
     */
    void setPipeTermination(in int configId);

    /**
     * Enable a output stream and install call back for packets from that
     * stream. This should be invoked prior to calling startPipe.
     * Call this for each output stream that a client wants to enable
     *
     * @param configId: describes the output stream configuration the client
     * wants to enable
     * @param maxInFlightCount: The maximum number of inflight packets the
     * client can handle.
     * @param handler: the handler for the output packets to be invoked once
     * packet is received
     * @param out OK void if setting callback succeeded
     */
    void setPipeOutputConfig(in int configId, in int maxInFlightCount, in IPipeStream handler);

    /**
     * Apply all configs.
     * The client has finsihed specifying all the config options.
     * Now the configs should be applied. Once the configs are applied the
     * client will get a notification saying PipeState::CONFIG_DONE.
     * The configuration applied with this step, will be retained for all future runs
     * unless explicitly reset by calling resetPipeConfigs().
     * In case of client death as well, the applied configurations are reset.
     * In case the runner reports a ERR_HALT state, at any time after applyPipeConfigs(),
     * all configurations are retained, and expected to be reset by the client
     * explicitly, prior to attempting a new run.
     * This call is only allowed when pipe is not running.
     *
     * @param out void::OK if the runner was notified to apply config.
     */
    void applyPipeConfigs();

    /**
     * Reset all configs.
     * The runner stores the configuration even after pipe execution has
     * completed. This call erases the previous configuration, so that client
     * can modify and set the configuration again. This call is only allowed
     * when pipe is not running.
     *
     * @param out void::OK if the runner was notified to apply config.
     */
    void resetPipeConfigs();

    /**
     * Start pipe execution on the runner. Prior to this step
     * each of the configuration steps should be completed. Once the
     * configurations have been applied, the state handler will be invoked with
     * the PipeState::CONFIG_DONE notification. Wait for this notification before starting the pipe.
     * Once the Pipe starts execution the client will receive the state
     * notification PipeState::RUNNING through the state handler.
     *
     * @param out OK void if start succeeded.
     */
    void startPipe();

    /**
     * Stop pipe execution on the runner.
     *
     * This can invoked only when the pipe is in run state ie PipeState::RUNNING.
     * If a client has already chosen a termination option, then this
     * call overrides that termination criteria.
     *
     * Client will be notified once the pipe has stopped using PipeState::DONE
     * notification. Until then, outstanding packets may continue to be received.
     * These packets must still be returned with doneWithPacket(). (This does not
     * apply to SEMANTIC_DATA, as they are copied in the stream callback).
     *
     * Once the Pipe stops execution (no new packets generated),
     * the client will receive the state
     * notification, PipeState::DONE.
     *
     * Once the pipe has completely quiesced, it will transition back to
     * PipeState::CONFIG_DONE and at this point a new startPipe() can be issued or
     * previously applied configs can be reset using the resetPipeConfigs() call.
     *
     * @param out OK void if stop succeeded
     */
    void stopPipe();

    /**
     * Signal completion of a packet having been consumed by the client.
     * With this signal from client the runner should release buffer corresponding to the packet.
     *
     * @param bufferId Buffer id of the packet
     * @param streamId Stream id of the packet
     * @param out OK void if successful
     */
    void doneWithPacket(in int bufferId, in int streamId);

    /**
     * Returns the debugger associated with the runner for this graph
     *
     * @param out Debugger handle to interact with specific graph
     */
    IPipeDebugger getPipeDebugger();

    /**
     * Immediately frees up all config resources associated with the client.
     * Client will not receive state notifications after this call is complete.
     *
     * This will also free up any in flight packet.
     * The client may still get in flight IPipeStream::deliverPacket() callbacks.
     * However the underlying buffer has been freed up from the packets.
     *
     * This also resets any configuration that a client may have performed,
     * ie pipe transitions back to PipeState::RESET state.
     * So client will have to start next session with
     * the configuration steps, ie invoke setPipe*() methods.
     *
     * If the client had chosen to enable profiling through IPipeDebugger,
     * the client should first invoke IPipeDebugger::Release() prior to
     * this method.
     *
     * @return status OK if all resources were freed up.
     */
    void releaseRunner();
}
