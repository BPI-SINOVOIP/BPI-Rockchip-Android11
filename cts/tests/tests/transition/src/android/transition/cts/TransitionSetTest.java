/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.transition.cts;

import static com.android.compatibility.common.util.CtsMockitoUtils.within;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.TimeInterpolator;
import android.graphics.Rect;
import android.transition.ArcMotion;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.PathMotion;
import android.transition.Transition;
import android.transition.TransitionPropagation;
import android.transition.TransitionSet;
import android.transition.TransitionValues;
import android.view.ViewGroup;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.DecelerateInterpolator;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class TransitionSetTest extends BaseTransitionTest {
    @Test
    public void testTransitionTogether() throws Throwable {
        TransitionSet transitionSet = new TransitionSet();
        Fade fade = new Fade();
        final Transition.TransitionListener fadeListener =
                mock(Transition.TransitionListener.class);
        fade.addListener(fadeListener);
        ChangeBounds changeBounds = new ChangeBounds();
        final Transition.TransitionListener changeBoundsListener =
                mock(Transition.TransitionListener.class);
        changeBounds.addListener(changeBoundsListener);
        transitionSet.addTransition(fade);
        transitionSet.addTransition(changeBounds);
        mTransition = transitionSet;
        resetListener();

        assertEquals(TransitionSet.ORDERING_TOGETHER, transitionSet.getOrdering());
        enterScene(R.layout.scene1);
        startTransition(R.layout.scene3);
        mActivityRule.runOnUiThread(() -> {
            verify(fadeListener, times(1)).onTransitionStart(any());
            verify(changeBoundsListener, times(1)).onTransitionStart(any());
        });
    }

    @Test
    public void testTransitionSequentially() throws Throwable {
        TransitionSet transitionSet = new TransitionSet();
        Fade fade = new Fade();
        fade.setDuration(500);
        final Transition.TransitionListener fadeListener =
                mock(Transition.TransitionListener.class);
        fade.addListener(fadeListener);
        ChangeBounds changeBounds = new ChangeBounds();
        final Transition.TransitionListener changeBoundsListener =
                mock(Transition.TransitionListener.class);
        changeBounds.addListener(changeBoundsListener);
        transitionSet.addTransition(fade);
        transitionSet.addTransition(changeBounds);
        mTransition = transitionSet;
        resetListener();

        assertEquals(TransitionSet.ORDERING_TOGETHER, transitionSet.getOrdering());
        transitionSet.setOrdering(TransitionSet.ORDERING_SEQUENTIAL);
        assertEquals(TransitionSet.ORDERING_SEQUENTIAL, transitionSet.getOrdering());

        enterScene(R.layout.scene1);
        startTransition(R.layout.scene3);
        verify(fadeListener, within(500)).onTransitionStart(any());
        verify(fadeListener, times(1)).onTransitionStart(any());

        // change bounds shouldn't start until after fade finishes
        verify(fadeListener, times(0)).onTransitionEnd(any());
        verify(changeBoundsListener, times(0)).onTransitionStart(any());

        // now wait for the fade transition to end
        verify(fadeListener, within(1000)).onTransitionEnd(any());

        // The change bounds should start soon after
        verify(changeBoundsListener, within(500)).onTransitionStart(any());
        verify(changeBoundsListener, times(1)).onTransitionStart(any());
    }

    @Test
    public void testTransitionCount() throws Throwable {
        TransitionSet transitionSet = new TransitionSet();
        assertEquals(0, transitionSet.getTransitionCount());

        Fade fade = new Fade();
        ChangeBounds changeBounds = new ChangeBounds();
        transitionSet.addTransition(fade);
        transitionSet.addTransition(changeBounds);

        assertEquals(2, transitionSet.getTransitionCount());
        assertSame(fade, transitionSet.getTransitionAt(0));
        assertSame(changeBounds, transitionSet.getTransitionAt(1));

        transitionSet.removeTransition(fade);

        assertEquals(1, transitionSet.getTransitionCount());
        assertSame(changeBounds, transitionSet.getTransitionAt(0));

        transitionSet.removeTransition(fade); // remove one that isn't there
        assertEquals(1, transitionSet.getTransitionCount());
        assertSame(changeBounds, transitionSet.getTransitionAt(0));
    }

    @Test
    public void testSetTransferValuesDuringAdd() throws Throwable {
        Fade fade = new Fade();
        fade.setDuration(500);
        fade.setPropagation(new TestPropagation());
        fade.setEpicenterCallback(new Transition.EpicenterCallback() {
            @Override
            public Rect onGetEpicenter(Transition transition) {
                return null;
            }
        });
        fade.setInterpolator(new AccelerateDecelerateInterpolator());
        fade.setPathMotion(new ArcMotion());

        TransitionSet transitionSet = new TransitionSet();
        int duration = 100;
        TestPropagation propagation = new TestPropagation();
        TimeInterpolator interpolator = new DecelerateInterpolator();
        PathMotion pathMotion = new ArcMotion();
        Transition.EpicenterCallback epicenterCallback = new Transition.EpicenterCallback() {
            @Override
            public Rect onGetEpicenter(Transition transition) {
                return null;
            }
        };
        transitionSet.setDuration(duration);
        transitionSet.setPropagation(propagation);
        transitionSet.setInterpolator(interpolator);
        transitionSet.setPathMotion(pathMotion);
        transitionSet.setEpicenterCallback(epicenterCallback);

        transitionSet.addTransition(fade);
        assertEquals(duration, fade.getDuration());
        assertSame(propagation, fade.getPropagation());
        assertSame(interpolator, fade.getInterpolator());
        assertSame(pathMotion, fade.getPathMotion());
        assertSame(epicenterCallback, fade.getEpicenterCallback());
    }

    @Test
    public void testSetTransferNullValuesDuringAdd() throws Throwable {
        Fade fade = new Fade();
        fade.setDuration(500);
        fade.setPropagation(new TestPropagation());
        fade.setEpicenterCallback(new Transition.EpicenterCallback() {
            @Override
            public Rect onGetEpicenter(Transition transition) {
                return null;
            }
        });
        fade.setInterpolator(new AccelerateDecelerateInterpolator());
        fade.setPathMotion(new ArcMotion());

        TransitionSet transitionSet = new TransitionSet();
        transitionSet.setDuration(0);
        transitionSet.setPropagation(null);
        transitionSet.setInterpolator(null);
        transitionSet.setPathMotion(null);
        transitionSet.setEpicenterCallback(null);

        transitionSet.addTransition(fade);
        assertEquals(0, fade.getDuration());
        assertNull(fade.getPropagation());
        assertNull(fade.getInterpolator());
        assertSame(transitionSet.getPathMotion(), fade.getPathMotion());
        assertNull(fade.getEpicenterCallback());
    }

    @Test
    public void testSetNoTransferValuesDuringAdd() throws Throwable {
        Fade fade = new Fade();
        int duration = 100;
        TestPropagation propagation = new TestPropagation();
        TimeInterpolator interpolator = new DecelerateInterpolator();
        PathMotion pathMotion = new ArcMotion();
        Transition.EpicenterCallback epicenterCallback = new Transition.EpicenterCallback() {
            @Override
            public Rect onGetEpicenter(Transition transition) {
                return null;
            }
        };
        fade.setDuration(duration);
        fade.setPropagation(propagation);
        fade.setInterpolator(interpolator);
        fade.setPathMotion(pathMotion);
        fade.setEpicenterCallback(epicenterCallback);

        TransitionSet transitionSet = new TransitionSet();

        transitionSet.addTransition(fade);
        assertEquals(duration, fade.getDuration());
        assertSame(propagation, fade.getPropagation());
        assertSame(interpolator, fade.getInterpolator());
        assertSame(pathMotion, fade.getPathMotion());
        assertSame(epicenterCallback, fade.getEpicenterCallback());
    }

    @Test
    public void testSetAllowsChildrenToOverrideConfigs() {
        TransitionSet transitionSet = new TransitionSet();
        transitionSet.setDuration(100);
        transitionSet.setInterpolator(new DecelerateInterpolator());

        Fade fade = new Fade();
        transitionSet.addTransition(fade); // here set's duration and interpolator are applied

        int overriddenDuration = 200;
        TimeInterpolator overriddenInterpolator = new AccelerateInterpolator();
        fade.setDuration(overriddenDuration);
        fade.setInterpolator(overriddenInterpolator);

        // emulate beginDelayedTransition where we clone the provided transition
        transitionSet = (TransitionSet) transitionSet.clone();
        fade = (Fade) transitionSet.getTransitionAt(0);

        assertEquals(overriddenDuration, fade.getDuration());
        assertEquals(overriddenInterpolator, fade.getInterpolator());
    }

    private static class TestPropagation extends TransitionPropagation {
        @Override
        public long getStartDelay(ViewGroup sceneRoot, Transition transition,
                TransitionValues startValues, TransitionValues endValues) {
            return 0;
        }

        @Override
        public void captureValues(TransitionValues transitionValues) {
        }

        @Override
        public String[] getPropagationProperties() {
            return new String[] { };
        }
    }
}

