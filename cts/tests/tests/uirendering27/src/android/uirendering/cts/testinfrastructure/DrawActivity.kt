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
package android.uirendering.cts.testinfrastructure

import android.app.Activity
import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.graphics.Point
import android.os.Bundle
import android.os.Handler
import android.os.Message
import android.uirendering.cts27.R
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.Nullable
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

class Offset(val dx: Float, val dy: Float) {
    override fun equals(other: Any?): Boolean {
        return this === other || (other as? Offset)?.let {
            dx == other.dx && dy == other.dy
        } ?: false
    }

    override fun hashCode(): Int = dx.hashCode() xor dy.hashCode()
}

/**
 * A generic activity that uses a view specified by the user.
 */
class DrawActivity : Activity() {
    companion object {
        internal const val EXTRA_WIDE_COLOR_GAMUT = "DrawActivity.WIDE_COLOR_GAMUT"
        internal const val EXTRA_USE_FORCE_DARK = "DrawActivity.USE_FORCE_DARK"

        private const val TIME_OUT_MS: Long = 10000

        private const val LAYOUT_MSG = 1
        private const val CANVAS_MSG = 2
    }

    private val mLock = java.lang.Object()
    private var mPositionInfo: ActivityTestBase.TestPositionInfo? = null

    private lateinit var mHandler: Handler
    private lateinit var mTestContainer: ViewGroup

    private var mView: View? = null

    private var mViewInitializer: ViewInitializer? = null

    public override fun onCreate(bundle: Bundle?) {
        super.onCreate(bundle)

        if (intent.getBooleanExtra(EXTRA_USE_FORCE_DARK, false)) {
            forceUiMode(Configuration.UI_MODE_NIGHT_YES)
            setTheme(R.style.AutoDarkTheme)
        } else {
            forceUiMode(Configuration.UI_MODE_NIGHT_NO)
        }

        if (intent.getBooleanExtra(EXTRA_WIDE_COLOR_GAMUT, false)) {
            window.colorMode = ActivityInfo.COLOR_MODE_WIDE_COLOR_GAMUT
        }

        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_FULLSCREEN
        mHandler = RenderSpecHandler()

        setContentView(R.layout.test_container)
        mTestContainer = findViewById(R.id.test_content_wrapper)
    }

    private fun forceUiMode(mode: Int) {
        if (resources.configuration.uiMode != mode) {
            val newConfig = Configuration(resources.configuration)
            newConfig.uiMode = mode
            resources.updateConfiguration(newConfig, resources.displayMetrics)
            onConfigurationChanged(newConfig)
        }
    }

    fun enqueueRenderSpecAndWait(
        layoutId: Int,
        canvasClient: CanvasClient?,
        @Nullable viewInitializer: ViewInitializer?,
        useHardware: Boolean,
        usePicture: Boolean
    ): ActivityTestBase.TestPositionInfo {
        (mHandler as RenderSpecHandler).setViewInitializer(viewInitializer)
        val arg2 = if (useHardware) View.LAYER_TYPE_NONE else View.LAYER_TYPE_SOFTWARE
        synchronized(mLock) {
            if (canvasClient != null) {
                mHandler.obtainMessage(CANVAS_MSG, if (usePicture) 1 else 0,
                    arg2, canvasClient
                ).sendToTarget()
            } else {
                mHandler.obtainMessage(LAYOUT_MSG, layoutId, arg2)
                    .sendToTarget()
            }

            try {
                mLock.wait(TIME_OUT_MS)
            } catch (e: InterruptedException) {
                throw AssertionError(e)
            }
        }
        assertNotNull("Timeout waiting for draw", mPositionInfo)
        return mPositionInfo!!
    }

    fun waitForRedraw() {
        synchronized(mLock) {
            mHandler.post {
                mTestContainer.invalidate()
                notifyOnDrawCompleted()
            }
            try {
                mLock.wait(TIME_OUT_MS)
            } catch (e: InterruptedException) {
                throw AssertionError(e)
            }
        }
    }

    private fun notifyOnDrawCompleted() {
        mTestContainer.viewTreeObserver.registerFrameCommitCallback {
            val location = IntArray(2)
            mTestContainer.getLocationInWindow(location)
            val surfaceOffset = Point(location[0], location[1])
            mTestContainer.getLocationOnScreen(location)
            val screenOffset = Point(location[0], location[1])
            synchronized(mLock) {
                mPositionInfo = ActivityTestBase.TestPositionInfo(
                    surfaceOffset, screenOffset
                )
                mLock.notify()
            }
        }
    }

    fun reset() {
        val fence = CountDownLatch(1)
        mHandler.post {
            mViewInitializer?.teardownView()
            mViewInitializer = null
            mView = null
            mTestContainer.removeAllViews()
            fence.countDown()
        }
        assertTrue(fence.await(TIME_OUT_MS, TimeUnit.MILLISECONDS))
    }

    private inner class RenderSpecHandler : Handler() {

        fun setViewInitializer(viewInitializer: ViewInitializer?) {
            mViewInitializer = viewInitializer
        }

        override fun handleMessage(message: Message) {
            mTestContainer.removeAllViews()
            when (message.what) {
                LAYOUT_MSG -> {
                    mView = LayoutInflater.from(this@DrawActivity).inflate(
                        message.arg1, mTestContainer, false
                    )
                }

                CANVAS_MSG -> {
                    val canvasClientView = CanvasClientView(this@DrawActivity)
                    canvasClientView.setCanvasClient(message.obj as CanvasClient)
                    if (message.arg1 != 0) {
                        canvasClientView.setUsePicture(true)
                    }
                    mView = canvasClientView
                }
            }

            if (mView == null) {
                throw IllegalStateException("failed to inflate test content")
            }

            mTestContainer.addView(
                mView, ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )

            if (mViewInitializer != null) {
                mViewInitializer!!.initializeView(mView)
            }

            // set layer on wrapper parent of view, so view initializer
            // can control layer type of View under test.
            mTestContainer.setLayerType(message.arg2, null)

            notifyOnDrawCompleted()
        }
    }

    override fun onPause() {
        super.onPause()
        mViewInitializer?.run {
            teardownView()
        }
    }

    override fun finish() {
        // Ignore
    }

    /** Call this when all the tests that use this activity have completed.
     * This will then clean up any internal state and finish the activity.  */
    fun allTestsFinished() {
        super.finish()
    }
}
