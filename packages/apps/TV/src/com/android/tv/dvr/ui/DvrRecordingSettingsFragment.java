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

package com.android.tv.dvr.ui;

import android.annotation.TargetApi;
import android.app.DialogFragment;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;

import com.android.tv.MainActivity;
import com.android.tv.R;
import com.android.tv.TvSingletons;
import com.android.tv.data.ProgramImpl;
import com.android.tv.data.api.Program;
import com.android.tv.dialog.SafeDismissDialogFragment;
import com.android.tv.dvr.DvrManager;
import com.android.tv.dvr.data.ScheduledRecording;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

import androidx.leanback.app.GuidedStepFragment;
import androidx.leanback.widget.GuidanceStylist.Guidance;
import androidx.leanback.widget.GuidedAction;
import androidx.leanback.widget.GuidedActionsStylist;

/** Fragment for DVR recording settings. */
@TargetApi(Build.VERSION_CODES.N)
@SuppressWarnings("AndroidApiChecker") // TODO(b/32513850) remove when error prone is updated
public class DvrRecordingSettingsFragment extends GuidedStepFragment {
    private static final String TAG = "RecordingSettingsFragment";

    private static final long ACTION_ID_START_EARLY = 100;
    private static final long ACTION_ID_END_LATE = 101;

    private static final int SUB_ACTION_ID_START_ON_TIME = 1;
    private static final int SUB_ACTION_ID_START_ONE_MIN = 2;
    private static final int SUB_ACTION_ID_START_FIVE_MIN = 3;
    private static final int SUB_ACTION_ID_START_FIFTEEN_MIN = 4;
    private static final int SUB_ACTION_ID_START_HALF_HOUR = 5;

    private static final int SUB_ACTION_ID_END_ON_TIME = 6;
    private static final int SUB_ACTION_ID_END_ONE_MIN = 7;
    private static final int SUB_ACTION_ID_END_FIFTEEN_MIN = 8;
    private static final int SUB_ACTION_ID_END_HALF_HOUR = 9;
    private static final int SUB_ACTION_ID_END_ONE_HOUR = 10;
    private static final int SUB_ACTION_ID_END_TWO_HOURS = 11;
    private static final int SUB_ACTION_ID_END_THREE_HOURS = 12;

    private Program mProgram;
    private String mFragmentTitle;
    private String mStartEarlyActionTitle;
    private String mEndLateActionTitle;
    private String mTimeActionOnTimeText;
    private String mTimeActionOneMinText;
    private String mTimeActionFiveMinText;
    private String mTimeActionFifteenMinText;
    private String mTimeActionHalfHourText;
    private String mTimeActionOneHourText;
    private String mTimeActionTwoHoursText;
    private String mTimeActionThreeHoursText;

    private GuidedAction mStartEarlyGuidedAction;
    private GuidedAction mEndLateGuidedAction;
    private long mStartEarlyTime = 0;
    private long mEndLateTime = 0;
    private DvrManager mDvrManager;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mDvrManager = TvSingletons.getSingletons(getContext()).getDvrManager();
        mProgram = getArguments().getParcelable(DvrRecordingSettingsActivity.PROGRAM);
        if (mProgram == null) {
            getActivity().finish();
            return;
        }
        mFragmentTitle = getString(R.string.dvr_recording_settings_title);
        mStartEarlyActionTitle = getString(R.string.dvr_start_early_title);
        mEndLateActionTitle = getString(R.string.dvr_end_late_title);
        mTimeActionOnTimeText = getString(R.string.dvr_recording_settings_time_none);
        mTimeActionOneMinText = getString(R.string.dvr_recording_settings_time_one_min);
        mTimeActionFiveMinText = getString(R.string.dvr_recording_settings_time_five_mins);
        mTimeActionFifteenMinText = getString(R.string.dvr_recording_settings_time_fifteen_mins);
        mTimeActionHalfHourText = getString(R.string.dvr_recording_settings_time_half_hour);
        mTimeActionOneHourText = getString(R.string.dvr_recording_settings_time_one_hour);
        mTimeActionTwoHoursText = getString(R.string.dvr_recording_settings_time_two_hours);
        mTimeActionThreeHoursText = getString(R.string.dvr_recording_settings_time_three_hours);
    }

    @Override
    public Guidance onCreateGuidance(Bundle savedInstanceState) {
        String breadcrumb = mProgram.getTitle();
        String title = mFragmentTitle;
        String description = mProgram.getEpisodeTitle() + "\n" + mProgram.getDescription();
        return new Guidance(title, description, breadcrumb, null);
    }

    @Override
    public void onCreateActions(List<GuidedAction> actions, Bundle savedInstanceState) {
        mStartEarlyGuidedAction =
                new GuidedAction.Builder(getActivity())
                        .id(ACTION_ID_START_EARLY)
                        .title(mStartEarlyActionTitle)
                        .description(mTimeActionOnTimeText)
                        .subActions(buildChannelSubActionStart())
                        .build();
        actions.add(mStartEarlyGuidedAction);

        mEndLateGuidedAction =
                new GuidedAction.Builder(getActivity())
                        .id(ACTION_ID_END_LATE)
                        .title(mEndLateActionTitle)
                        .description(mTimeActionOnTimeText)
                        .subActions(buildChannelSubActionEnd())
                        .build();
        actions.add(mEndLateGuidedAction);
    }

    @Override
    public void onCreateButtonActions(List<GuidedAction> actions, Bundle savedInstanceState) {
        actions.add(
                new GuidedAction.Builder(getActivity())
                        .clickAction(GuidedAction.ACTION_ID_OK)
                        .build());
        actions.add(
                new GuidedAction.Builder(getActivity())
                        .clickAction(GuidedAction.ACTION_ID_CANCEL)
                        .build());
    }

    @Override
    public void onGuidedActionClicked(GuidedAction action) {
        long actionId = action.getId();
        if (actionId == GuidedAction.ACTION_ID_OK) {
            long startEarlyTimeMs = TimeUnit.MINUTES.toMillis(mStartEarlyTime);
            long endLateTimeMs = TimeUnit.MINUTES.toMillis(mEndLateTime);
            long startTimeMs = mProgram.getStartTimeUtcMillis() - startEarlyTimeMs;
            if (startTimeMs < System.currentTimeMillis()) {
                startTimeMs = System.currentTimeMillis();
                startEarlyTimeMs = mProgram.getStartTimeUtcMillis() - startTimeMs;
            }
            long endTimeMs = mProgram.getEndTimeUtcMillis() + endLateTimeMs;
            Program customizedProgram =
                    new ProgramImpl.Builder(mProgram)
                        .setStartTimeUtcMillis(startTimeMs)
                        .setEndTimeUtcMillis(endTimeMs)
                        .build();
            mDvrManager.addSchedule(customizedProgram , startEarlyTimeMs, endLateTimeMs);
            List<ScheduledRecording> conflicts =
                    mDvrManager.getConflictingSchedules(customizedProgram);
            if (conflicts.isEmpty()) {
                DvrUiHelper.showAddScheduleToast(
                        getContext(),
                        customizedProgram.getTitle(),
                        customizedProgram.getStartTimeUtcMillis(),
                        customizedProgram.getEndTimeUtcMillis());
                dismissDialog();
                finishGuidedStepFragments();
            } else {
                DvrUiHelper.showScheduleConflictDialog(getActivity(), customizedProgram);
            }
        } else if (actionId == GuidedAction.ACTION_ID_CANCEL) {
            finishGuidedStepFragments();
        }
    }

    @Override
    public boolean onSubGuidedActionClicked(GuidedAction action) {
        long actionId = action.getId();
        switch ((int) actionId) {
            case SUB_ACTION_ID_START_ON_TIME :
                mStartEarlyTime = 0;
                updateGuidedActions(true, mTimeActionOnTimeText);
                break;
            case SUB_ACTION_ID_START_ONE_MIN :
                mStartEarlyTime = 1;
                updateGuidedActions(true, mTimeActionOneMinText);
                break;
            case SUB_ACTION_ID_START_FIVE_MIN :
                mStartEarlyTime = 5;
                updateGuidedActions(true, mTimeActionFiveMinText);
                break;
            case SUB_ACTION_ID_START_FIFTEEN_MIN :
                mStartEarlyTime = 15;
                updateGuidedActions(true, mTimeActionFifteenMinText);
                break;
            case SUB_ACTION_ID_START_HALF_HOUR :
                mStartEarlyTime = 30;
                updateGuidedActions(true, mTimeActionHalfHourText);
                break;
            case SUB_ACTION_ID_END_ON_TIME :
                mEndLateTime = 0;
                updateGuidedActions(false, mTimeActionOnTimeText);
                break;
            case SUB_ACTION_ID_END_ONE_MIN :
                mEndLateTime = 1;
                updateGuidedActions(false, mTimeActionOneMinText);
                break;
            case SUB_ACTION_ID_END_FIFTEEN_MIN :
                mEndLateTime = 15;
                updateGuidedActions(false, mTimeActionFifteenMinText);
                break;
            case SUB_ACTION_ID_END_HALF_HOUR :
                mEndLateTime = 30;
                updateGuidedActions(false, mTimeActionHalfHourText);
                break;
            case SUB_ACTION_ID_END_ONE_HOUR :
                mEndLateTime = 60;
                updateGuidedActions(false, mTimeActionOneHourText);
                break;
            case SUB_ACTION_ID_END_TWO_HOURS :
                mEndLateTime = 120;
                updateGuidedActions(false, mTimeActionTwoHoursText);
                break;
            case SUB_ACTION_ID_END_THREE_HOURS :
                mEndLateTime = 180;
                updateGuidedActions(false, mTimeActionThreeHoursText);
                break;
            default :
                mStartEarlyTime = 0;
                mEndLateTime = 0;
                updateGuidedActions(true, mTimeActionOnTimeText);
                updateGuidedActions(false, mTimeActionOnTimeText);
                break;
        }
        return true;
    }

    private void updateGuidedActions(boolean start, CharSequence description) {
        if (start) {
            mStartEarlyGuidedAction.setDescription(description);
            notifyActionChanged(findActionPositionById(ACTION_ID_START_EARLY));
        } else {
            mEndLateGuidedAction.setDescription(description);
            notifyActionChanged(findActionPositionById(ACTION_ID_END_LATE));
        }
    }

    @Override
    public GuidedActionsStylist onCreateButtonActionsStylist() {
        return new DvrGuidedActionsStylist(true);
    }

    private List<GuidedAction> buildChannelSubActionStart() {
        List<GuidedAction> timeSubActions = new ArrayList<>();
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_START_ON_TIME)
                        .title(mTimeActionOnTimeText)
                        .build());
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_START_ONE_MIN)
                        .title(mTimeActionOneMinText)
                        .build());
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_START_FIVE_MIN)
                        .title(mTimeActionFiveMinText)
                        .build());
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_START_FIFTEEN_MIN)
                        .title(mTimeActionFifteenMinText)
                        .build());
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_START_HALF_HOUR)
                        .title(mTimeActionHalfHourText)
                        .build());
        return timeSubActions;
    }

    private List<GuidedAction> buildChannelSubActionEnd() {
        List<GuidedAction> timeSubActions = new ArrayList<>();
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_END_ON_TIME)
                        .title(mTimeActionOnTimeText)
                        .build());
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_END_ONE_MIN)
                        .title(mTimeActionOneMinText)
                        .build());
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_END_FIFTEEN_MIN)
                        .title(mTimeActionFifteenMinText)
                        .build());
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_END_HALF_HOUR)
                        .title(mTimeActionHalfHourText)
                        .build());
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_END_ONE_HOUR)
                        .title(mTimeActionOneHourText)
                        .build());
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_END_TWO_HOURS)
                        .title(mTimeActionTwoHoursText)
                        .build());
        timeSubActions.add(
                new GuidedAction.Builder(getActivity())
                        .id(SUB_ACTION_ID_END_THREE_HOURS)
                        .title(mTimeActionThreeHoursText)
                        .build());
        return timeSubActions;
    }

    protected void dismissDialog() {
        if (getActivity() instanceof MainActivity) {
            SafeDismissDialogFragment currentDialog =
                    ((MainActivity) getActivity()).getOverlayManager().getCurrentDialog();
            if (currentDialog instanceof DvrHalfSizedDialogFragment) {
                currentDialog.dismiss();
            }
        } else if (getParentFragment() instanceof DialogFragment) {
            ((DialogFragment) getParentFragment()).dismiss();
        }
    }
}
