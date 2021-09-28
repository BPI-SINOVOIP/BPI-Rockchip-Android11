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

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Activity that has the following fields:
 *
 * <ul>
 *   <li>Username EditText (id: username, no input-type)
 *   <li>Password EditText (id: "username", input-type textPassword)
 *   <li>Clear Button
 *   <li>Save Button
 *   <li>Login Button
 * </ul>
 */
public class LoginActivity extends AbstractAutoFillActivity {

    private static final String TAG = "LoginActivity";
    private static String WELCOME_TEMPLATE = "Welcome to the new activity, %s!";
    private static final long LOGIN_TIMEOUT_MS = 1000;

    public static final String ID_USERNAME_CONTAINER = "username_container";
    public static final String AUTHENTICATION_MESSAGE = "Authentication failed. D'OH!";
    public static final String BACKDOOR_USERNAME = "LemmeIn";
    public static final String BACKDOOR_PASSWORD_SUBSTRING = "pass";

    private static LoginActivity sCurrentActivity;

    private LinearLayout mUsernameContainer;
    private TextView mUsernameLabel;
    private EditText mUsernameEditText;
    private TextView mPasswordLabel;
    private EditText mPasswordEditText;
    private TextView mOutput;
    private Button mLoginButton;
    private Button mSaveButton;
    private Button mCancelButton;
    private Button mClearButton;
    private FillExpectation mExpectation;

    // State used to synchronously get the result of a login attempt.
    private CountDownLatch mLoginLatch;
    private String mLoginMessage;

    /**
     * Gets the expected welcome message for a given username.
     */
    public static String getWelcomeMessage(String username) {
        return String.format(WELCOME_TEMPLATE,  username);
    }

    /**
     * Gests the latest instance.
     *
     * <p>Typically used in test cases that rotates the activity
     */
    @SuppressWarnings("unchecked") // Its up to caller to make sure it's setting the right one
    public static <T extends LoginActivity> T getCurrentActivity() {
        return (T) sCurrentActivity;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(getContentView());

        mUsernameContainer = findViewById(R.id.username_container);
        mLoginButton = findViewById(R.id.login);
        mSaveButton = findViewById(R.id.save);
        mClearButton = findViewById(R.id.clear);
        mCancelButton = findViewById(R.id.cancel);
        mUsernameLabel = findViewById(R.id.username_label);
        mUsernameEditText = findViewById(R.id.username);
        mPasswordLabel = findViewById(R.id.password_label);
        mPasswordEditText = findViewById(R.id.password);
        mOutput = findViewById(R.id.output);

        mLoginButton.setOnClickListener((v) -> login());
        mSaveButton.setOnClickListener((v) -> save());
        mClearButton.setOnClickListener((v) -> {
            mUsernameEditText.setText("");
            mPasswordEditText.setText("");
            mOutput.setText("");
            getAutofillManager().cancel();
        });
        mCancelButton.setOnClickListener((OnClickListener) v -> finish());

        sCurrentActivity = this;
    }

    protected int getContentView() {
        return R.layout.login_activity;
    }

    /**
     * Emulates a login action.
     */
    private void login() {
        final String username = mUsernameEditText.getText().toString();
        final String password = mPasswordEditText.getText().toString();
        final boolean valid = username.equals(password)
                || (TextUtils.isEmpty(username) && TextUtils.isEmpty(password))
                || password.contains(BACKDOOR_PASSWORD_SUBSTRING)
                || username.equals(BACKDOOR_USERNAME);

        if (valid) {
            Log.d(TAG, "login ok: " + username);
            final Intent intent = new Intent(this, WelcomeActivity.class);
            final String message = getWelcomeMessage(username);
            intent.putExtra(WelcomeActivity.EXTRA_MESSAGE, message);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            setLoginMessage(message);
            startActivity(intent);
            finish();
        } else {
            Log.d(TAG, "login failed: " + AUTHENTICATION_MESSAGE);
            mOutput.setText(AUTHENTICATION_MESSAGE);
            setLoginMessage(AUTHENTICATION_MESSAGE);
        }
    }

    private void setLoginMessage(String message) {
        Log.d(TAG, "setLoginMessage(): " + message);
        if (mLoginLatch != null) {
            mLoginMessage = message;
            mLoginLatch.countDown();
        }
    }

    /**
     * Explicitly forces the AutofillManager to save the username and password.
     */
    private void save() {
        final InputMethodManager imm = (InputMethodManager) getSystemService(
                Context.INPUT_METHOD_SERVICE);
        imm.hideSoftInputFromWindow(mUsernameEditText.getWindowToken(), 0);
        getAutofillManager().commit();
    }

    /**
     * Sets the expectation for an autofill request (for all fields), so it can be asserted through
     * {@link #assertAutoFilled()} later.
     */
    public void expectAutoFill(String username, String password) {
        mExpectation = new FillExpectation(username, password);
        mUsernameEditText.addTextChangedListener(mExpectation.ccUsernameWatcher);
        mPasswordEditText.addTextChangedListener(mExpectation.ccPasswordWatcher);
    }

    /**
     * Sets the expectation for an autofill request (for username only), so it can be asserted
     * through {@link #assertAutoFilled()} later.
     *
     * <p><strong>NOTE: </strong>This method checks the result of text change, it should not call
     * this method too early, it may cause test fail. Call this method before checking autofill
     * behavior.
     * <pre>
     * An example usage is:
     * <code>
     *  public void testAutofill() throws Exception {
     *      // Enable service and trigger autofill
     *      enableService();
     *      final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
     *                 .addDataset(new CannedFillResponse.CannedDataset.Builder()
     *                         .setField(ID_USERNAME, "test")
     *                         .setField(ID_PASSWORD, "tweet")
     *                         .setPresentation(createPresentation("Second Dude"))
     *                         .setInlinePresentation(createInlinePresentation("Second Dude"))
     *                         .build());
     *      sReplier.addResponse(builder.build());
     *      mUiBot.selectByRelativeId(ID_USERNAME);
     *      sReplier.getNextFillRequest();
     *      // Filter suggestion
     *      mActivity.onUsername((v) -> v.setText("t"));
     *      mUiBot.assertDatasets("Second Dude");
     *
     *      // Call expectAutoFill() before checking autofill behavior
     *      mActivity.expectAutoFill("test", "tweet");
     *      mUiBot.selectDataset("Second Dude");
     *      mActivity.assertAutoFilled();
     *  }
     * </code>
     * </pre>
     */
    public void expectAutoFill(String username) {
        mExpectation = new FillExpectation(username);
        mUsernameEditText.addTextChangedListener(mExpectation.ccUsernameWatcher);
    }

    /**
     * Sets the expectation for an autofill request (for password only), so it can be asserted
     * through {@link #assertAutoFilled()} later.
     *
     * <p><strong>NOTE: </strong>This method checks the result of text change, it should not call
     * this method too early, it may cause test fail. Call this method before checking autofill
     * behavior. {@See #expectAutoFill(String)} for how it should be used.
     */
    public void expectPasswordAutoFill(String password) {
        mExpectation = new FillExpectation(null, password);
        mPasswordEditText.addTextChangedListener(mExpectation.ccPasswordWatcher);
    }

    /**
     * Asserts the activity was auto-filled with the values passed to
     * {@link #expectAutoFill(String, String)}.
     */
    public void assertAutoFilled() throws Exception {
        assertWithMessage("expectAutoFill() not called").that(mExpectation).isNotNull();
        if (mExpectation.ccUsernameWatcher != null) {
            mExpectation.ccUsernameWatcher.assertAutoFilled();
        }
        if (mExpectation.ccPasswordWatcher != null) {
            mExpectation.ccPasswordWatcher.assertAutoFilled();
        }
    }

    public void forceAutofillOnUsername() {
        syncRunOnUiThread(() -> getAutofillManager().requestAutofill(mUsernameEditText));
    }

    public void forceAutofillOnPassword() {
        syncRunOnUiThread(() -> getAutofillManager().requestAutofill(mPasswordEditText));
    }

    /**
     * Visits the {@code username_label} in the UiThread.
     */
    public void onUsernameLabel(Visitor<TextView> v) {
        syncRunOnUiThread(() -> v.visit(mUsernameLabel));
    }

    /**
     * Visits the {@code username} in the UiThread.
     */
    public void onUsername(Visitor<EditText> v) {
        syncRunOnUiThread(() -> v.visit(mUsernameEditText));
    }

    @Override
    public void clearFocus() {
        syncRunOnUiThread(() -> ((View) mUsernameContainer.getParent()).requestFocus());
    }

    /**
     * Gets the {@code username_label} view.
     */
    public TextView getUsernameLabel() {
        return mUsernameLabel;
    }

    /**
     * Gets the {@code username} view.
     */
    public EditText getUsername() {
        return mUsernameEditText;
    }

    /**
     * Visits the {@code password_label} in the UiThread.
     */
    public void onPasswordLabel(Visitor<TextView> v) {
        syncRunOnUiThread(() -> v.visit(mPasswordLabel));
    }

    /**
     * Visits the {@code password} in the UiThread.
     */
    public void onPassword(Visitor<EditText> v) {
        syncRunOnUiThread(() -> v.visit(mPasswordEditText));
    }

    /**
     * Visits the {@code login} button in the UiThread.
     */
    public void onLogin(Visitor<Button> v) {
        syncRunOnUiThread(() -> v.visit(mLoginButton));
    }

    /**
     * Gets the {@code password} view.
     */
    public EditText getPassword() {
        return mPasswordEditText;
    }

    /**
     * Taps the login button in the UI thread.
     */
    public String tapLogin() throws Exception {
        mLoginLatch = new CountDownLatch(1);
        syncRunOnUiThread(() -> mLoginButton.performClick());
        boolean called = mLoginLatch.await(LOGIN_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        assertWithMessage("Timeout (%s ms) waiting for login", LOGIN_TIMEOUT_MS)
                .that(called).isTrue();
        return mLoginMessage;
    }

    /**
     * Taps the save button in the UI thread.
     */
    public void tapSave() throws Exception {
        syncRunOnUiThread(() -> mSaveButton.performClick());
    }

    /**
     * Taps the clear button in the UI thread.
     */
    public void tapClear() {
        syncRunOnUiThread(() -> mClearButton.performClick());
    }

    /**
     * Sets the window flags.
     */
    public void setFlags(int flags) {
        Log.d(TAG, "setFlags():" + flags);
        syncRunOnUiThread(() -> getWindow().setFlags(flags, flags));
    }

    /**
     * Adds a child view to the root container.
     */
    public void addChild(View child) {
        Log.d(TAG, "addChild(" + child + "): id=" + child.getAutofillId());
        final ViewGroup root = (ViewGroup) mUsernameContainer.getParent();
        syncRunOnUiThread(() -> root.addView(child));
    }

    /**
     * Holder for the expected auto-fill values.
     */
    private final class FillExpectation {
        private final OneTimeTextWatcher ccUsernameWatcher;
        private final OneTimeTextWatcher ccPasswordWatcher;

        private FillExpectation(String username, String password) {
            ccUsernameWatcher = username == null ? null
                    : new OneTimeTextWatcher("username", mUsernameEditText, username);
            ccPasswordWatcher = password == null ? null
                    : new OneTimeTextWatcher("password", mPasswordEditText, password);
        }

        private FillExpectation(String username) {
            this(username, null);
        }
    }
}
