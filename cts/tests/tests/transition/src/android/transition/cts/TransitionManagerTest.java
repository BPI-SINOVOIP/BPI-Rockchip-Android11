/*
 * Copyright (C) 2015 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Rect;
import android.os.SystemClock;
import android.transition.Scene;
import android.transition.Transition;
import android.transition.TransitionManager;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class TransitionManagerTest extends BaseTransitionTest {
    @Test
    public void testBeginDelayedTransition() throws Throwable {
        mActivityRule.runOnUiThread(() -> {
            TransitionManager.beginDelayedTransition(mSceneRoot, mTransition);
            View view = mActivity.getLayoutInflater().inflate(R.layout.scene1, mSceneRoot,
                    false);
            mSceneRoot.addView(view);
        });

        waitForStart();
        waitForEnd(800);
        verify(mListener, never()).onTransitionResume(any());
        verify(mListener, never()).onTransitionPause(any());
        verify(mListener, never()).onTransitionCancel(any());
        ArgumentCaptor<Transition> transitionArgumentCaptor =
                ArgumentCaptor.forClass(Transition.class);
        verify(mListener, times(1)).onTransitionStart(transitionArgumentCaptor.capture());
        assertEquals(TestTransition.class, transitionArgumentCaptor.getValue().getClass());
        assertTrue(mTransition != transitionArgumentCaptor.getValue());
        mActivityRule.runOnUiThread(() -> {
            assertNotNull(mActivity.findViewById(R.id.redSquare));
            assertNotNull(mActivity.findViewById(R.id.greenSquare));
        });
    }

    @Test
    public void testDefaultBeginDelayedTransition() throws Throwable {
        enterScene(R.layout.scene1);
        final CountDownLatch startLatch = new CountDownLatch(1);
        final Scene scene6 = loadScene(R.layout.scene6);

        final CountDownLatch moveLatch = watchForRedSquareMoving();
        mActivityRule.runOnUiThread(() -> {
            mSceneRoot.getViewTreeObserver().addOnPreDrawListener(
                    new ViewTreeObserver.OnPreDrawListener() {
                        @Override
                        public boolean onPreDraw() {
                            mSceneRoot.getViewTreeObserver().removeOnPreDrawListener(this);
                            startLatch.countDown();
                            return true;
                        }
                    });
            TransitionManager.beginDelayedTransition(mSceneRoot);
            scene6.enter();
        });
        assertTrue(startLatch.await(500, TimeUnit.MILLISECONDS));
        assertTrue(moveLatch.await(1000, TimeUnit.MILLISECONDS));
        endTransition();
    }

    private CountDownLatch watchForRedSquareMoving() throws Throwable {
        final CountDownLatch latch = new CountDownLatch(1);

        mActivityRule.runOnUiThread(() -> {
            final View decor = mActivity.getWindow().getDecorView();
            final View viewBeforeTransition = mActivity.findViewById(R.id.redSquare);
            decor.getViewTreeObserver().addOnPreDrawListener(
                    new ViewTreeObserver.OnPreDrawListener() {
                        // Wait at most 20 frames for the position to change
                        int mFramesToChange = 20;
                        Rect mLastPosition = null;
                        View mView = null;
                        int mPositionChanges = 0;

                        @Override
                        public boolean onPreDraw() {
                            final View view = mActivity.findViewById(R.id.redSquare);
                            if (mView == null && view != viewBeforeTransition) {
                                // Capture the new red square View. It will be in the end
                                // position now, so don't capture its position.
                                mView = view;
                                return true;
                            }
                            Rect nextPosition = new Rect(view.getLeft(), view.getTop(),
                                    view.getRight(), view.getBottom());
                            if (mLastPosition == null) {
                                // This is the start position
                                mLastPosition = nextPosition;
                                return true;
                            }
                            mFramesToChange--;
                            if (!nextPosition.equals(mLastPosition)) {
                                mPositionChanges++;
                                mLastPosition = nextPosition;
                                if (mPositionChanges > 2) {
                                    latch.countDown();
                                }
                            }
                            if (mFramesToChange <= 0 || mPositionChanges > 2) {
                                decor.getViewTreeObserver().removeOnPreDrawListener(this);
                            }
                            return true;
                        }
                    });
        });
        return latch;
    }

    @Test
    public void testGo() throws Throwable {
        startTransition(R.layout.scene1);
        waitForStart();
        waitForEnd(800);

        verify(mListener, never()).onTransitionResume(any());
        verify(mListener, never()).onTransitionPause(any());
        verify(mListener, never()).onTransitionCancel(any());
        ArgumentCaptor<Transition> transitionArgumentCaptor =
                ArgumentCaptor.forClass(Transition.class);
        verify(mListener, times(1)).onTransitionStart(transitionArgumentCaptor.capture());
        assertEquals(TestTransition.class, transitionArgumentCaptor.getValue().getClass());
        assertTrue(mTransition != transitionArgumentCaptor.getValue());
        mActivityRule.runOnUiThread(() -> {
            assertNotNull(mActivity.findViewById(R.id.redSquare));
            assertNotNull(mActivity.findViewById(R.id.greenSquare));
        });
    }

    @Test
    public void testDefaultGo() throws Throwable {
        enterScene(R.layout.scene1);
        final CountDownLatch startLatch = new CountDownLatch(1);
        final Scene scene6 = loadScene(R.layout.scene6);
        final CountDownLatch moveLatch = watchForRedSquareMoving();
        mActivityRule.runOnUiThread(() -> {
            mSceneRoot.getViewTreeObserver().addOnPreDrawListener(
                    new ViewTreeObserver.OnPreDrawListener() {
                        @Override
                        public boolean onPreDraw() {
                            mSceneRoot.getViewTreeObserver().removeOnPreDrawListener(this);
                            startLatch.countDown();
                            return true;
                        }
                    });
            TransitionManager.go(scene6);
        });
        assertTrue(startLatch.await(500, TimeUnit.MILLISECONDS));
        assertTrue(moveLatch.await(1000, TimeUnit.MILLISECONDS));
        endTransition();
    }

    @Test
    public void testSetTransition1() throws Throwable {
        final TransitionManager transitionManager = new TransitionManager();

        mActivityRule.runOnUiThread(() -> {
            Scene scene = Scene.getSceneForLayout(mSceneRoot, R.layout.scene1, mActivity);
            transitionManager.setTransition(scene, mTransition);
            transitionManager.transitionTo(scene);
        });

        waitForStart();
        waitForEnd(800);
        verify(mListener, never()).onTransitionResume(any());
        verify(mListener, never()).onTransitionPause(any());
        verify(mListener, never()).onTransitionCancel(any());
        ArgumentCaptor<Transition> transitionArgumentCaptor =
                ArgumentCaptor.forClass(Transition.class);
        verify(mListener, times(1)).onTransitionStart(transitionArgumentCaptor.capture());
        assertEquals(TestTransition.class, transitionArgumentCaptor.getValue().getClass());
        assertTrue(mTransition != transitionArgumentCaptor.getValue());
        mActivityRule.runOnUiThread(() -> {
            reset(mListener);
            assertNotNull(mActivity.findViewById(R.id.redSquare));
            assertNotNull(mActivity.findViewById(R.id.greenSquare));
            Scene scene = Scene.getSceneForLayout(mSceneRoot, R.layout.scene2, mActivity);
            transitionManager.transitionTo(scene);
        });
        SystemClock.sleep(50);
        verify(mListener, never()).onTransitionStart(any());
        endTransition();
    }

    @Test
    public void testSetTransition2() throws Throwable {
        final TransitionManager transitionManager = new TransitionManager();
        final Scene[] scenes = new Scene[3];

        mActivityRule.runOnUiThread(() -> {
            scenes[0] = Scene.getSceneForLayout(mSceneRoot, R.layout.scene1, mActivity);
            scenes[1] = Scene.getSceneForLayout(mSceneRoot, R.layout.scene2, mActivity);
            scenes[2] = Scene.getSceneForLayout(mSceneRoot, R.layout.scene3, mActivity);
            transitionManager.setTransition(scenes[0], scenes[1], mTransition);
            transitionManager.transitionTo(scenes[0]);
        });
        SystemClock.sleep(100);
        verify(mListener, never()).onTransitionStart(any());

        mActivityRule.runOnUiThread(() -> transitionManager.transitionTo(scenes[1]));

        waitForStart();
        waitForEnd(800);
        verify(mListener, never()).onTransitionResume(any());
        verify(mListener, never()).onTransitionPause(any());
        verify(mListener, never()).onTransitionCancel(any());
        ArgumentCaptor<Transition> transitionArgumentCaptor =
                ArgumentCaptor.forClass(Transition.class);
        verify(mListener, times(1)).onTransitionStart(transitionArgumentCaptor.capture());
        assertEquals(TestTransition.class, transitionArgumentCaptor.getValue().getClass());
        assertTrue(mTransition != transitionArgumentCaptor.getValue());
        mActivityRule.runOnUiThread(() -> {
            reset(mListener);
            transitionManager.transitionTo(scenes[2]);
        });
        SystemClock.sleep(50);
        verify(mListener, never()).onTransitionStart(any());
        endTransition();
    }

    @Test
    public void testEndTransitions() throws Throwable {
        mTransition.setDuration(400);

        startTransition(R.layout.scene1);
        waitForStart();
        endTransition();
        waitForEnd(400);
    }

    @Test
    public void testEndTransitionsBeforeStarted() throws Throwable {
        mTransition.setDuration(400);

        mActivityRule.runOnUiThread(() -> {
            Scene scene = Scene.getSceneForLayout(mSceneRoot, R.layout.scene1, mActivity);
            TransitionManager.go(scene, mTransition);
            TransitionManager.endTransitions(mSceneRoot);
        });
        SystemClock.sleep(100);
        verify(mListener, never()).onTransitionStart(any());
        SystemClock.sleep(10);
        verify(mListener, never()).onTransitionEnd(any());
    }

    @Test
    public void testGo_enterAction() throws Throwable {
        Scene scene = loadScene(R.layout.scene1);
        Runnable enterCheck = mock(Runnable.class);
        scene.setEnterAction(enterCheck);

        mActivityRule.runOnUiThread(() -> {
            TransitionManager.go(scene);
            verify(enterCheck, times(1)).run();
        });
    }

    @Test
    public void testGo_exitAction() throws Throwable {
        Scene scene1 = loadScene(R.layout.scene1);
        Runnable exitAction = mock(Runnable.class);
        scene1.setExitAction(exitAction);

        mActivityRule.runOnUiThread(() -> {
            TransitionManager.go(scene1);
            verify(exitAction, never()).run();
            clearInvocations(exitAction);
        });

        Scene scene2 = loadScene(R.layout.scene2);

        mActivityRule.runOnUiThread(() -> {
            TransitionManager.go(scene2);
            verify(exitAction, times(1)).run();
        });
    }

    @Test
    public void testGo_nullParameter_enterAction() throws Throwable {
        Scene scene = loadScene(R.layout.scene1);
        Runnable enterCheck = mock(Runnable.class);
        scene.setEnterAction(enterCheck);


        mActivityRule.runOnUiThread(() -> {
            TransitionManager.go(scene, null);
            verify(enterCheck, times(1)).run();
        });
    }

    @Test
    public void testGo_nullParameter_exitAction() throws Throwable {
        Scene scene1 = loadScene(R.layout.scene1);
        Runnable exitAction = mock(Runnable.class);
        scene1.setExitAction(exitAction);

        mActivityRule.runOnUiThread(() -> {
            TransitionManager.go(scene1, null);
            verify(exitAction, never()).run();
            clearInvocations(exitAction);
        });

        Scene scene2 = loadScene(R.layout.scene2);

        mActivityRule.runOnUiThread(() -> {
            TransitionManager.go(scene2, null);
            verify(exitAction, times(1)).run();
        });
    }

}

