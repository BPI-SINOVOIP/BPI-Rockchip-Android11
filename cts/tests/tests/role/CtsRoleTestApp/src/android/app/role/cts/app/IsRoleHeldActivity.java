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

package android.app.role.cts.app;

import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

/**
 * An activity that checks whether a role is held.
 */
public class IsRoleHeldActivity extends Activity {

    private static final String EXTRA_IS_ROLE_HELD = "android.app.role.cts.app.extra.IS_ROLE_HELD";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String roleName = getIntent().getStringExtra(Intent.EXTRA_ROLE_NAME);
        if (TextUtils.isEmpty(roleName)) {
            throw new IllegalArgumentException("Role name in extras cannot be null or empty");
        }

        RoleManager roleManager = getSystemService(RoleManager.class);
        setResult(RESULT_OK, new Intent()
                .putExtra(EXTRA_IS_ROLE_HELD, roleManager.isRoleHeld(roleName)));
        finish();
    }
}
