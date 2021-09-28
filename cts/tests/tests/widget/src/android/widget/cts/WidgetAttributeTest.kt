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
 * limitations under the License
 */

package android.widget.cts

import android.app.Activity
import android.support.test.uiautomator.UiDevice
import android.view.LayoutInflater
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.Switch
import android.widget.TextView
import android.widget.Toolbar
import android.widget.ViewAnimator
import androidx.test.InstrumentationRegistry
import androidx.test.filters.MediumTest
import androidx.test.rule.ActivityTestRule
import androidx.test.runner.AndroidJUnit4
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@MediumTest
@RunWith(AndroidJUnit4::class)
class WidgetAttributeTest {
    companion object {
        const val DISABLE_SHELL_COMMAND =
                "settings delete global debug_view_attributes_application_package"
        const val ENABLE_SHELL_COMMAND =
                "settings put global debug_view_attributes_application_package android.widget.cts"
    }

    @get:Rule
    val activityRule = ActivityTestRule<Activity>(Activity::class.java, true, false)
    private lateinit var uiDevice: UiDevice

    @Before
    fun setUp() {
        uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        uiDevice.executeShellCommand(ENABLE_SHELL_COMMAND)
        activityRule.launchActivity(null)
    }

    @After
    fun tearDown() {
        uiDevice.executeShellCommand(DISABLE_SHELL_COMMAND)
    }

    @Test
    fun testGetAttributeResolutionStack() {
        val inflater = LayoutInflater.from(activityRule.getActivity())
        val rootView = inflater.inflate(R.layout.widget_attribute_layout, null) as LinearLayout

        // Toolbar that has the default style MyToolbarStyle set in WidgetStyleTestTheme Activity
        // theme via android:toolbarStyle
        val toolbar1 = rootView.findViewById<Toolbar>(R.id.toolbar_view1)
        val stackToolbar1padding =
                toolbar1.getAttributeResolutionStack(android.R.attr.padding)
        assertEquals(3, stackToolbar1padding.size.toLong())
        assertEquals(R.layout.widget_attribute_layout, stackToolbar1padding[0])
        assertEquals(R.style.MyToolbarStyle, stackToolbar1padding[1])
        assertEquals(R.style.MyToolbarStyleParent, stackToolbar1padding[2])
        val stackToolbar1titleMarginEnd =
                toolbar1.getAttributeResolutionStack(android.R.attr.titleMarginEnd)
        assertEquals(3, stackToolbar1titleMarginEnd.size.toLong())
        assertEquals(R.layout.widget_attribute_layout, stackToolbar1titleMarginEnd[0])
        assertEquals(R.style.MyToolbarStyle, stackToolbar1titleMarginEnd[1])
        assertEquals(R.style.MyToolbarStyleParent, stackToolbar1titleMarginEnd[2])

        // Toolbar that has the default style MyToolbarStyle set in WidgetStyleTestTheme Activity
        // theme via android:toolbarStyle has an explicit style ExplicitStyle1 set via style = ...
        val toolbar2 = rootView.findViewById<Toolbar>(R.id.toolbar_view2)
        val stackToolbar2padding =
                toolbar2.getAttributeResolutionStack(android.R.attr.padding)
        assertEquals(5, stackToolbar2padding.size.toLong())
        assertEquals(R.layout.widget_attribute_layout, stackToolbar2padding[0])
        assertEquals(R.style.ExplicitStyle1, stackToolbar2padding[1])
        assertEquals(R.style.ParentOfExplicitStyle1, stackToolbar2padding[2])
        assertEquals(R.style.MyToolbarStyle, stackToolbar2padding[3])
        assertEquals(R.style.MyToolbarStyleParent, stackToolbar2padding[4])
        val stackToobar2titleMarginEnd =
                toolbar2.getAttributeResolutionStack(android.R.attr.titleMarginEnd)
        assertEquals(5, stackToobar2titleMarginEnd.size.toLong())
        assertEquals(R.layout.widget_attribute_layout, stackToobar2titleMarginEnd[0])
        assertEquals(R.style.ExplicitStyle1, stackToobar2titleMarginEnd[1])
        assertEquals(R.style.ParentOfExplicitStyle1, stackToobar2titleMarginEnd[2])
        assertEquals(R.style.MyToolbarStyle, stackToobar2titleMarginEnd[3])
        assertEquals(R.style.MyToolbarStyleParent, stackToobar2titleMarginEnd[4])

        val textview1 = rootView.findViewById<TextView>(R.id.textview1)
        val stackTextView1textColor =
                textview1.getAttributeResolutionStack(android.R.attr.textColor)
        assertEquals(0, stackTextView1textColor.size.toLong())
        val stackTextView1textSize =
                textview1.getAttributeResolutionStack(android.R.attr.textSize)
        assertEquals(4, stackTextView1textSize.size.toLong())
        assertEquals(R.layout.widget_attribute_layout, stackTextView1textSize[0])
        assertEquals(R.style.ExplicitStyle1, stackTextView1textSize[1])
        assertEquals(R.style.ParentOfExplicitStyle1, stackTextView1textSize[2])
        assertEquals(R.style.TextViewWithoutColorAndAppearance, stackTextView1textSize[3])

        val viewAnimator = rootView.findViewById<ViewAnimator>(R.id.viewAnimator)
        val viewAnimatorOutAnimation =
                viewAnimator.getAttributeResolutionStack(android.R.attr.outAnimation)
        assertEquals(1, viewAnimatorOutAnimation.size.toLong())
        assertEquals(R.layout.widget_attribute_layout, viewAnimatorOutAnimation[0])
    }

    @Test
    fun testGetAttributeSourceResourceMap() {
        val inflater = LayoutInflater.from(activityRule.getActivity())
        val rootView = inflater.inflate(R.layout.widget_attribute_layout, null) as LinearLayout

        // ProgressBar that has an explicit style ExplicitStyle1 set via style = ...
        val progressBar = rootView.findViewById<ProgressBar>(R.id.progress_bar)
        val attributeMapProgressBar = progressBar!!.attributeSourceResourceMap
        assertEquals(R.layout.widget_attribute_layout,
                attributeMapProgressBar[android.R.attr.minWidth]!!.toInt())
        assertEquals(R.layout.widget_attribute_layout,
                attributeMapProgressBar[android.R.attr.maxWidth]!!.toInt())
        assertEquals(R.layout.widget_attribute_layout,
                attributeMapProgressBar[android.R.attr.progressTint]!!.toInt())
        assertEquals(R.style.ExplicitStyle1,
                attributeMapProgressBar[android.R.attr.progress]!!.toInt())
        assertEquals(R.style.ExplicitStyle1,
                attributeMapProgressBar[android.R.attr.padding]!!.toInt())
        assertEquals(R.style.ExplicitStyle1,
                attributeMapProgressBar[android.R.attr.max]!!.toInt())
        assertEquals(R.style.ParentOfExplicitStyle1,
                attributeMapProgressBar[android.R.attr.mirrorForRtl]!!.toInt())

        // Switch that has an explicit style ExplicitStyle2 set via style = ...
        val switch = rootView.findViewById<Switch>(R.id.switch_view)
        val attributeMapSwitch = switch!!.attributeSourceResourceMap
        assertEquals(R.layout.widget_attribute_layout,
                attributeMapSwitch[android.R.attr.switchPadding]!!.toInt())

        // Toolbar that has MyToolbarStyle set via the theme android:toolbarStyle = ...
        val toolbar = rootView.findViewById<Toolbar>(R.id.toolbar_view1)
        val attributeMapToobar = toolbar!!.attributeSourceResourceMap
        assertEquals(R.style.MyToolbarStyle,
                attributeMapToobar[android.R.attr.titleMarginEnd]!!.toInt())
        assertEquals(R.style.MyToolbarStyleParent,
                attributeMapToobar[android.R.attr.titleMarginStart]!!.toInt())

        val textview1 = rootView.findViewById<TextView>(R.id.textview1)
        val attributeMapTextView1 = textview1!!.attributeSourceResourceMap
        assertEquals(R.style.TextViewWithoutColorAndAppearance,
                attributeMapTextView1[android.R.attr.textSize]!!.toInt())
        assertEquals(R.style.ExplicitStyle1,
                attributeMapTextView1[android.R.attr.textColorHighlight]!!.toInt())
    }
}