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

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;

import android.transition.Scene;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.invocation.InvocationOnMock;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class SceneTest extends BaseTransitionTest {
    /**
     * Test Scene(ViewGroup) with enterAction and exitAction
     */
    @Test
    public void testDynamicConstructor() throws Throwable {
        Scene scene = new Scene(mSceneRoot);
        assertEquals(mSceneRoot, scene.getSceneRoot());
        Runnable enterCheck = mock(Runnable.class);
        doAnswer((InvocationOnMock invocation) -> mActivity.getLayoutInflater().inflate(
                R.layout.scene1, mSceneRoot, true)).when(enterCheck).run();
        scene.setEnterAction(enterCheck);
        Runnable exitCheck = mock(Runnable.class);
        scene.setExitAction(exitCheck);
        enterScene(scene);

        verify(enterCheck, times(1)).run();
        verifyZeroInteractions(exitCheck);

        View redSquare = mActivity.findViewById(R.id.redSquare);
        assertNotNull(redSquare);

        exitScene(scene);
        assertNotNull(mSceneRoot.findViewById(R.id.redSquare));
        verify(exitCheck, times(1)).run();

        enterScene(R.layout.scene4);
        assertNull(mSceneRoot.findViewById(R.id.redSquare));
    }

    /**
     * Test Scene(ViewGroup, View)
     */
    @Test
    public void testViewConstructor() throws Throwable {
        View view = loadLayout(R.layout.scene1);
        constructorTest(new Scene(mSceneRoot, view));
    }

    /**
     * Test Scene(ViewGroup, ViewGroup)
     */
    @Test
    public void testDeprecatedConstructor() throws Throwable {
        View view = loadLayout(R.layout.scene1);
        constructorTest(new Scene(mSceneRoot, (ViewGroup) view));
    }

    /**
     * Test Scene.getSceneForLayout
     */
    @Test
    public void testFactory() throws Throwable {
        Scene scene = loadScene(R.layout.scene1);
        constructorTest(scene);
    }

    @Test
    public void testGetCurrentScene() throws Throwable {
        Scene scene = Scene.getSceneForLayout(mSceneRoot, R.layout.scene1, mActivity);
        enterScene(scene);
        assertThat(Scene.getCurrentScene(mSceneRoot), is(scene));
    }

    /**
     * Tests that the Scene was constructed properly from a scene1
     */
    private void constructorTest(Scene scene) throws Throwable {
        assertEquals(mSceneRoot, scene.getSceneRoot());
        Runnable enterCheck = mock(Runnable.class);
        scene.setEnterAction(enterCheck);
        Runnable exitCheck = mock(Runnable.class);
        scene.setExitAction(exitCheck);
        enterScene(scene);

        verify(enterCheck, times(1)).run();
        verifyZeroInteractions(exitCheck);

        View redSquare = mActivity.findViewById(R.id.redSquare);
        assertNotNull(redSquare);

        exitScene(scene);
        assertNotNull(mSceneRoot.findViewById(R.id.redSquare));
        verify(exitCheck, times(1)).run();
    }
}

