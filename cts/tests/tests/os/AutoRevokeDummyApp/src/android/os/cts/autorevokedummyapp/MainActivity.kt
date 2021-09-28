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

package android.os.cts.autorevokedummyapp

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.LinearLayout.VERTICAL
import android.widget.TextView

class MainActivity : Activity() {

    val whitelistStatus by lazy { TextView(this) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(LinearLayout(this).apply {
            orientation = VERTICAL

            addView(whitelistStatus)
            addView(Button(this@MainActivity).apply {
                text = "Request whitelist"

                setOnClickListener {
                    startActivity(
                        Intent(Intent.ACTION_AUTO_REVOKE_PERMISSIONS)
                            .setData(Uri.fromParts("package", packageName, null)))
                }
            })
        })

        requestPermissions(arrayOf("android.permission.READ_CALENDAR"), 0)
    }

    override fun onResume() {
        super.onResume()

        whitelistStatus.text = "Auto-revoke whitelisted: " + packageManager.isAutoRevokeWhitelisted
    }
}
