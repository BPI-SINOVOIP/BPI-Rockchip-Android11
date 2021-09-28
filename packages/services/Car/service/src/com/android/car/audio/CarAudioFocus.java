/*
 * Copyright (C) 2015 The Android Open Source Project
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
package com.android.car.audio;

import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioFocusInfo;
import android.media.AudioManager;
import android.media.audiopolicy.AudioPolicy;
import android.util.LocalLog;
import android.util.Log;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;


public class CarAudioFocus extends AudioPolicy.AudioPolicyFocusListener {

    private static final String TAG = "CarAudioFocus";

    private static final int FOCUS_EVENT_LOGGER_QUEUE_SIZE = 25;

    private final AudioManager mAudioManager;
    private final PackageManager mPackageManager;
    private AudioPolicy mAudioPolicy; // Dynamically assigned just after construction

    private final LocalLog mFocusEventLogger;

    private final FocusInteraction mFocusInteraction;

    private final boolean mEnabledDelayedFocusRequest;
    private AudioFocusInfo mDelayedRequest;


    // We keep track of all the focus requesters in this map, with their clientId as the key.
    // This is used both for focus dispatch and death handling
    // Note that the clientId reflects the AudioManager instance and listener object (if any)
    // so that one app can have more than one unique clientId by setting up distinct listeners.
    // Because the listener gets only LOSS/GAIN messages, this is important for an app to do if
    // it expects to request focus concurrently for different USAGEs so it knows which USAGE
    // gained or lost focus at any given moment.  If the SAME listener is used for requests of
    // different USAGE while the earlier request is still in the focus stack (whether holding
    // focus or pending), the new request will be REJECTED so as to avoid any confusion about
    // the meaning of subsequent GAIN/LOSS events (which would continue to apply to the focus
    // request that was already active or pending).
    private final HashMap<String, FocusEntry> mFocusHolders = new HashMap<>();
    private final HashMap<String, FocusEntry> mFocusLosers = new HashMap<>();

    private final Object mLock = new Object();


    CarAudioFocus(AudioManager audioManager, PackageManager packageManager,
            FocusInteraction focusInteraction, boolean enableDelayedFocusRequest) {
        mAudioManager = audioManager;
        mPackageManager = packageManager;
        mFocusEventLogger = new LocalLog(FOCUS_EVENT_LOGGER_QUEUE_SIZE);
        mFocusInteraction = focusInteraction;
        mEnabledDelayedFocusRequest = enableDelayedFocusRequest;
    }


    // This has to happen after the construction to avoid a chicken and egg problem when setting up
    // the AudioPolicy which must depend on this object.
    public void setOwningPolicy(AudioPolicy parentPolicy) {
        mAudioPolicy = parentPolicy;
    }


    // This sends a focus loss message to the targeted requester.
    private void sendFocusLossLocked(AudioFocusInfo loser, int lossType) {
        int result = mAudioManager.dispatchAudioFocusChange(loser, lossType,
                mAudioPolicy);
        if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            // TODO:  Is this actually an error, or is it okay for an entry in the focus stack
            // to NOT have a listener?  If that's the case, should we even keep it in the focus
            // stack?
            Log.e(TAG, "Failure to signal loss of audio focus with error: " + result);
        }

        logFocusEvent("sendFocusLoss for client " + loser.getClientId()
                + " with loss type " + focusEventToString(lossType)
                + " resulted in " + focusRequestResponseToString(result));
    }


    /** @see AudioManager#requestAudioFocus(AudioManager.OnAudioFocusChangeListener, int, int, int) */
    // Note that we replicate most, but not all of the behaviors of the default MediaFocusControl
    // engine as of Android P.
    // Besides the interaction matrix which allows concurrent focus for multiple requestors, which
    // is the reason for this module, we also treat repeated requests from the same clientId
    // slightly differently.
    // If a focus request for the same listener (clientId) is received while that listener is
    // already in the focus stack, we REJECT it outright unless it is for the same USAGE.
    // If it is for the same USAGE, we replace the old request with the new one.
    // The default audio framework's behavior is to remove the previous entry in the stack (no-op
    // if the requester is already holding focus).
    private int evaluateFocusRequestLocked(AudioFocusInfo afi) {
        Log.i(TAG, "Evaluating " + focusEventToString(afi.getGainRequest())
                + " request for client " + afi.getClientId()
                + " with usage " + afi.getAttributes().usageToString());

        // Is this a request for premanant focus?
        // AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE -- Means Notifications should be denied
        // AUDIOFOCUS_GAIN_TRANSIENT -- Means current focus holders should get transient loss
        // AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK -- Means other can duck (no loss message from us)
        // NOTE:  We expect that in practice it will be permanent for all media requests and
        //        transient for everything else, but that isn't currently an enforced requirement.
        final boolean permanent =
                (afi.getGainRequest() == AudioManager.AUDIOFOCUS_GAIN);
        final boolean allowDucking =
                (afi.getGainRequest() == AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK);

        boolean delayFocusForCurrentRequest = false;

        final int requestedContext = CarAudioContext.getContextForUsage(
                afi.getAttributes().getSystemUsage());

        // If we happen to find entries that this new request should replace, we'll store them here.
        // This happens when a client makes a second AF request on the same listener.
        // After we've granted audio focus to our current request, we'll abandon these requests.
        FocusEntry replacedCurrentEntry = null;
        FocusEntry replacedBlockedEntry = null;

        boolean allowDelayedFocus = mEnabledDelayedFocusRequest && canReceiveDelayedFocus(afi);

        // We don't allow sharing listeners (client IDs) between two concurrent requests
        // (because the app would have no way to know to which request a later event applied)
        if (mDelayedRequest != null && afi.getClientId().equals(mDelayedRequest.getClientId())) {
            int delayedRequestedContext = CarAudioContext.getContextForUsage(
                    mDelayedRequest.getAttributes().getSystemUsage());
            // If it is for a different context then reject
            if (delayedRequestedContext != requestedContext) {
                // Trivially reject a request for a different USAGE
                Log.e(TAG, String.format(
                        "Client %s has already delayed requested focus for %s "
                                + "- cannot request focus for %s on same listener.",
                        mDelayedRequest.getClientId(),
                        mDelayedRequest.getAttributes().usageToString(),
                        afi.getAttributes().usageToString()));
                return AudioManager.AUDIOFOCUS_REQUEST_FAILED;
            }
        }

        // Scan all active and pending focus requests.  If any should cause rejection of
        // this new request, then we're done.  Keep a list of those against whom we're exclusive
        // so we can update the relationships if/when we are sure we won't get rejected.
        Log.i(TAG, "Scanning focus holders...");
        final ArrayList<FocusEntry> losers = new ArrayList<FocusEntry>();
        for (FocusEntry entry : mFocusHolders.values()) {
            Log.d(TAG, "Evaluating focus holder: " + entry.getClientId());

            // If this request is for Notifications and a current focus holder has specified
            // AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE, then reject the request.
            // This matches the hardwired behavior in the default audio policy engine which apps
            // might expect (The interaction matrix doesn't have any provision for dealing with
            // override flags like this).
            if ((requestedContext == CarAudioContext.NOTIFICATION)
                    && (entry.getAudioFocusInfo().getGainRequest()
                    == AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE)) {
                return AudioManager.AUDIOFOCUS_REQUEST_FAILED;
            }

            // We don't allow sharing listeners (client IDs) between two concurrent requests
            // (because the app would have no way to know to which request a later event applied)
            if (afi.getClientId().equals(entry.getAudioFocusInfo().getClientId())) {
                if (entry.getAudioContext() == requestedContext) {
                    // This is a request from a current focus holder.
                    // Abandon the previous request (without sending a LOSS notification to it),
                    // and don't check the interaction matrix for it.
                    Log.i(TAG, "Replacing accepted request from same client");
                    replacedCurrentEntry = entry;
                    continue;
                } else {
                    // Trivially reject a request for a different USAGE
                    Log.e(TAG, String.format(
                            "Client %s has already requested focus for %s - cannot request focus "
                                    + "for %s on same listener.",
                            entry.getClientId(),
                            entry.getAudioFocusInfo().getAttributes().usageToString(),
                            afi.getAttributes().usageToString()));
                    return AudioManager.AUDIOFOCUS_REQUEST_FAILED;
                }
            }

            @AudioManager.FocusRequestResult int interactionResult = mFocusInteraction
                    .evaluateRequest(requestedContext, entry, losers, allowDucking,
                            allowDelayedFocus);
            if (interactionResult == AudioManager.AUDIOFOCUS_REQUEST_FAILED) {
                return interactionResult;
            }
            if (interactionResult == AudioManager.AUDIOFOCUS_REQUEST_DELAYED) {
                delayFocusForCurrentRequest = true;
            }
        }
        Log.i(TAG, "Scanning those who've already lost focus...");
        final ArrayList<FocusEntry> blocked = new ArrayList<FocusEntry>();
        for (FocusEntry entry : mFocusLosers.values()) {
            Log.i(TAG, entry.getAudioFocusInfo().getClientId());

            // If this request is for Notifications and a pending focus holder has specified
            // AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE, then reject the request
            if ((requestedContext == CarAudioContext.NOTIFICATION)
                    && (entry.getAudioFocusInfo().getGainRequest()
                    == AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE)) {
                return AudioManager.AUDIOFOCUS_REQUEST_FAILED;
            }

            // We don't allow sharing listeners (client IDs) between two concurrent requests
            // (because the app would have no way to know to which request a later event applied)
            if (afi.getClientId().equals(entry.getAudioFocusInfo().getClientId())) {
                if (entry.getAudioContext() == requestedContext) {
                    // This is a repeat of a request that is currently blocked.
                    // Evaluate it as if it were a new request, but note that we should remove
                    // the old pending request, and move it.
                    // We do not want to evaluate the new request against itself.
                    Log.i(TAG, "Replacing pending request from same client");
                    replacedBlockedEntry = entry;
                    continue;
                } else {
                    // Trivially reject a request for a different USAGE
                    Log.e(TAG, String.format(
                            "Client %s has already requested focus for %s - cannot request focus "
                                    + "for %s on same listener.",
                            entry.getClientId(),
                            entry.getAudioFocusInfo().getAttributes().usageToString(),
                            afi.getAttributes().usageToString()));
                    return AudioManager.AUDIOFOCUS_REQUEST_FAILED;
                }
            }

            @AudioManager.FocusRequestResult int interactionResult = mFocusInteraction
                    .evaluateRequest(requestedContext, entry, blocked, allowDucking,
                            allowDelayedFocus);
            if (interactionResult == AudioManager.AUDIOFOCUS_REQUEST_FAILED) {
                return interactionResult;
            }
            if (interactionResult == AudioManager.AUDIOFOCUS_REQUEST_DELAYED) {
                delayFocusForCurrentRequest = true;
            }
        }


        // Now that we've decided we'll grant focus, construct our new FocusEntry
        FocusEntry newEntry = new FocusEntry(afi, requestedContext, mPackageManager);

        // These entries have permanently lost focus as a result of this request, so they
        // should be removed from all blocker lists.
        ArrayList<FocusEntry> permanentlyLost = new ArrayList<>();

        if (replacedCurrentEntry != null) {
            mFocusHolders.remove(replacedCurrentEntry.getClientId());
            permanentlyLost.add(replacedCurrentEntry);
        }
        if (replacedBlockedEntry != null) {
            mFocusLosers.remove(replacedBlockedEntry.getClientId());
            permanentlyLost.add(replacedBlockedEntry);
        }


        // Now that we're sure we'll accept this request, update any requests which we would
        // block but are already out of focus but waiting to come back
        for (FocusEntry entry : blocked) {
            // If we're out of focus it must be because somebody is blocking us
            assert !entry.isUnblocked();

            if (permanent) {
                // This entry has now lost focus forever
                sendFocusLossLocked(entry.getAudioFocusInfo(), AudioManager.AUDIOFOCUS_LOSS);
                entry.setDucked(false);
                final FocusEntry deadEntry = mFocusLosers.remove(
                        entry.getAudioFocusInfo().getClientId());
                assert deadEntry != null;
                permanentlyLost.add(entry);
            } else {
                if (!allowDucking && entry.isDucked()) {
                    // This entry was previously allowed to duck, but can no longer do so.
                    Log.i(TAG, "Converting duckable loss to non-duckable for "
                            + entry.getClientId());
                    sendFocusLossLocked(entry.getAudioFocusInfo(),
                            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);
                    entry.setDucked(false);
                }
                // Note that this new request is yet one more reason we can't (yet) have focus
                entry.addBlocker(newEntry);
            }
        }

        // Notify and update any requests which are now losing focus as a result of the new request
        for (FocusEntry entry : losers) {
            // If we have focus (but are about to loose it), nobody should be blocking us yet
            assert entry.isUnblocked();

            int lossType;
            if (permanent) {
                lossType = AudioManager.AUDIOFOCUS_LOSS;
            } else if (allowDucking && entry.receivesDuckEvents()) {
                lossType = AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK;
                entry.setDucked(true);
            } else {
                lossType = AudioManager.AUDIOFOCUS_LOSS_TRANSIENT;
            }
            sendFocusLossLocked(entry.getAudioFocusInfo(), lossType);

            // The entry no longer holds focus, so take it out of the holders list
            mFocusHolders.remove(entry.getAudioFocusInfo().getClientId());

            if (permanent) {
                permanentlyLost.add(entry);
            } else {
                // Add ourselves to the list of requests waiting to get focus back and
                // note why we lost focus so we can tell when it's time to get it back
                mFocusLosers.put(entry.getAudioFocusInfo().getClientId(), entry);
                entry.addBlocker(newEntry);
            }
        }

        // Now that all new blockers have been added, clear out any other requests that have been
        // permanently lost as a result of this request. Treat them as abandoned - if they're on
        // any blocker lists, remove them. If any focus requests become unblocked as a result,
        // re-grant them. (This can happen when a GAIN_TRANSIENT_MAY_DUCK request replaces a
        // GAIN_TRANSIENT request from the same listener.)
        for (FocusEntry entry : permanentlyLost) {
            Log.d(TAG, "Cleaning up entry " + entry.getClientId());
            removeBlockerAndRestoreUnblockedWaitersLocked(entry);
        }

        // Finally, add the request we're granting to the focus holders' list
        if (delayFocusForCurrentRequest) {
            swapDelayedAudioFocusRequestLocked(afi);
            return AudioManager.AUDIOFOCUS_REQUEST_DELAYED;
        }

        mFocusHolders.put(afi.getClientId(), newEntry);

        Log.i(TAG, "AUDIOFOCUS_REQUEST_GRANTED");
        return AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
    }

    @Override
    public void onAudioFocusRequest(AudioFocusInfo afi, int requestResult) {
        int response;
        AudioPolicy policy;
        AudioFocusInfo replacedDelayedAudioFocusInfo = null;
        synchronized (mLock) {
            policy = mAudioPolicy;
            response = evaluateFocusRequestLocked(afi);
        }

        // Post our reply for delivery to the original focus requester
        mAudioManager.setFocusRequestResult(afi, response, policy);
        logFocusEvent("onAudioFocusRequest for client " + afi.getClientId()
                + " with gain type " + focusEventToString(afi.getGainRequest())
                + " resulted in " + focusRequestResponseToString(response));
    }

    private void swapDelayedAudioFocusRequestLocked(AudioFocusInfo afi) {
        // If we are swapping to a different client then send the focus loss signal
        if (mDelayedRequest != null
                && !afi.getClientId().equals(mDelayedRequest.getClientId())) {
            sendFocusLossLocked(mDelayedRequest, AudioManager.AUDIOFOCUS_LOSS);
        }
        mDelayedRequest = afi;
    }

    private boolean canReceiveDelayedFocus(AudioFocusInfo afi) {
        if (afi.getGainRequest() != AudioManager.AUDIOFOCUS_GAIN) {
            return false;
        }
        return (afi.getFlags() & AudioManager.AUDIOFOCUS_FLAG_DELAY_OK)
            == AudioManager.AUDIOFOCUS_FLAG_DELAY_OK;
    }

    /**
     * @see AudioManager#abandonAudioFocus(AudioManager.OnAudioFocusChangeListener, AudioAttributes)
     * Note that we'll get this call for a focus holder that dies while in the focus stack, so
     * we don't need to watch for death notifications directly.
     * */
    @Override
    public void onAudioFocusAbandon(AudioFocusInfo afi) {
        logFocusEvent("onAudioFocusAbandon for client " + afi.getClientId());
        synchronized (mLock) {
            FocusEntry deadEntry = removeFocusEntryLocked(afi);

            if (deadEntry != null) {
                removeBlockerAndRestoreUnblockedWaitersLocked(deadEntry);
            } else {
                removeDelayedAudioFocusRequestLocked(afi);
            }
        }
    }

    private void removeDelayedAudioFocusRequestLocked(AudioFocusInfo afi) {
        if (mDelayedRequest != null && afi.getClientId().equals(mDelayedRequest.getClientId())) {
            mDelayedRequest = null;
        }
    }

    /**
     * Remove Focus entry from focus holder or losers entry lists
     * @param afi Audio Focus Info to remove
     * @return Removed Focus Entry
     */
    private FocusEntry removeFocusEntryLocked(AudioFocusInfo afi) {
        Log.i(TAG, "removeFocusEntry " + afi.getClientId());

        // Remove this entry from our active or pending list
        FocusEntry deadEntry = mFocusHolders.remove(afi.getClientId());
        if (deadEntry == null) {
            deadEntry = mFocusLosers.remove(afi.getClientId());
            if (deadEntry == null) {
                // Caller is providing an unrecognzied clientId!?
                Log.w(TAG, "Audio focus abandoned by unrecognized client id: " + afi.getClientId());
                // This probably means an app double released focused for some reason.  One
                // harmless possibility is a race between an app being told it lost focus and the
                // app voluntarily abandoning focus.  More likely the app is just sloppy.  :)
                // The more nefarious possibility is that the clientId is actually corrupted
                // somehow, in which case we might have a real focus entry that we're going to fail
                // to remove. If that were to happen, I'd expect either the app to swallow it
                // silently, or else take unexpected action (eg: resume playing spontaneously), or
                // else to see "Failure to signal ..." gain/loss error messages in the log from
                // this module when a focus change tries to take action on a truly zombie entry.
            }
        }
        return deadEntry;
    }

    private void removeBlockerAndRestoreUnblockedWaitersLocked(FocusEntry deadEntry) {
        attemptToGainFocusForDelayedAudioFocusRequest();
        removeBlockerAndRestoreUnblockedFocusLosersLocked(deadEntry);
    }

    private void attemptToGainFocusForDelayedAudioFocusRequest() {
        if (!mEnabledDelayedFocusRequest || mDelayedRequest == null) {
            return;
        }
        int delayedFocusRequestResults = evaluateFocusRequestLocked(mDelayedRequest);
        if (delayedFocusRequestResults == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            FocusEntry focusEntry = mFocusHolders.get(mDelayedRequest.getClientId());
            mDelayedRequest = null;
            if (dispatchFocusGainedLocked(focusEntry.getAudioFocusInfo())
                    == AudioManager.AUDIOFOCUS_REQUEST_FAILED) {
                Log.e(TAG,
                        "Failure to signal gain of audio focus gain for "
                                + "delayed focus clientId " + focusEntry.getClientId());
                mFocusHolders.remove(focusEntry.getClientId());
                removeBlockerFromBlockedFocusLosersLocked(focusEntry);
                sendFocusLossLocked(focusEntry.getAudioFocusInfo(),
                        AudioManager.AUDIOFOCUS_LOSS);
                logFocusEvent("Did not gained delayed audio focus for "
                        + focusEntry.getClientId());
            }
        }
    }

    /**
     * Removes the dead entry from blocked waiters but does not send focus gain signal
     */
    private void removeBlockerFromBlockedFocusLosersLocked(FocusEntry deadEntry) {
        // Remove this entry from the blocking list of any pending requests
        Iterator<FocusEntry> it = mFocusLosers.values().iterator();
        while (it.hasNext()) {
            FocusEntry entry = it.next();
            // Remove the retiring entry from all blocker lists
            entry.removeBlocker(deadEntry);
        }
    }

    /**
     * Removes the dead entry from blocked waiters and sends focus gain signal
     */
    private void removeBlockerAndRestoreUnblockedFocusLosersLocked(FocusEntry deadEntry) {
        // Remove this entry from the blocking list of any pending requests
        Iterator<FocusEntry> it = mFocusLosers.values().iterator();
        while (it.hasNext()) {
            FocusEntry entry = it.next();

            // Remove the retiring entry from all blocker lists
            entry.removeBlocker(deadEntry);

            // Any entry whose blocking list becomes empty should regain focus
            if (entry.isUnblocked()) {
                Log.i(TAG, "Restoring unblocked entry " + entry.getClientId());
                // Pull this entry out of the focus losers list
                it.remove();

                // Add it back into the focus holders list
                mFocusHolders.put(entry.getClientId(), entry);

                dispatchFocusGainedLocked(entry.getAudioFocusInfo());
            }
        }
    }

    /**
     * Dispatch focus gain
     * @param afi Audio focus info
     * @return AudioManager.AUDIOFOCUS_REQUEST_GRANTED if focus is dispatched successfully
     */
    private int dispatchFocusGainedLocked(AudioFocusInfo afi) {
        // Send the focus (re)gain notification
        int result = mAudioManager.dispatchAudioFocusChange(
                afi,
                afi.getGainRequest(),
                mAudioPolicy);
        if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            // TODO:  Is this actually an error, or is it okay for an entry in the focus
            // stack to NOT have a listener?  If that's the case, should we even keep
            // it in the focus stack?
            Log.e(TAG, "Failure to signal gain of audio focus with error: " + result);
        }

        logFocusEvent("dispatchFocusGainedLocked for client " + afi.getClientId()
                        + " with gain type " + focusEventToString(afi.getGainRequest())
                        + " resulted in " + focusRequestResponseToString(result));
        return result;
    }

    /**
     * Query the current list of focus loser for uid
     * @param uid uid to query current focus loser
     * @return list of current focus losers for uid
     */
    ArrayList<AudioFocusInfo> getAudioFocusLosersForUid(int uid) {
        return getAudioFocusListForUid(uid, mFocusLosers);
    }

    /**
     * Query the current list of focus holders for uid
     * @param uid uid to query current focus holders
     * @return list of current focus holders that for uid
     */
    ArrayList<AudioFocusInfo> getAudioFocusHoldersForUid(int uid) {
        return getAudioFocusListForUid(uid, mFocusHolders);
    }

    /**
     * Query input list for matching uid
     * @param uid uid to match in map
     * @param mapToQuery map to query for uid info
     * @return list of audio focus info that match uid
     */
    private ArrayList<AudioFocusInfo> getAudioFocusListForUid(int uid,
            HashMap<String, FocusEntry> mapToQuery) {
        ArrayList<AudioFocusInfo> matchingInfoList = new ArrayList<>();
        synchronized (mLock) {
            for (String clientId : mapToQuery.keySet()) {
                AudioFocusInfo afi = mapToQuery.get(clientId).getAudioFocusInfo();
                if (afi.getClientUid() == uid) {
                    matchingInfoList.add(afi);
                }
            }
        }
        return matchingInfoList;
    }

    /**
     * Remove the audio focus info, if entry is still active
     * dispatch lose focus transient to listeners
     * @param afi Audio Focus info to remove
     */
    void removeAudioFocusInfoAndTransientlyLoseFocus(AudioFocusInfo afi) {
        synchronized (mLock) {
            FocusEntry deadEntry = removeFocusEntryLocked(afi);
            if (deadEntry != null) {
                sendFocusLossLocked(deadEntry.getAudioFocusInfo(),
                        AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);
                removeBlockerAndRestoreUnblockedWaitersLocked(deadEntry);
            }
        }
    }

    /**
     * Reevaluate focus request and regain focus
     * @param afi audio focus info to reevaluate
     * @return AudioManager.AUDIOFOCUS_REQUEST_GRANTED if focus is granted
     */
    int reevaluateAndRegainAudioFocus(AudioFocusInfo afi) {
        int results;
        synchronized (mLock) {
            results = evaluateFocusRequestLocked(afi);
            if (results == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                results = dispatchFocusGainedLocked(afi);
            }
        }

        return results;
    }

    /**
     * dumps the current state of the CarAudioFocus object
     * @param indent indent to add to each line in the current stream
     * @param writer stream to write to
     */
    public void dump(String indent, PrintWriter writer) {
        synchronized (mLock) {
            writer.printf("%s*CarAudioFocus*\n", indent);
            String innerIndent = indent + "\t";
            String focusIndent = innerIndent + "\t";
            mFocusInteraction.dump(innerIndent, writer);

            writer.printf("%sCurrent Focus Holders:\n", innerIndent);
            for (String clientId : mFocusHolders.keySet()) {
                mFocusHolders.get(clientId).dump(focusIndent, writer);
            }

            writer.printf("%sTransient Focus Losers:\n", innerIndent);
            for (String clientId : mFocusLosers.keySet()) {
                mFocusLosers.get(clientId).dump(focusIndent, writer);
            }

            writer.printf("%sQueued Delayed Focus: %s\n", innerIndent,
                    mDelayedRequest == null ? "None" : mDelayedRequest.getClientId());

            writer.printf("%sFocus Events:\n", innerIndent);
            mFocusEventLogger.dump(innerIndent + "\t", writer);
        }
    }

    private static String focusEventToString(int focusEvent) {
        switch (focusEvent) {
            case AudioManager.AUDIOFOCUS_GAIN:
                return "GAIN";
            case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT:
                return "GAIN_TRANSIENT";
            case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE:
                return "GAIN_TRANSIENT_EXCLUSIVE";
            case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK:
                return "GAIN_TRANSIENT_MAY_DUCK";
            case AudioManager.AUDIOFOCUS_LOSS:
                return "LOSS";
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                return "LOSS_TRANSIENT";
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                return "LOSS_TRANSIENT_CAN_DUCK";
            default:
                return "unknown event " + focusEvent;
        }
    }

    private static String focusRequestResponseToString(int response) {
        if (response == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            return "REQUEST_GRANTED";
        } else if (response == AudioManager.AUDIOFOCUS_REQUEST_FAILED) {
            return "REQUEST_FAILED";
        }
        return "REQUEST_DELAYED";
    }

    private void logFocusEvent(String log) {
        mFocusEventLogger.log(log);
        Log.i(TAG, log);
    }

    /**
     * Returns the focus interaction for this car focus instance.
     */
    public FocusInteraction getFocusInteraction() {
        return mFocusInteraction;
    }
}
