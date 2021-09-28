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

package android.controls.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.service.controls.Control;
import android.service.controls.ControlsProviderService;
import android.service.controls.actions.BooleanAction;
import android.service.controls.actions.CommandAction;
import android.service.controls.actions.ControlAction;
import android.service.controls.actions.FloatAction;
import android.service.controls.actions.ModeAction;
import android.service.controls.templates.ControlTemplate;
import android.service.controls.templates.RangeTemplate;
import android.service.controls.templates.TemperatureControlTemplate;
import android.service.controls.templates.ToggleRangeTemplate;
import android.service.controls.templates.ToggleTemplate;
import android.test.mock.MockContext;

import androidx.test.runner.AndroidJUnit4;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Flow.Publisher;
import java.util.concurrent.Flow.Subscriber;
import java.util.concurrent.Flow.Subscription;
import java.util.function.Consumer;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class CtsControlsServiceTest {

    private static final String ACTION_ADD_CONTROL = "android.service.controls.action.ADD_CONTROL";
    private static final String EXTRA_CONTROL = "android.service.controls.extra.CONTROL";

    private CtsControlsService mControlsService;

    @Before
    public void setUp() {
        mControlsService = new CtsControlsService();
    }

    @Test
    public void testLoadAllAvailable() {
        Publisher<Control> publisher = mControlsService.createPublisherForAllAvailable();
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 10, loadedControls);

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(new Control.StatelessBuilder(
                mControlsService.buildLight(false, 0.0f)).build());
        expectedControls.add(new Control.StatelessBuilder(
                mControlsService.buildLock(false)).build());
        expectedControls.add(new Control.StatelessBuilder(
                mControlsService.buildRoutine()).build());
        expectedControls.add(new Control.StatelessBuilder(mControlsService.buildThermostat(
                TemperatureControlTemplate.MODE_OFF)).build());
        expectedControls.add(new Control.StatelessBuilder(
                mControlsService.buildMower(false)).build());
        expectedControls.add(new Control.StatelessBuilder(
                mControlsService.buildSwitch(false)).build());
        expectedControls.add(new Control.StatelessBuilder(
                mControlsService.buildGate(false)).build());

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testLoadSuggested() {
        Publisher<Control> publisher = mControlsService.createPublisherForSuggested();
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 3, loadedControls);

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(new Control.StatelessBuilder(
                mControlsService.buildLight(false, 0.0f)).build());
        expectedControls.add(new Control.StatelessBuilder(
                mControlsService.buildLock(false)).build());
        expectedControls.add(new Control.StatelessBuilder(
                mControlsService.buildRoutine()).build());

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testPublisherForSingleControl() {
        List<String> idsToLoad = new ArrayList<>();
        idsToLoad.add("mower");

        Publisher<Control> publisher = mControlsService.createPublisherFor(idsToLoad);
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 10, loadedControls);

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(mControlsService.buildMower(false));

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testPublisherForMultipleControls() {
        List<String> idsToLoad = new ArrayList<>();
        idsToLoad.add("lock");
        idsToLoad.add("light");

        Publisher<Control> publisher = mControlsService.createPublisherFor(idsToLoad);
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 10, loadedControls);

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(mControlsService.buildLock(false));
        expectedControls.add(mControlsService.buildLight(false, 0.0f));

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testBooleanAction() {
        List<String> idsToLoad = new ArrayList<>();
        idsToLoad.add("switch");

        Publisher<Control> publisher = mControlsService.createPublisherFor(idsToLoad);
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 10, loadedControls);

        BooleanAction action = new BooleanAction("action", true);
        assertEquals(action.getActionType(), ControlAction.TYPE_BOOLEAN);
        mControlsService.performControlAction("switch", action,
                assertConsumer(ControlAction.RESPONSE_OK));

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(mControlsService.buildSwitch(false));
        expectedControls.add(mControlsService.buildSwitch(true));

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testFloatAction() {
        List<String> idsToLoad = new ArrayList<>();
        idsToLoad.add("light");

        Publisher<Control> publisher = mControlsService.createPublisherFor(idsToLoad);
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 10, loadedControls);

        mControlsService.performControlAction("light", new BooleanAction("action", true),
                assertConsumer(ControlAction.RESPONSE_OK));

        FloatAction action = new FloatAction("action", 80.0f);
        assertEquals(action.getActionType(), ControlAction.TYPE_FLOAT);
        mControlsService.performControlAction("light", action,
                assertConsumer(ControlAction.RESPONSE_OK));

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(mControlsService.buildLight(false, 0.0f));
        expectedControls.add(mControlsService.buildLight(true, 50.0f));
        expectedControls.add(mControlsService.buildLight(true, 80.0f));

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testCommandAction() {
        List<String> idsToLoad = new ArrayList<>();
        idsToLoad.add("routine");

        Publisher<Control> publisher = mControlsService.createPublisherFor(idsToLoad);
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 10, loadedControls);

        CommandAction action = new CommandAction("action");
        assertEquals(action.getActionType(), ControlAction.TYPE_COMMAND);
        mControlsService.performControlAction("routine", action,
                assertConsumer(ControlAction.RESPONSE_OK));

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(mControlsService.buildRoutine());
        expectedControls.add(mControlsService.buildRoutine());

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testBooleanActionWithPinChallenge() {
        List<String> idsToLoad = new ArrayList<>();
        idsToLoad.add("lock");

        Publisher<Control> publisher = mControlsService.createPublisherFor(idsToLoad);
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 10, loadedControls);

        mControlsService.performControlAction("lock", new BooleanAction("action", true),
                assertConsumer(ControlAction.RESPONSE_CHALLENGE_PIN));

        mControlsService.performControlAction("lock", new BooleanAction("action", true, "1234"),
                assertConsumer(ControlAction.RESPONSE_OK));

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(mControlsService.buildLock(false));
        expectedControls.add(mControlsService.buildLock(true));

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testBooleanActionWithPassphraseChallenge() {
        List<String> idsToLoad = new ArrayList<>();
        idsToLoad.add("gate");

        Publisher<Control> publisher = mControlsService.createPublisherFor(idsToLoad);
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 10, loadedControls);

        mControlsService.performControlAction("gate", new BooleanAction("action", true),
                assertConsumer(ControlAction.RESPONSE_CHALLENGE_PASSPHRASE));

        mControlsService.performControlAction("gate", new BooleanAction("action", true, "abc123"),
                assertConsumer(ControlAction.RESPONSE_OK));

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(mControlsService.buildGate(false));
        expectedControls.add(mControlsService.buildGate(true));

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testBooleanActionWithAckChallenge() {
        List<String> idsToLoad = new ArrayList<>();
        idsToLoad.add("mower");

        Publisher<Control> publisher = mControlsService.createPublisherFor(idsToLoad);
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 10, loadedControls);

        mControlsService.performControlAction("mower", new BooleanAction("action", true),
                assertConsumer(ControlAction.RESPONSE_CHALLENGE_ACK));

        mControlsService.performControlAction("mower", new BooleanAction("action", true, "true"),
                assertConsumer(ControlAction.RESPONSE_OK));

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(mControlsService.buildMower(false));
        expectedControls.add(mControlsService.buildMower(true));

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testModeAction() {
        List<String> idsToLoad = new ArrayList<>();
        idsToLoad.add("thermostat");

        Publisher<Control> publisher = mControlsService.createPublisherFor(idsToLoad);
        List<Control> loadedControls = new ArrayList<>();
        subscribe(publisher, 10, loadedControls);

        ModeAction action = new ModeAction("action", TemperatureControlTemplate.MODE_COOL);
        assertEquals(action.getActionType(), ControlAction.TYPE_MODE);
        mControlsService.performControlAction("thermostat", action,
                assertConsumer(ControlAction.RESPONSE_OK));

        List<Control> expectedControls = new ArrayList<>();
        expectedControls.add(mControlsService.buildThermostat(TemperatureControlTemplate.MODE_OFF));
        expectedControls.add(mControlsService.buildThermostat(
                TemperatureControlTemplate.MODE_COOL));

        assertControlsList(loadedControls, expectedControls);
    }

    @Test
    public void testRequestAddControl() {
        Resources res = mock(Resources.class);
        when(res.getString(anyInt())).thenReturn("");

        final ComponentName testComponent = new ComponentName("TestPkg", "TestClass");
        final Control control = new Control.StatelessBuilder(mControlsService.buildMower(false))
                .build();

        Context context = new MockContext() {
            public Resources getResources() {
                return res;
            }

            public void sendBroadcast(Intent intent, String receiverPermission) {
                assertEquals(intent.getAction(), ACTION_ADD_CONTROL);
                assertEquals((ComponentName) intent.getParcelableExtra(Intent.EXTRA_COMPONENT_NAME),
                        testComponent);
                assertEquals((Control) intent.getParcelableExtra(EXTRA_CONTROL), control);
                assertEquals(receiverPermission, "android.permission.BIND_CONTROLS");
            }
        };

        ControlsProviderService.requestAddControl(context, testComponent, control);
    }

    private Consumer<Integer> assertConsumer(int expectedStatus) {
        return (status) -> {
            ControlAction.isValidResponse(status);
            assertEquals((int) status, expectedStatus);
        };
    }

    private void subscribe(Publisher<Control> publisher, final int request,
            final List<Control> addToList) {
        publisher.subscribe(new Subscriber<Control>() {
                public void onSubscribe(Subscription s) {
                    s.request(request);
                }

                public void onNext(Control c) {
                    addToList.add(c);
                }

                public void onError(Throwable t) {
                    throw new IllegalStateException("onError should not be called here");
                }

                public void onComplete() {

                }
            });
    }

    private void assertControlsList(List<Control> actualControls, List<Control> expectedControls) {
        assertEquals(actualControls.size(), expectedControls.size());

        for (int i = 0; i < actualControls.size(); i++) {
            assertControlEquals(actualControls.get(i), expectedControls.get(i));
        }
    }

    private void assertControlEquals(Control c1, Control c2) {
        assertEquals(c1.getTitle(), c2.getTitle());
        assertEquals(c1.getSubtitle(), c2.getSubtitle());
        assertEquals(c1.getStructure(), c2.getStructure());
        assertEquals(c1.getZone(), c2.getZone());
        assertEquals(c1.getDeviceType(), c2.getDeviceType());
        assertEquals(c1.getStatus(), c2.getStatus());
        assertEquals(c1.getControlId(), c2.getControlId());
        assertEquals(c1.getCustomIcon(), c2.getCustomIcon());
        assertEquals(c1.getCustomColor(), c2.getCustomColor());

        assertTemplateEquals(c1.getControlTemplate(), c2.getControlTemplate());
    }

    private void assertTemplateEquals(ControlTemplate ct1, ControlTemplate ct2) {
        if (ct1 == null) {
            assertNull(ct2);
            return;
        } else {
            assertNotNull(ct2);
        }

        assertNotEquals(ct1, ControlTemplate.getErrorTemplate());
        assertNotEquals(ct2, ControlTemplate.getErrorTemplate());
        assertEquals(ct1.getTemplateType(), ct2.getTemplateType());
        assertEquals(ct1.getTemplateId(), ct2.getTemplateId());

        switch (ct1.getTemplateType()) {
            case ControlTemplate.TYPE_TOGGLE:
                assertToggleTemplate((ToggleTemplate) ct1, (ToggleTemplate) ct2);
                break;
            case ControlTemplate.TYPE_RANGE:
                assertRangeTemplate((RangeTemplate) ct1, (RangeTemplate) ct2);
                break;
            case ControlTemplate.TYPE_TEMPERATURE:
                assertTemperatureControlTemplate((TemperatureControlTemplate) ct1,
                        (TemperatureControlTemplate) ct2);
                break;
            case ControlTemplate.TYPE_TOGGLE_RANGE:
                assertToggleRangeTemplate((ToggleRangeTemplate) ct1, (ToggleRangeTemplate) ct2);
                break;
        }
    }

    private void assertToggleTemplate(ToggleTemplate t1, ToggleTemplate t2) {
        assertEquals(t1.isChecked(), t2.isChecked());
        assertEquals(t1.getContentDescription(), t2.getContentDescription());
    }

    private void assertRangeTemplate(RangeTemplate t1, RangeTemplate t2) {
        assertEquals(t1.getMinValue(), t2.getMinValue(), 0.0f);
        assertEquals(t1.getMaxValue(), t2.getMaxValue(), 0.0f);
        assertEquals(t1.getCurrentValue(), t2.getCurrentValue(), 0.0f);
        assertEquals(t1.getStepValue(), t2.getStepValue(), 0.0f);
        assertEquals(t1.getFormatString(), t2.getFormatString());
    }

    private void assertTemperatureControlTemplate(TemperatureControlTemplate t1,
            TemperatureControlTemplate t2) {
        assertEquals(t1.getCurrentMode(), t2.getCurrentMode());
        assertEquals(t1.getCurrentActiveMode(), t2.getCurrentActiveMode());
        assertEquals(t1.getModes(), t2.getModes());
        assertTemplateEquals(t1.getTemplate(), t2.getTemplate());
    }

    private void assertToggleRangeTemplate(ToggleRangeTemplate t1, ToggleRangeTemplate t2) {
        assertEquals(t1.isChecked(), t2.isChecked());
        assertEquals(t1.getActionDescription(), t2.getActionDescription());
        assertRangeTemplate(t1.getRange(), t2.getRange());
    }
}
