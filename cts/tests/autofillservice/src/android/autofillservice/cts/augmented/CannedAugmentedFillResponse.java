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
package android.autofillservice.cts.augmented;

import static android.autofillservice.cts.augmented.AugmentedHelper.getContentDescriptionForUi;

import android.autofillservice.cts.R;
import android.content.Context;
import android.content.IntentSender;
import android.os.Bundle;
import android.service.autofill.InlinePresentation;
import android.service.autofill.augmented.FillCallback;
import android.service.autofill.augmented.FillController;
import android.service.autofill.augmented.FillRequest;
import android.service.autofill.augmented.FillResponse;
import android.service.autofill.augmented.FillWindow;
import android.service.autofill.augmented.PresentationParams;
import android.service.autofill.augmented.PresentationParams.Area;
import android.util.ArrayMap;
import android.util.Log;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Collectors;

/**
 * Helper class used to produce a {@link FillResponse}.
 */
public final class CannedAugmentedFillResponse {

    private static final String TAG = CannedAugmentedFillResponse.class.getSimpleName();

    public static final String CLIENT_STATE_KEY = "clientStateKey";
    public static final String CLIENT_STATE_VALUE = "clientStateValue";

    private final AugmentedResponseType mResponseType;
    private final Map<AutofillId, Dataset> mDatasets;
    private long mDelay;
    private final Dataset mOnlyDataset;
    private final @Nullable List<Dataset> mInlineSuggestions;

    private CannedAugmentedFillResponse(@NonNull Builder builder) {
        mResponseType = builder.mResponseType;
        mDatasets = builder.mDatasets;
        mDelay = builder.mDelay;
        mOnlyDataset = builder.mOnlyDataset;
        mInlineSuggestions = builder.mInlineSuggestions;
    }

    /**
     * Constant used to pass a {@code null} response to the
     * {@link FillCallback#onSuccess(FillResponse)} method.
     */
    public static final CannedAugmentedFillResponse NO_AUGMENTED_RESPONSE =
            new Builder(AugmentedResponseType.NULL).build();

    /**
     * Constant used to emulate a timeout by not calling any method on {@link FillCallback}.
     */
    public static final CannedAugmentedFillResponse DO_NOT_REPLY_AUGMENTED_RESPONSE =
            new Builder(AugmentedResponseType.TIMEOUT).build();

    public AugmentedResponseType getResponseType() {
        return mResponseType;
    }

    public long getDelay() {
        return mDelay;
    }

    /**
     * Creates the "real" response.
     */
    public FillResponse asFillResponse(@NonNull Context context, @NonNull FillRequest request,
            @NonNull FillController controller) {
        final AutofillId focusedId = request.getFocusedId();

        final Dataset dataset;
        if (mOnlyDataset != null) {
            dataset = mOnlyDataset;
        } else {
            dataset = mDatasets.get(focusedId);
        }
        if (dataset == null) {
            Log.d(TAG, "no dataset for field " + focusedId);
            return null;
        }

        Log.d(TAG, "asFillResponse: id=" + focusedId + ", dataset=" + dataset);

        final PresentationParams presentationParams = request.getPresentationParams();
        if (presentationParams == null) {
            Log.w(TAG, "No PresentationParams");
            return null;
        }

        final Area strip = presentationParams.getSuggestionArea();
        if (strip == null) {
            Log.w(TAG, "No suggestion strip");
            return null;
        }

        if (mInlineSuggestions != null) {
            return createResponseWithInlineSuggestion();
        }

        final LayoutInflater inflater = LayoutInflater.from(context);
        final TextView rootView = (TextView) inflater.inflate(R.layout.augmented_autofill_ui, null);

        Log.d(TAG, "Setting autofill UI text to:" + dataset.mPresentation);
        rootView.setText(dataset.mPresentation);

        rootView.setContentDescription(getContentDescriptionForUi(focusedId));
        final FillWindow fillWindow = new FillWindow();
        rootView.setOnClickListener((v) -> {
            Log.d(TAG, "Destroying window first");
            fillWindow.destroy();
            final List<Pair<AutofillId, AutofillValue>> values;
            final AutofillValue onlyValue = dataset.getOnlyFieldValue();
            if (onlyValue != null) {
                Log.i(TAG, "Autofilling only value for " + focusedId + " as " + onlyValue);
                values = new ArrayList<>(1);
                values.add(new Pair<AutofillId, AutofillValue>(focusedId, onlyValue));
            } else {
                values = dataset.getValues();
                Log.i(TAG, "Autofilling: " + AugmentedHelper.toString(values));
            }
            controller.autofill(values);
        });

        boolean ok = fillWindow.update(strip, rootView, 0);
        if (!ok) {
            Log.w(TAG, "FillWindow.update() failed for " + strip + " and " + rootView);
            return null;
        }

        return new FillResponse.Builder().setFillWindow(fillWindow).build();
    }

    @Override
    public String toString() {
        return "CannedAugmentedFillResponse: [type=" + mResponseType
                + ", onlyDataset=" + mOnlyDataset
                + ", datasets=" + mDatasets
                + "]";
    }

    public enum AugmentedResponseType {
        NORMAL,
        NULL,
        TIMEOUT,
    }

    private Bundle newClientState() {
        Bundle b = new Bundle();
        b.putString(CLIENT_STATE_KEY, CLIENT_STATE_VALUE);
        return b;
    }

    private FillResponse createResponseWithInlineSuggestion() {
        List<android.service.autofill.Dataset> list = new ArrayList<>();
        for (Dataset dataset : mInlineSuggestions) {
            if (!dataset.getValues().isEmpty()) {
                android.service.autofill.Dataset.Builder datasetBuilder =
                        new android.service.autofill.Dataset.Builder();
                for (Pair<AutofillId, AutofillValue> pair : dataset.getValues()) {
                    final AutofillId id = pair.first;
                    datasetBuilder.setFieldInlinePresentation(id, pair.second, null,
                            dataset.mFieldPresentationById.get(id));
                    datasetBuilder.setAuthentication(dataset.mAuthentication);
                }
                list.add(datasetBuilder.build());
            }
        }
        return new FillResponse.Builder().setInlineSuggestions(list).setClientState(
                newClientState()).build();
    }

    public static final class Builder {
        private final Map<AutofillId, Dataset> mDatasets = new ArrayMap<>();
        private final AugmentedResponseType mResponseType;
        private long mDelay;
        private Dataset mOnlyDataset;
        private @Nullable List<Dataset> mInlineSuggestions;

        public Builder(@NonNull AugmentedResponseType type) {
            mResponseType = type;
        }

        public Builder() {
            this(AugmentedResponseType.NORMAL);
        }

        /**
         * Sets the {@link Dataset} that will be filled when the given {@code ids} is focused and
         * the UI is tapped.
         */
        @NonNull
        public Builder setDataset(@NonNull Dataset dataset, @NonNull AutofillId... ids) {
            if (mOnlyDataset != null) {
                throw new IllegalStateException("already called setOnlyDataset()");
            }
            for (AutofillId id : ids) {
                mDatasets.put(id, dataset);
            }
            return this;
        }

        /**
         * The {@link android.service.autofill.Dataset}s representing the inline suggestions data.
         * Defaults to null if no inline suggestions are available from the service.
         */
        @NonNull
        public Builder addInlineSuggestion(@NonNull Dataset dataset) {
            if (mInlineSuggestions == null) {
                mInlineSuggestions = new ArrayList<>();
            }
            mInlineSuggestions.add(dataset);
            return this;
        }

        /**
         * Sets the delay for onFillRequest().
         */
        public Builder setDelay(long delay) {
            mDelay = delay;
            return this;
        }

        /**
         * Sets the only dataset that will be returned.
         *
         * <p>Used when the test case doesn't know the autofill id of the focused field.
         * @param dataset
         */
        @NonNull
        public Builder setOnlyDataset(@NonNull Dataset dataset) {
            if (!mDatasets.isEmpty()) {
                throw new IllegalStateException("already called setDataset()");
            }
            mOnlyDataset = dataset;
            return this;
        }

        @NonNull
        public CannedAugmentedFillResponse build() {
            return new CannedAugmentedFillResponse(this);
        }
    } // CannedAugmentedFillResponse.Builder


    /**
     * Helper class used to define which fields will be autofilled when the user taps the Augmented
     * Autofill UI.
     */
    public static class Dataset {
        private final Map<AutofillId, AutofillValue> mFieldValuesById;
        private final Map<AutofillId, InlinePresentation> mFieldPresentationById;
        private final String mPresentation;
        private final AutofillValue mOnlyFieldValue;
        private final IntentSender mAuthentication;

        private Dataset(@NonNull Builder builder) {
            mFieldValuesById = builder.mFieldValuesById;
            mPresentation = builder.mPresentation;
            mOnlyFieldValue = builder.mOnlyFieldValue;
            mFieldPresentationById = builder.mFieldPresentationById;
            this.mAuthentication = builder.mAuthentication;
        }

        @NonNull
        public List<Pair<AutofillId, AutofillValue>> getValues() {
            return mFieldValuesById.entrySet().stream()
                    .map((entry) -> (new Pair<>(entry.getKey(), entry.getValue())))
                    .collect(Collectors.toList());
        }

        @Nullable
        public AutofillValue getOnlyFieldValue() {
            return mOnlyFieldValue;
        }

        @Override
        public String toString() {
            return "Dataset: [presentation=" + mPresentation
                    + ", onlyField=" + mOnlyFieldValue
                    + ", fields=" + mFieldValuesById
                    + ", auth=" + mAuthentication
                    + "]";
        }

        public static class Builder {
            private final Map<AutofillId, AutofillValue> mFieldValuesById = new ArrayMap<>();
            private final Map<AutofillId, InlinePresentation> mFieldPresentationById =
                    new ArrayMap<>();

            private final String mPresentation;
            private AutofillValue mOnlyFieldValue;
            private IntentSender mAuthentication;

            public Builder(@NonNull String presentation) {
                mPresentation = Objects.requireNonNull(presentation);
            }

            /**
             * Sets the value that will be autofilled on the field with {@code id}.
             */
            public Builder setField(@NonNull AutofillId id, @NonNull String text) {
                if (mOnlyFieldValue != null) {
                    throw new IllegalStateException("already called setOnlyField()");
                }
                mFieldValuesById.put(id, AutofillValue.forText(text));
                return this;
            }

            /**
             * Sets the value that will be autofilled on the field with {@code id}.
             */
            public Builder setField(@NonNull AutofillId id, @NonNull String text,
                    @NonNull InlinePresentation presentation) {
                if (mOnlyFieldValue != null) {
                    throw new IllegalStateException("already called setOnlyField()");
                }
                mFieldValuesById.put(id, AutofillValue.forText(text));
                mFieldPresentationById.put(id, presentation);
                return this;
            }

            /**
             * Sets this dataset to return the given {@code text} for the focused field.
             *
             * <p>Used when the test case doesn't know the autofill id of the focused field.
             */
            public Builder setOnlyField(@NonNull String text) {
                if (!mFieldValuesById.isEmpty()) {
                    throw new IllegalStateException("already called setField()");
                }
                mOnlyFieldValue = AutofillValue.forText(text);
                return this;
            }

            /**
             * Sets the authentication intent for this dataset.
             */
            public Builder setAuthentication(IntentSender authentication) {
                mAuthentication = authentication;
                return this;
            }

            public Dataset build() {
                return new Dataset(this);
            }
        } // Dataset.Builder
    } // Dataset
} // CannedAugmentedFillResponse
