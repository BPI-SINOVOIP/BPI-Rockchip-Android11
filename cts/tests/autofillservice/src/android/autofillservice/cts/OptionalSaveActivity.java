/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.autofillservice.cts;

import static com.google.common.truth.Truth.assertWithMessage;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;

import androidx.annotation.Nullable;

/**
 * Activity that has the following fields:
 *
 * <ul>
 *   <li>Address 1 EditText (id: address1)
 *   <li>Address 2 EditText (id: address2)
 *   <li>City EditText (id: city)
 *   <li>Favorite Color EditText (id: favorite_color)
 *   <li>Clear Button
 *   <li>SaveButton
 * </ul>
 *
 * <p>It's used to test auto-fill Save when not all fields are required.
 */
public class OptionalSaveActivity extends AbstractAutoFillActivity {

    static final String ID_ADDRESS1 = "address1";
    static final String ID_ADDRESS2 = "address2";
    static final String ID_CITY = "city";
    static final String ID_FAVORITE_COLOR = "favorite_color";

    EditText mAddress1;
    EditText mAddress2;
    EditText mCity;
    EditText mFavoriteColor;
    private Button mSaveButton;
    private Button mClearButton;
    private FillExpectation mExpectation;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.optional_save_activity);

        mAddress1 = (EditText) findViewById(R.id.address1);
        mAddress2 = (EditText) findViewById(R.id.address2);
        mCity = (EditText) findViewById(R.id.city);
        mFavoriteColor = (EditText) findViewById(R.id.favorite_color);
        mSaveButton = (Button) findViewById(R.id.save);
        mClearButton = (Button) findViewById(R.id.clear);
        mSaveButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                save();
            }
        });
        mClearButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                resetFields();
            }
        });
    }

    /**
     * Resets the values of the input fields.
     */
    private void resetFields() {
        mAddress1.setText("");
        mAddress2.setText("");
        mCity.setText("");
        mFavoriteColor.setText("");
    }

    /**
     * Emulates a save action.
     */
    void save() {
        final Intent intent = new Intent(this, WelcomeActivity.class);
        intent.putExtra(WelcomeActivity.EXTRA_MESSAGE, "Saved and sounded, please come again!");

        startActivity(intent);
        finish();
    }

    /**
     * Sets the expectation for an auto-fill request, so it can be asserted through
     * {@link #assertAutoFilled()} later.
     */
    void expectAutoFill(@Nullable String address1, @Nullable String address2, @Nullable String city,
            @Nullable String favColor) {
        mExpectation = new FillExpectation(address1, address2, city, favColor);
        if (address1 != null) {
            mAddress1.addTextChangedListener(mExpectation.address1Watcher);
        }
        if (address2 != null) {
            mAddress2.addTextChangedListener(mExpectation.address2Watcher);
        }
        if (city != null) {
            mCity.addTextChangedListener(mExpectation.cityWatcher);
        }
        if (favColor != null) {
            mFavoriteColor.addTextChangedListener(mExpectation.favoriteColorWatcher);
        }
    }

    /**
     * Asserts the activity was auto-filled with the values passed to
     * {@link #expectAutoFill(String, String, String, String)}.
     */
    void assertAutoFilled() throws Exception {
        assertWithMessage("expectAutoFill() not called").that(mExpectation).isNotNull();
        if (mExpectation.address1Watcher != null) {
            mExpectation.address1Watcher.assertAutoFilled();
        }
        if (mExpectation.address2Watcher != null) {
            mExpectation.address2Watcher.assertAutoFilled();
        }
        if (mExpectation.cityWatcher != null) {
            mExpectation.cityWatcher.assertAutoFilled();
        }
        if (mExpectation.favoriteColorWatcher != null) {
            mExpectation.favoriteColorWatcher.assertAutoFilled();
        }
    }

    /**
     * Holder for the expected auto-fill values.
     */
    private final class FillExpectation {
        private final OneTimeTextWatcher address1Watcher;
        private final OneTimeTextWatcher address2Watcher;
        private final OneTimeTextWatcher cityWatcher;
        private final OneTimeTextWatcher favoriteColorWatcher;

        private FillExpectation(@Nullable String address1, @Nullable String address2,
                @Nullable String city, @Nullable String favColor) {
            address1Watcher = address1 == null ? null
                    : new OneTimeTextWatcher("address1", mAddress1, address1);
            address2Watcher = address2 == null ? null
                    : new OneTimeTextWatcher("address2", mAddress2, address2);
            cityWatcher = city == null ? null : new OneTimeTextWatcher("city", mCity, city);
            favoriteColorWatcher = favColor == null ? null
                    : new OneTimeTextWatcher("favColor", mFavoriteColor, favColor);
        }
    }
}
