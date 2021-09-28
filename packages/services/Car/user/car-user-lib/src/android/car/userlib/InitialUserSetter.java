/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.car.userlib;

import static android.car.userlib.UserHalHelper.userFlagsToString;
import static android.car.userlib.UserHelper.safeName;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.app.IActivityManager;
import android.content.Context;
import android.content.pm.UserInfo;
import android.hardware.automotive.vehicle.V2_0.UserFlags;
import android.os.RemoteException;
import android.os.Trace;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;
import android.sysprop.CarProperties;
import android.util.Log;
import android.util.Pair;
import android.util.Slog;
import android.util.TimingsTraceLog;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.Preconditions;
import com.android.internal.widget.LockPatternUtils;

import java.io.PrintWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Consumer;

/**
 * Helper used to set the initial Android user on boot or when resuming from RAM.
 */
public final class InitialUserSetter {

    private static final String TAG = InitialUserSetter.class.getSimpleName();

    private static final boolean DBG = false;

    /**
     * Sets the initial user using the default behavior.
     *
     * <p>The default behavior is:
     *
     * <ol>
     *  <li>On first boot, it creates and switches to a new user.
     *  <li>Otherwise, it will switch to either:
     *  <ol>
     *   <li>User defined by {@code android.car.systemuser.bootuseroverrideid} (when it was
     * constructed with such option enabled).
     *   <li>Last active user (as defined by
     * {@link android.provider.Settings.Global.LAST_ACTIVE_USER_ID}.
     *  </ol>
     * </ol>
     */
    public static final int TYPE_DEFAULT_BEHAVIOR = 0;

    /**
     * Switches to the given user, falling back to {@link #fallbackDefaultBehavior(String)} if it
     * fails.
     */
    public static final int TYPE_SWITCH = 1;

    /**
     * Creates a new user and switches to it, falling back to
     * {@link #fallbackDefaultBehavior(String) if any of these steps fails.
     *
     * @param name (optional) name of the new user
     * @param halFlags user flags as defined by Vehicle HAL ({@code UserFlags} enum).
     */
    public static final int TYPE_CREATE = 2;

    /**
     * Creates a new guest user and switches to it, if current user is unlocked guest user.
     * Does not fallback if any of these steps fails. falling back to
     * {@link #fallbackDefaultBehavior(String) if any of these steps fails
     */
    public static final int TYPE_REPLACE_GUEST = 3;

    @IntDef(prefix = { "TYPE_" }, value = {
            TYPE_DEFAULT_BEHAVIOR,
            TYPE_SWITCH,
            TYPE_CREATE,
            TYPE_REPLACE_GUEST
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface InitialUserInfoType { }

    private final Context mContext;

    // TODO(b/150413304): abstract AM / UM into interfaces, then provide local and remote
    // implementation (where local is implemented by ActivityManagerInternal / UserManagerInternal)
    private final CarUserManagerHelper mHelper;
    private final UserManager mUm;
    private final LockPatternUtils mLockPatternUtils;

    private final String mNewUserName;
    private final String mNewGuestName;

    private final Consumer<UserInfo> mListener;

    public InitialUserSetter(@NonNull Context context, @NonNull Consumer<UserInfo> listener) {
        this(context, listener, /* newGuestName= */ null);
    }

    public InitialUserSetter(@NonNull Context context, @NonNull Consumer<UserInfo> listener,
            @Nullable String newGuestName) {
        this(context, new CarUserManagerHelper(context), UserManager.get(context), listener,
                new LockPatternUtils(context),
                context.getString(com.android.internal.R.string.owner_name), newGuestName);
    }

    @VisibleForTesting
    public InitialUserSetter(@NonNull Context context, @NonNull CarUserManagerHelper helper,
            @NonNull UserManager um, @NonNull Consumer<UserInfo> listener,
            @NonNull LockPatternUtils lockPatternUtils,
            @Nullable String newUserName, @Nullable String newGuestName) {
        mContext = context;
        mHelper = helper;
        mUm = um;
        mListener = listener;
        mLockPatternUtils = lockPatternUtils;
        mNewUserName = newUserName;
        mNewGuestName = newGuestName;
    }

    /**
     * Builder for {@link InitialUserInfo} objects.
     *
     */
    public static final class Builder {

        private final @InitialUserInfoType int mType;
        private boolean mReplaceGuest;
        private @UserIdInt int mSwitchUserId;
        private @Nullable String mNewUserName;
        private int mNewUserFlags;
        private boolean mSupportsOverrideUserIdProperty;
        private @Nullable String mUserLocales;

        /**
         * Constructor for the given type.
         *
         * @param type {@link #TYPE_DEFAULT_BEHAVIOR}, {@link #TYPE_SWITCH},
         * {@link #TYPE_CREATE} or {@link #TYPE_REPLACE_GUEST}.
         */
        public Builder(@InitialUserInfoType int type) {
            Preconditions.checkArgument(type == TYPE_DEFAULT_BEHAVIOR || type == TYPE_SWITCH
                    || type == TYPE_CREATE || type == TYPE_REPLACE_GUEST, "invalid builder type");
            mType = type;
        }

        /**
         * Sets the id of the user to be switched to.
         *
         * @throws IllegalArgumentException if builder is not for {@link #TYPE_SWITCH}.
         */
        @NonNull
        public Builder setSwitchUserId(@UserIdInt int userId) {
            Preconditions.checkArgument(mType == TYPE_SWITCH, "invalid builder type: " + mType);
            mSwitchUserId = userId;
            return this;
        }

        /**
         * Sets whether the current user should be replaced when it's a guest.
         */
        @NonNull
        public Builder setReplaceGuest(boolean value) {
            mReplaceGuest = value;
            return this;
        }

        /**
         * Sets the name of the new user being created.
         *
         * @throws IllegalArgumentException if builder is not for {@link #TYPE_CREATE}.
         */
        @NonNull
        public Builder setNewUserName(@Nullable String name) {
            Preconditions.checkArgument(mType == TYPE_CREATE, "invalid builder type: " + mType);
            mNewUserName = name;
            return this;
        }

        /**
         * Sets the flags (as defined by {@link android.hardware.automotive.vehicle.V2_0.UserFlags})
         * of the new user being created.
         *
         * @throws IllegalArgumentException if builder is not for {@link #TYPE_CREATE}.
         */
        @NonNull
        public Builder setNewUserFlags(int flags) {
            Preconditions.checkArgument(mType == TYPE_CREATE, "invalid builder type: " + mType);
            mNewUserFlags = flags;
            return this;
        }

        /**
         * Sets whether the {@link CarProperties#boot_user_override_id()} should be taking in
         * account when using the default behavior.
         */
        @NonNull
        public Builder setSupportsOverrideUserIdProperty(boolean value) {
            mSupportsOverrideUserIdProperty = value;
            return this;
        }

        /**
         * Sets the system locales for the initial user (when it's created).
         */
        @NonNull
        public Builder setUserLocales(@Nullable String userLocales) {
            mUserLocales = userLocales;
            return this;
        }

        /**
         * Builds the object.
         */
        @NonNull
        public InitialUserInfo build() {
            return new InitialUserInfo(this);
        }
    }

    /**
     * Object used to define the properties of the initial user (which can then be set by
     * {@link InitialUserSetter#set(InitialUserInfo)});
     */
    public static final class InitialUserInfo {
        public final @InitialUserInfoType int type;
        public final boolean replaceGuest;
        public final @UserIdInt int switchUserId;
        public final @Nullable String newUserName;
        public final int newUserFlags;
        public final boolean supportsOverrideUserIdProperty;
        public @Nullable String userLocales;

        private InitialUserInfo(@NonNull Builder builder) {
            type = builder.mType;
            switchUserId = builder.mSwitchUserId;
            replaceGuest = builder.mReplaceGuest;
            newUserName = builder.mNewUserName;
            newUserFlags = builder.mNewUserFlags;
            supportsOverrideUserIdProperty = builder.mSupportsOverrideUserIdProperty;
            userLocales = builder.mUserLocales;
        }
    }

    /**
     * Sets the initial user.
     */
    public void set(@NonNull InitialUserInfo info) {
        Preconditions.checkArgument(info != null, "info cannot be null");

        switch (info.type) {
            case TYPE_DEFAULT_BEHAVIOR:
                executeDefaultBehavior(info, /* fallback= */ false);
                break;
            case TYPE_SWITCH:
                try {
                    switchUser(info, /* fallback= */ true);
                } catch (Exception e) {
                    fallbackDefaultBehavior(info, /* fallback= */ true,
                            "Exception switching user: " + e);
                }
                break;
            case TYPE_CREATE:
                try {
                    createAndSwitchUser(info, /* fallback= */ true);
                } catch (Exception e) {
                    fallbackDefaultBehavior(info, /* fallback= */ true,
                            "Exception createUser user with name "
                                    + UserHelper.safeName(info.newUserName) + " and flags "
                                    + UserHalHelper.userFlagsToString(info.newUserFlags) + ": "
                                    + e);
                }
                break;
            case TYPE_REPLACE_GUEST:
                try {
                    replaceUser(info, /* fallback= */ true);
                } catch (Exception e) {
                    fallbackDefaultBehavior(info, /* fallback= */ true,
                            "Exception replace guest user: " + e);
                }
                break;
            default:
                throw new IllegalArgumentException("invalid InitialUserInfo type: " + info.type);
        }
    }

    private void replaceUser(InitialUserInfo info, boolean fallback) {
        int currentUserId = ActivityManager.getCurrentUser();
        UserInfo currentUser = mUm.getUserInfo(currentUserId);

        UserInfo newUser = replaceGuestIfNeeded(currentUser);
        if (newUser == null) {
            fallbackDefaultBehavior(info, fallback,
                    "could not replace guest " + currentUser.toFullString());
            return;
        }

        switchUser(new Builder(TYPE_SWITCH)
                .setSwitchUserId(newUser.id)
                .build(), fallback);

        if (newUser.id != currentUser.id) {
            Slog.i(TAG, "Removing old guest " + currentUser.id);
            if (!mUm.removeUser(currentUser.id)) {
                Slog.w(TAG, "Could not remove old guest " + currentUser.id);
            }
        }
    }

    private void executeDefaultBehavior(@NonNull InitialUserInfo info, boolean fallback) {
        if (!mHelper.hasInitialUser()) {
            if (DBG) Log.d(TAG, "executeDefaultBehavior(): no initial user, creating it");
            createAndSwitchUser(new Builder(TYPE_CREATE)
                    .setNewUserName(mNewUserName)
                    .setNewUserFlags(UserFlags.ADMIN)
                    .setSupportsOverrideUserIdProperty(info.supportsOverrideUserIdProperty)
                    .setUserLocales(info.userLocales)
                    .build(), fallback);
        } else {
            if (DBG) Log.d(TAG, "executeDefaultBehavior(): switching to initial user");
            int userId = mHelper.getInitialUser(info.supportsOverrideUserIdProperty);
            switchUser(new Builder(TYPE_SWITCH)
                    .setSwitchUserId(userId)
                    .setSupportsOverrideUserIdProperty(info.supportsOverrideUserIdProperty)
                    .setReplaceGuest(info.replaceGuest)
                    .build(), fallback);
        }
    }

    @VisibleForTesting
    void fallbackDefaultBehavior(@NonNull InitialUserInfo info, boolean fallback,
            @NonNull String reason) {
        if (!fallback) {
            // Only log the error
            Log.w(TAG, reason);
            // Must explicitly tell listener that initial user could not be determined
            notifyListener(/*initialUser= */ null);
            return;
        }
        Log.w(TAG, "Falling back to default behavior. Reason: " + reason);
        executeDefaultBehavior(info, /* fallback= */ false);
    }

    private void switchUser(@NonNull InitialUserInfo info, boolean fallback) {
        int userId = info.switchUserId;
        boolean replaceGuest = info.replaceGuest;

        if (DBG) {
            Log.d(TAG, "switchUser(): userId=" + userId + ", replaceGuest=" + replaceGuest
                    + ", fallback=" + fallback);
        }

        UserInfo user = mUm.getUserInfo(userId);
        if (user == null) {
            fallbackDefaultBehavior(info, fallback, "user with id " + userId + " doesn't exist");
            return;
        }

        UserInfo actualUser = user;

        if (user.isGuest() && replaceGuest) {
            actualUser = replaceGuestIfNeeded(user);

            if (actualUser == null) {
                fallbackDefaultBehavior(info, fallback, "could not replace guest "
                        + user.toFullString());
                return;
            }
        }

        int actualUserId = actualUser.id;

        unlockSystemUserIfNecessary(actualUserId);

        int currentUserId = ActivityManager.getCurrentUser();
        if (actualUserId != currentUserId) {
            if (!startForegroundUser(actualUserId)) {
                fallbackDefaultBehavior(info, fallback,
                        "am.switchUser(" + actualUserId + ") failed");
                return;
            }
            mHelper.setLastActiveUser(actualUserId);
        }
        notifyListener(actualUser);

        if (actualUserId != userId) {
            Slog.i(TAG, "Removing old guest " + userId);
            if (!mUm.removeUser(userId)) {
                Slog.w(TAG, "Could not remove old guest " + userId);
            }
        }
    }

    private void unlockSystemUserIfNecessary(@UserIdInt int userId) {
        // If system user is the only user to unlock, it will be handled when boot is complete.
        if (userId != UserHandle.USER_SYSTEM) {
            unlockSystemUser();
        }
    }

    /**
     * Check if the user is a guest and can be replaced.
     */
    public boolean canReplaceGuestUser(UserInfo user) {
        if (!user.isGuest()) return false;

        if (mLockPatternUtils.isSecure(user.id)) {
            if (DBG) {
                Log.d(TAG, "replaceGuestIfNeeded(), skipped, since user "
                        + user.id + " has secure lock pattern");
            }
            return false;
        }

        return true;
    }

    // TODO(b/151758646): move to CarUserManagerHelper
    /**
     * Replaces {@code user} by a new guest, if necessary.
     *
     * <p>If {@code user} is not a guest, it doesn't do anything and returns the same user.
     *
     * <p>Otherwise, it marks the current guest for deletion, creates a new one, and returns the
     * new guest (or {@code null} if a new guest could not be created).
     */

    @VisibleForTesting
    @Nullable
    UserInfo replaceGuestIfNeeded(@NonNull UserInfo user) {
        Preconditions.checkArgument(user != null, "user cannot be null");

        if (!canReplaceGuestUser(user)) {
            return user;
        }

        Log.i(TAG, "Replacing guest (" + user.toFullString() + ")");

        int halFlags = UserFlags.GUEST;
        if (user.isEphemeral()) {
            halFlags |= UserFlags.EPHEMERAL;
        } else {
            // TODO(b/150413515): decide whether we should allow it or not. Right now we're
            // just logging, as UserManagerService will automatically set it to ephemeral if
            // platform is set to do so.
            Log.w(TAG, "guest being replaced is not ephemeral: " + user.toFullString());
        }

        if (!mUm.markGuestForDeletion(user.id)) {
            // Don't need to recover in case of failure - most likely create new user will fail
            // because there is already a guest
            Log.w(TAG, "failed to mark guest " + user.id + " for deletion");
        }

        Pair<UserInfo, String> result = createNewUser(new Builder(TYPE_CREATE)
                .setNewUserName(mNewGuestName)
                .setNewUserFlags(halFlags)
                .build());

        String errorMessage = result.second;
        if (errorMessage != null) {
            Log.w(TAG, "could not replace guest " + user.toFullString() + ": " + errorMessage);
            return null;
        }

        return result.first;
    }

    private void createAndSwitchUser(@NonNull InitialUserInfo info, boolean fallback) {
        Pair<UserInfo, String> result = createNewUser(info);
        String reason = result.second;
        if (reason != null) {
            fallbackDefaultBehavior(info, fallback, reason);
            return;
        }

        switchUser(new Builder(TYPE_SWITCH)
                .setSwitchUserId(result.first.id)
                .setSupportsOverrideUserIdProperty(info.supportsOverrideUserIdProperty)
                .build(), fallback);
    }

    /**
     * Creates a new user.
     *
     * @return on success, first element is the new user; on failure, second element contains the
     * error message.
     */
    @NonNull
    private Pair<UserInfo, String> createNewUser(@NonNull InitialUserInfo info) {
        String name = info.newUserName;
        int halFlags = info.newUserFlags;

        if (DBG) {
            Log.d(TAG, "createUser(name=" + safeName(name) + ", flags="
                    + userFlagsToString(halFlags) + ")");
        }

        if (UserHalHelper.isSystem(halFlags)) {
            return new Pair<>(null, "Cannot create system user");
        }

        if (UserHalHelper.isAdmin(halFlags)) {
            boolean validAdmin = true;
            if (UserHalHelper.isGuest(halFlags)) {
                Log.w(TAG, "Cannot create guest admin");
                validAdmin = false;
            }
            if (UserHalHelper.isEphemeral(halFlags)) {
                Log.w(TAG, "Cannot create ephemeral admin");
                validAdmin = false;
            }
            if (!validAdmin) {
                return new Pair<>(null, "Invalid flags for admin user");
            }
        }
        // TODO(b/150413515): decide what to if HAL requested a non-ephemeral guest but framework
        // sets all guests as ephemeral - should it fail or just warn?

        int flags = UserHalHelper.toUserInfoFlags(halFlags);
        String type = UserHalHelper.isGuest(halFlags) ? UserManager.USER_TYPE_FULL_GUEST
                : UserManager.USER_TYPE_FULL_SECONDARY;

        if (DBG) {
            Log.d(TAG, "calling am.createUser((name=" + safeName(name) + ", type=" + type
                    + ", flags=" + UserInfo.flagsToString(flags) + ")");
        }

        UserInfo userInfo = mUm.createUser(name, type, flags);
        if (userInfo == null) {
            return new Pair<>(null, "createUser(name=" + safeName(name) + ", flags="
                    + userFlagsToString(halFlags) + "): failed to create user");
        }

        if (DBG) Log.d(TAG, "user created: " + userInfo.id);

        if (info.userLocales != null) {
            if (DBG) {
                Log.d(TAG, "setting locale for user " + userInfo.id + " to " + info.userLocales);
            }
            Settings.System.putStringForUser(mContext.getContentResolver(),
                    Settings.System.SYSTEM_LOCALES, info.userLocales, userInfo.id);
        }

        return new Pair<>(userInfo, null);
    }

    @VisibleForTesting
    void unlockSystemUser() {
        Log.i(TAG, "unlocking system user");
        IActivityManager am = ActivityManager.getService();

        TimingsTraceLog t = new TimingsTraceLog(TAG, Trace.TRACE_TAG_SYSTEM_SERVER);
        t.traceBegin("UnlockSystemUser");
        try {
            // This is for force changing state into RUNNING_LOCKED. Otherwise unlock does not
            // update the state and USER_SYSTEM unlock happens twice.
            t.traceBegin("am.startUser");
            boolean started = am.startUserInBackground(UserHandle.USER_SYSTEM);
            t.traceEnd();
            if (!started) {
                Log.w(TAG, "could not restart system user in foreground; trying unlock instead");
                t.traceBegin("am.unlockUser");
                boolean unlocked = am.unlockUser(UserHandle.USER_SYSTEM, /* token= */ null,
                        /* secret= */ null, /* listener= */ null);
                t.traceEnd();
                if (!unlocked) {
                    Log.w(TAG, "could not unlock system user neither");
                    return;
                }
            }
        } catch (RemoteException e) {
            // should not happen for local call.
            Log.wtf("RemoteException from AMS", e);
        } finally {
            t.traceEnd();
        }
    }

    @VisibleForTesting
    boolean startForegroundUser(@UserIdInt int userId) {
        if (UserHelper.isHeadlessSystemUser(userId)) {
            // System User doesn't associate with real person, can not be switched to.
            return false;
        }
        try {
            return ActivityManager.getService().startUserInForegroundWithListener(userId, null);
        } catch (RemoteException e) {
            Log.w(TAG, "failed to start user " + userId, e);
            return false;
        }
    }

    private void notifyListener(@Nullable UserInfo initialUser) {
        if (DBG) Log.d(TAG, "notifyListener(): " + initialUser);
        mListener.accept(initialUser);
    }

    /**
     * Dumps it state.
     */
    public void dump(@NonNull PrintWriter writer) {
        writer.println("InitialUserSetter");
        String indent = "  ";
        writer.printf("%smNewUserName: %s\n", indent, mNewUserName);
        writer.printf("%smNewGuestName: %s\n", indent, mNewGuestName);
    }
}
