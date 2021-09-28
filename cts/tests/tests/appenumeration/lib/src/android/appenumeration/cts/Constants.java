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

package android.appenumeration.cts;

public class Constants {
    public static final String PKG_BASE = "android.appenumeration.";

    /** A package that queries for {@link #TARGET_NO_API} package */
    public static final String QUERIES_PACKAGE = PKG_BASE + "queries.pkg";
    /** Queries for the unexported authority in {@link #TARGET_FILTERS} provider */
    public static final String QUERIES_UNEXPORTED_PROVIDER_AUTH =
            PKG_BASE + "queries.provider.authority.unexported";
    /** Queries for the unexported action in {@link #TARGET_FILTERS} provider */
    public static final String QUERIES_UNEXPORTED_PROVIDER_ACTION =
            PKG_BASE + "queries.provider.action.unexported";
    /** Queries for the unexported action in {@link #TARGET_FILTERS} service filter */
    public static final String QUERIES_UNEXPORTED_SERVICE_ACTION =
            PKG_BASE + "queries.service.action.unexported";
    /** Queries for the unexported action in {@link #TARGET_FILTERS} activity filter */
    public static final String QUERIES_UNEXPORTED_ACTIVITY_ACTION =
            PKG_BASE + "queries.activity.action.unexported";
    /** A package that queries for the authority in {@link #TARGET_FILTERS} provider */
    public static final String QUERIES_PROVIDER_AUTH = PKG_BASE + "queries.provider.authority";
    /** A package that queries for the authority in {@link #TARGET_FILTERS} provider */
    public static final String QUERIES_PROVIDER_ACTION = PKG_BASE + "queries.provider.action";
    /** A package that queries for the action in {@link #TARGET_FILTERS} service filter */
    public static final String QUERIES_SERVICE_ACTION = PKG_BASE + "queries.service.action";
    /** A package that queries for the action in {@link #TARGET_FILTERS} activity filter */
    public static final String QUERIES_ACTIVITY_ACTION = PKG_BASE + "queries.activity.action";
    /** A package that has no queries but gets the QUERY_ALL_PACKAGES permission */
    public static final String QUERIES_NOTHING_PERM = PKG_BASE + "queries.nothing.haspermission";
    /** A package that has no queries but has a provider that can be queried */
    public static final String QUERIES_NOTHING_PROVIDER = PKG_BASE + "queries.nothing.hasprovider";
    /** A package that has no queries tag or permissions but targets Q */
    public static final String QUERIES_NOTHING_Q = PKG_BASE + "queries.nothing.q";
    /** A package that has no queries tag or permission to query any specific packages */
    public static final String QUERIES_NOTHING = PKG_BASE + "queries.nothing";
    /** Another package that has no queries tag or permission to query any specific packages */
    public static final String QUERIES_NOTHING_RECEIVES_URI =
            PKG_BASE + "queries.nothing.receives.uri";
    public static final String QUERIES_NOTHING_RECEIVES_PERM_URI =
            PKG_BASE + "queries.nothing.receives.perm.uri";
    /** Another package that has no queries tag or permission to query any specific packages */
    public static final String QUERIES_NOTHING_SEES_INSTALLER =
            PKG_BASE + "queries.nothing.sees.installer";
    /** A package that queries nothing, but is part of a shared user */
    public static final String QUERIES_NOTHING_SHARED_USER = PKG_BASE + "queries.nothing.shareduid";
    /** A package that queries via wildcard action. */
    public static final String QUERIES_WILDCARD_ACTION = PKG_BASE + "queries.wildcard.action";
    /** A package that queries for all BROWSABLE intents. */
    public static final String QUERIES_WILDCARD_BROWSABLE = PKG_BASE + "queries.wildcard.browsable";
    /** A package that queries for all profile / contact targets. */
    public static final String QUERIES_WILDCARD_CONTACTS = PKG_BASE + "queries.wildcard.contacts";
    /** A package that queries for document viewer / editor targets. */
    public static final String QUERIES_WILDCARD_EDITOR = PKG_BASE + "queries.wildcard.editor";
    /** A package that queries for all jpeg share targets. */
    public static final String QUERIES_WILDCARD_SHARE = PKG_BASE + "queries.wildcard.share";
    /** A package that queries for all web intent browsable targets. */
    public static final String QUERIES_WILDCARD_WEB = PKG_BASE + "queries.wildcard.web";
    /** A package that queries for only browser intent targets. */
    public static final String QUERIES_WILDCARD_BROWSER = PKG_BASE + "queries.wildcard.browser";

    /** A package that queries for {@link #TARGET_NO_API} package */
    public static final String TARGET_SHARED_USER = PKG_BASE + "noapi.shareduid";
    /** A package that exposes itself via various intent filters (activities, services, etc.) */
    public static final String TARGET_FILTERS = PKG_BASE + "filters";
    /** A package that declares itself force queryable, making it visible to all other packages.
     *  This is installed as forceQueryable as non-system apps cannot declare themselves as such. */
    public static final String TARGET_FORCEQUERYABLE = PKG_BASE + "forcequeryable";
    /** A package that declares itself force queryable, but is installed normally making it not
     *  visible to other packages */
    public static final String TARGET_FORCEQUERYABLE_NORMAL =
            PKG_BASE + "forcequeryable.normalinstall";
    /** A package with no published API and so isn't queryable by anything but package name */
    public static final String TARGET_NO_API = PKG_BASE + "noapi";
    /** A package that offers an activity used for opening / editing file types */
    public static final String TARGET_EDITOR = PKG_BASE + "editor.activity";
    /** A package that offers an activity used viewing a contact / profile */
    public static final String TARGET_CONTACTS = PKG_BASE + "contacts.activity";
    /** A package that offers an content sharing activity */
    public static final String TARGET_SHARE = PKG_BASE + "share.activity";
    /** A package that offers an activity that handles browsable web intents for a specific host */
    public static final String TARGET_WEB = PKG_BASE + "web.activity";
    /** A package that offers an activity acts as a browser with host undefined */
    public static final String TARGET_BROWSER = PKG_BASE + "browser.activity";
    /** A package that offers an activity acts as a browser, but uses a wildcard for host */
    public static final String TARGET_BROWSER_WILDCARD = PKG_BASE + "browser.wildcard.activity";

    private static final String BASE_PATH = "/data/local/tmp/cts/appenumeration/";
    public static final String TARGET_NO_API_APK = BASE_PATH + "CtsAppEnumerationNoApi.apk";
    public static final String TARGET_FILTERS_APK = BASE_PATH + "CtsAppEnumerationFilters.apk";
    public static final String QUERIES_NOTHING_SEES_INSTALLER_APK =
            BASE_PATH + "CtsAppEnumerationQueriesNothingSeesInstaller.apk";

    public static final String[] ALL_QUERIES_TARGETING_R_PACKAGES = {
            QUERIES_NOTHING,
            QUERIES_NOTHING_PERM,
            QUERIES_ACTIVITY_ACTION,
            QUERIES_SERVICE_ACTION,
            QUERIES_PROVIDER_AUTH,
            QUERIES_UNEXPORTED_ACTIVITY_ACTION,
            QUERIES_UNEXPORTED_SERVICE_ACTION,
            QUERIES_UNEXPORTED_PROVIDER_AUTH,
            QUERIES_PACKAGE,
            QUERIES_NOTHING_SHARED_USER,
            QUERIES_WILDCARD_ACTION,
            QUERIES_WILDCARD_BROWSABLE,
            QUERIES_WILDCARD_CONTACTS,
            QUERIES_WILDCARD_EDITOR,
            QUERIES_WILDCARD_SHARE,
            QUERIES_WILDCARD_WEB,
    };

    public static final String ACTIVITY_CLASS_TEST = PKG_BASE + "cts.query.TestActivity";
    public static final String ACTIVITY_CLASS_DUMMY_ACTIVITY = PKG_BASE + "testapp.DummyActivity";

    public static final String ACTION_MANIFEST_ACTIVITY = PKG_BASE + "action.ACTIVITY";
    public static final String ACTION_MANIFEST_SERVICE = PKG_BASE + "action.SERVICE";
    public static final String ACTION_MANIFEST_PROVIDER = PKG_BASE + "action.PROVIDER";
    public static final String ACTION_SEND_RESULT = PKG_BASE + "cts.action.SEND_RESULT";
    public static final String ACTION_GET_PACKAGE_INFO = PKG_BASE + "cts.action.GET_PACKAGE_INFO";
    public static final String ACTION_START_FOR_RESULT = PKG_BASE + "cts.action.START_FOR_RESULT";
    public static final String ACTION_START_DIRECTLY = PKG_BASE + "cts.action.START_DIRECTLY";
    public static final String ACTION_JUST_FINISH = PKG_BASE + "cts.action.JUST_FINISH";
    public static final String ACTION_AWAIT_PACKAGE_REMOVED =
            PKG_BASE + "cts.action.AWAIT_PACKAGE_REMOVED";
    public static final String ACTION_AWAIT_PACKAGE_ADDED =
            PKG_BASE + "cts.action.AWAIT_PACKAGE_ADDED";

    public static final String ACTION_QUERY_ACTIVITIES =
            PKG_BASE + "cts.action.QUERY_INTENT_ACTIVITIES";
    public static final String ACTION_QUERY_SERVICES =
            PKG_BASE + "cts.action.QUERY_INTENT_SERVICES";
    public static final String ACTION_QUERY_PROVIDERS =
            PKG_BASE + "cts.action.QUERY_INTENT_PROVIDERS";
    public static final String ACTION_GET_INSTALLED_PACKAGES =
            PKG_BASE + "cts.action.GET_INSTALLED_PACKAGES";
    public static final String ACTION_START_SENDER_FOR_RESULT =
            PKG_BASE + "cts.action.START_SENDER_FOR_RESULT";
    public static final String ACTION_QUERY_RESOLVER =
            PKG_BASE + "cts.action.QUERY_RESOLVER_FOR_VISIBILITY";

    public static final String EXTRA_REMOTE_CALLBACK = "remoteCallback";
    public static final String EXTRA_ERROR = "error";
    public static final String EXTRA_FLAGS = "flags";
    public static final String EXTRA_DATA = "data";
    public static final String EXTRA_AUTHORITY = "authority";
}
