/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.internal.app;

import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import android.os.RemoteCallback;

import com.android.internal.app.IVoiceActionCheckCallback;
import com.android.internal.app.IVoiceInteractionSessionShowCallback;
import com.android.internal.app.IVoiceInteractor;
import com.android.internal.app.IVoiceInteractionSessionListener;
import android.hardware.soundtrigger.IRecognitionStatusCallback;
import android.hardware.soundtrigger.KeyphraseMetadata;
import android.hardware.soundtrigger.ModelParams;
import android.hardware.soundtrigger.SoundTrigger;
import android.service.voice.IVoiceInteractionService;
import android.service.voice.IVoiceInteractionSession;

interface IVoiceInteractionManagerService {
    void showSession(in Bundle sessionArgs, int flags);
    boolean deliverNewSession(IBinder token, IVoiceInteractionSession session,
            IVoiceInteractor interactor);
    boolean showSessionFromSession(IBinder token, in Bundle sessionArgs, int flags);
    boolean hideSessionFromSession(IBinder token);
    int startVoiceActivity(IBinder token, in Intent intent, String resolvedType,
            String callingFeatureId);
    int startAssistantActivity(IBinder token, in Intent intent, String resolvedType,
            String callingFeatureId);
    void setKeepAwake(IBinder token, boolean keepAwake);
    void closeSystemDialogs(IBinder token);
    void finish(IBinder token);
    void setDisabledShowContext(int flags);
    int getDisabledShowContext();
    int getUserDisabledShowContext();

    /**
     * Gets the registered Sound model for keyphrase detection for the current user.
     * May be null if no matching sound model exists.
     * Caller must either be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}, or the caller must be a voice model
     * enrollment application detected by
     * {@link android.hardware.soundtrigger.KeyphraseEnrollmentInfo}.
     *
     * @param keyphraseId The unique identifier for the keyphrase.
     * @param bcp47Locale The BCP47 language tag  for the keyphrase's locale.
     * @RequiresPermission Manifest.permission.MANAGE_VOICE_KEYPHRASES
     */
    @UnsupportedAppUsage
    SoundTrigger.KeyphraseSoundModel getKeyphraseSoundModel(int keyphraseId, in String bcp47Locale);
    /**
     * Add/Update the given keyphrase sound model for the current user.
     * Caller must either be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}, or the caller must be a voice model
     * enrollment application detected by
     * {@link android.hardware.soundtrigger.KeyphraseEnrollmentInfo}.
     *
     * @param model The keyphrase sound model to store peristantly.
     * @RequiresPermission Manifest.permission.MANAGE_VOICE_KEYPHRASES
     */
    int updateKeyphraseSoundModel(in SoundTrigger.KeyphraseSoundModel model);
    /**
     * Deletes the given keyphrase sound model for the current user.
     * Caller must either be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}, or the caller must be a voice model
     * enrollment application detected by
     * {@link android.hardware.soundtrigger.KeyphraseEnrollmentInfo}.
     *
     * @param keyphraseId The unique identifier for the keyphrase.
     * @param bcp47Locale The BCP47 language tag  for the keyphrase's locale.
     * @RequiresPermission Manifest.permission.MANAGE_VOICE_KEYPHRASES
     */
    int deleteKeyphraseSoundModel(int keyphraseId, in String bcp47Locale);

    /**
     * Gets the properties of the DSP hardware on this device, null if not present.
     * Caller must be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}.
     */
    SoundTrigger.ModuleProperties getDspModuleProperties();
    /**
     * Indicates if there's a keyphrase sound model available for the given keyphrase ID and the
     * user ID of the caller.
     * Caller must be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}.
     *
     * @param keyphraseId The unique identifier for the keyphrase.
     * @param bcp47Locale The BCP47 language tag  for the keyphrase's locale.
     */
    boolean isEnrolledForKeyphrase(int keyphraseId, String bcp47Locale);
    /**
     * Generates KeyphraseMetadata for an enrolled sound model based on keyphrase string, locale,
     * and the user ID of the caller.
     * Caller must be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}.
     *
     * @param keyphrase Keyphrase text associated with the enrolled model
     * @param bcp47Locale The BCP47 language tag for the keyphrase's locale.
     * @return The metadata for the enrolled voice model bassed on the passed in parameters. Null if
     *         no matching voice model exists.
     */
    KeyphraseMetadata getEnrolledKeyphraseMetadata(String keyphrase, String bcp47Locale);
    /**
     * Starts a recognition for the given keyphrase.
     * Caller must be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}.
     */
    int startRecognition(int keyphraseId, in String bcp47Locale,
            in IRecognitionStatusCallback callback,
            in SoundTrigger.RecognitionConfig recognitionConfig);
    /**
     * Stops a recognition for the given keyphrase.
     * Caller must be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}.
     */
    int stopRecognition(int keyphraseId, in IRecognitionStatusCallback callback);
    /**
     * Set a model specific ModelParams with the given value. This
     * parameter will keep its value for the duration the model is loaded regardless of starting and
     * stopping recognition. Once the model is unloaded, the value will be lost.
     * queryParameter should be checked first before calling this method.
     * Caller must be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}.
     *
     * @param keyphraseId The unique identifier for the keyphrase.
     * @param modelParam   ModelParams
     * @param value        Value to set
     * @return - {@link SoundTrigger#STATUS_OK} in case of success
     *         - {@link SoundTrigger#STATUS_NO_INIT} if the native service cannot be reached
     *         - {@link SoundTrigger#STATUS_BAD_VALUE} invalid input parameter
     *         - {@link SoundTrigger#STATUS_INVALID_OPERATION} if the call is out of sequence or
     *           if API is not supported by HAL
     */
    int setParameter(int keyphraseId, in ModelParams modelParam, int value);
    /**
     * Get a model specific ModelParams. This parameter will keep its value
     * for the duration the model is loaded regardless of starting and stopping recognition.
     * Once the model is unloaded, the value will be lost. If the value is not set, a default
     * value is returned. See ModelParams for parameter default values.
     * queryParameter should be checked first before calling this method.
     * Caller must be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}.
     *
     * @param keyphraseId The unique identifier for the keyphrase.
     * @param modelParam   ModelParams
     * @return value of parameter
     */
    int getParameter(int keyphraseId, in ModelParams modelParam);
    /**
     * Determine if parameter control is supported for the given model handle.
     * This method should be checked prior to calling setParameter or getParameter.
     * Caller must be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}.
     *
     * @param keyphraseId The unique identifier for the keyphrase.
     * @param modelParam ModelParams
     * @return supported range of parameter, null if not supported
     */
    @nullable SoundTrigger.ModelParamRange queryParameter(int keyphraseId,
            in ModelParams modelParam);

    /**
     * @return the component name for the currently active voice interaction service
     * @RequiresPermission Manifest.permission.ACCESS_VOICE_INTERACTION_SERVICE
     */
    ComponentName getActiveServiceComponentName();

    /**
     * Shows the session for the currently active service. Used to start a new session from system
     * affordances.
     *
     * @param args the bundle to pass as arguments to the voice interaction session
     * @param sourceFlags flags indicating the source of this show
     * @param showCallback optional callback to be notified when the session was shown
     * @param activityToken optional token of activity that needs to be on top
     * @RequiresPermission Manifest.permission.ACCESS_VOICE_INTERACTION_SERVICE
     */
    boolean showSessionForActiveService(in Bundle args, int sourceFlags,
            IVoiceInteractionSessionShowCallback showCallback, IBinder activityToken);

    /**
     * Hides the session from the active service, if it is showing.
     * @RequiresPermission Manifest.permission.ACCESS_VOICE_INTERACTION_SERVICE
     */
    void hideCurrentSession();

    /**
     * Notifies the active service that a launch was requested from the Keyguard. This will only
     * be called if {@link #activeServiceSupportsLaunchFromKeyguard()} returns true.
     * @RequiresPermission Manifest.permission.ACCESS_VOICE_INTERACTION_SERVICE
     */
    void launchVoiceAssistFromKeyguard();

    /**
     * Indicates whether there is a voice session running (but not necessarily showing).
     * @RequiresPermission Manifest.permission.ACCESS_VOICE_INTERACTION_SERVICE
     */
    boolean isSessionRunning();

    /**
     * Indicates whether the currently active voice interaction service is capable of handling the
     * assist gesture.
     * @RequiresPermission Manifest.permission.ACCESS_VOICE_INTERACTION_SERVICE
     */
    boolean activeServiceSupportsAssist();

    /**
     * Indicates whether the currently active voice interaction service is capable of being launched
     * from the lockscreen.
     * @RequiresPermission Manifest.permission.ACCESS_VOICE_INTERACTION_SERVICE
     */
    boolean activeServiceSupportsLaunchFromKeyguard();

    /**
     * Called when the lockscreen got shown.
     * @RequiresPermission Manifest.permission.ACCESS_VOICE_INTERACTION_SERVICE
     */
    void onLockscreenShown();

    /**
     * Register a voice interaction listener.
     * @RequiresPermission Manifest.permission.ACCESS_VOICE_INTERACTION_SERVICE
     */
    void registerVoiceInteractionSessionListener(IVoiceInteractionSessionListener listener);

    /**
     * Checks the availability of a set of voice actions for the current active voice service.
     * Returns all supported voice actions.
     * @RequiresPermission Manifest.permission.ACCESS_VOICE_INTERACTION_SERVICE
     */
    void getActiveServiceSupportedActions(in List<String> voiceActions,
     in IVoiceActionCheckCallback callback);

    /**
     * Provide hints for showing UI.
     * Caller must be the active voice interaction service via
     * {@link Settings.Secure.VOICE_INTERACTION_SERVICE}.
     */
    void setUiHints(in Bundle hints);

    /**
     * Requests a list of supported actions from a specific activity.
     */
    void requestDirectActions(in IBinder token, int taskId, IBinder assistToken,
             in RemoteCallback cancellationCallback, in RemoteCallback callback);

    /**
     * Requests performing an action from a specific activity.
     */
    void performDirectAction(in IBinder token, String actionId, in Bundle arguments, int taskId,
            IBinder assistToken, in RemoteCallback cancellationCallback,
            in RemoteCallback resultCallback);

    /**
     * Temporarily disables voice interaction (for example, on Automotive when the display is off).
     *
     * It will shutdown the service, and only re-enable it after it's called again (or after a
     * system restart).
     *
     * NOTE: it's only effective when the service itself is available / enabled in the device, so
     * calling setDisable(false) would be a no-op when it isn't.
     */
    void setDisabled(boolean disabled);

}
