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

package android.uirendering.cts.testclasses;

import static junit.framework.Assert.assertEquals;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.Color;
import android.uirendering.cts.R;
import android.uirendering.cts.bitmapverifiers.ColorVerifier;
import android.uirendering.cts.testclasses.view.AlphaTestView;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;
import android.uirendering.cts.testinfrastructure.ViewInitializer;
import android.view.View;
import android.view.ViewPropertyAnimator;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ViewPropertyAnimatorTests extends ActivityTestBase {

    @Test
    public void testViewCustomAlpha() {
        createViewPropertyAnimatorTest(new ViewPropertyAnimatorTestDelegate<AlphaTestView>() {
            @Override
            public void configureView(AlphaTestView target) {
                target.setStartColor(Color.RED);
                target.setEndColor(Color.BLUE);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.alpha(0.0f);
            }

            @Override
            public void verifyViewState(AlphaTestView target) {
                assertEquals(Color.BLUE, target.getBlendedColor());
            }

        }).runWithVerifier(new ColorVerifier(Color.BLUE));
    }

    @Test
    public void testViewNonCustomAlpha() {
        final CountDownLatch latch = new CountDownLatch(1);
        createTest().addLayout(R.layout.viewpropertyanimator_test_layout,
                (ViewInitializer) view -> {
                    View testContent = view.findViewById(R.id.viewalpha_test_container);
                    testContent.animate().alpha(0.0f).setListener(new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            latch.countDown();
                        }
                    }).setDuration(16).start();
                }, true, latch).runWithVerifier(new ColorVerifier(Color.WHITE));
    }

    @Test
    public void testViewCustomAlphaBy() {
        createViewPropertyAnimatorTest(new ViewPropertyAnimatorTestDelegate<AlphaTestView>() {
            @Override
            public void configureView(AlphaTestView target) {
                target.setStartColor(Color.RED);
                target.setEndColor(Color.BLUE);
                target.setAlpha(0.5f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.alphaBy(-0.5f);
            }

            @Override
            public void verifyViewState(AlphaTestView target) {
                assertEquals(Color.BLUE, target.getBlendedColor());
            }

        }).runWithVerifier(new ColorVerifier(Color.BLUE));
    }

    @Test
    public void testViewTranslateX() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.translationX(100.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(100.0f, target.getTranslationX());
            }
        });
    }

    @Test
    public void testViewTranslateXBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setTranslationX(20.0f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.translationXBy(100.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(120.0f, target.getTranslationX());
            }
        });
    }

    @Test
    public void testViewTranslateY() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.translationY(60.0f);

            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(60.0f, target.getTranslationY());
            }
        });
    }

    @Test
    public void testViewTranslateYBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setTranslationY(30.0f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.translationYBy(60.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(90.0f, target.getTranslationY());
            }
        });
    }

    @Test
    public void testViewTranslateZ() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {
            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.translationZ(30.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(30.0f, target.getTranslationZ());
            }
        });
    }

    @Test
    public void testViewTranslateZBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setTranslationZ(40.0f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.translationZBy(30.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(70.0f, target.getTranslationZ());
            }
        });
    }

    @Test
    public void testViewRotation() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {
            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.rotation(20.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(20.0f, target.getRotation());
            }
        });
    }

    @Test
    public void testViewRotationBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setRotation(30.0f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.rotationBy(20.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(50.0f, target.getRotation());
            }
        });
    }

    @Test
    public void testViewRotationX() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {
            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.rotationX(80.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(80.0f, target.getRotationX());
            }
        });
    }

    @Test
    public void testViewRotationXBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setRotationX(30.0f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.rotationXBy(80.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(110.0f, target.getRotationX());
            }
        });
    }

    @Test
    public void testViewRotationY() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {
            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.rotationY(25.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(25.0f, target.getRotationY());
            }
        });
    }

    @Test
    public void testViewRotationYBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setRotationY(10.0f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.rotationYBy(25.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(35.0f, target.getRotationY());
            }
        });
    }

    @Test
    public void testViewScaleX() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {
            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.scaleX(2.5f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(2.5f, target.getScaleX());
            }
        });
    }

    @Test
    public void testViewScaleXBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setScaleX(1.2f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.scaleXBy(2.5f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(3.7f, target.getScaleX());
            }
        });
    }

    @Test
    public void testViewScaleY() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {
            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.scaleY(3.2f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(3.2f, target.getScaleY());
            }
        });
    }

    @Test
    public void testViewScaleYBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setScaleY(1.2f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.scaleYBy(3.2f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(4.4f, target.getScaleY());
            }
        });
    }

    @Test
    public void testViewX() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {
            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.x(27.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(27.0f, target.getX());
            }
        });
    }

    @Test
    public void testViewXBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setX(140.0f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.xBy(27.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(167.0f, target.getX());
            }
        });
    }

    @Test
    public void testViewY() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {
            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.y(77.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(77.0f, target.getY());
            }
        });
    }

    @Test
    public void testViewYBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setY(80.0f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.yBy(77.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(157.0f, target.getY());
            }
        });
    }

    @Test
    public void testViewZ() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {
            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.z(17.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(17.0f, target.getZ());
            }
        });
    }

    @Test
    public void testViewZBy() {
        runViewPropertyAnimatorTestWithoutVerification(new ViewPropertyAnimatorTestDelegate() {

            @Override
            public void configureView(View target) {
                target.setZ(38.0f);
            }

            @Override
            public void configureAnimator(ViewPropertyAnimator animator) {
                animator.zBy(17.0f);
            }

            @Override
            public void verifyViewState(View target) {
                assertEquals(55.0f, target.getZ());
            }
        });
    }

    private void runViewPropertyAnimatorTestWithoutVerification(
            ViewPropertyAnimatorTestDelegate delegate) {
        createViewPropertyAnimatorTest(delegate).runWithoutVerification();
    }

    private TestCaseBuilder createViewPropertyAnimatorTest(
            final ViewPropertyAnimatorTestDelegate delegate) {
        final CountDownLatch latch = new CountDownLatch(1);
        return createTest().addLayout(R.layout.viewpropertyanimator_test_layout,
                (ViewInitializer) view -> {
                    AlphaTestView alphaView = view.findViewById(R.id.alpha_test_view);
                    delegate.configureView(alphaView);
                    alphaView.setStartColor(Color.RED);
                    alphaView.setEndColor(Color.BLUE);
                    ViewPropertyAnimator animator = alphaView.animate();
                    delegate.configureAnimator(animator);
                    animator.setListener(
                            new AnimatorListenerAdapter() {

                                @Override
                                public void onAnimationEnd(Animator animator) {
                                    delegate.verifyViewState(alphaView);
                                    latch.countDown();
                                }

                            }).setDuration(16).start();
                }, true, latch);
    }

    private interface ViewPropertyAnimatorTestDelegate<T extends View> {

        default void configureView(T target) {
            // NO-OP
        }

        void configureAnimator(ViewPropertyAnimator animator);

        void verifyViewState(T target);
    }
}
