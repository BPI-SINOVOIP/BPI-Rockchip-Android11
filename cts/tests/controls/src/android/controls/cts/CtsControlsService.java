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

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.Icon;
import android.service.controls.Control;
import android.service.controls.ControlsProviderService;
import android.service.controls.DeviceTypes;
import android.service.controls.actions.BooleanAction;
import android.service.controls.actions.CommandAction;
import android.service.controls.actions.ControlAction;
import android.service.controls.actions.FloatAction;
import android.service.controls.actions.ModeAction;
import android.service.controls.templates.ControlButton;
import android.service.controls.templates.ControlTemplate;
import android.service.controls.templates.RangeTemplate;
import android.service.controls.templates.StatelessTemplate;
import android.service.controls.templates.TemperatureControlTemplate;
import android.service.controls.templates.ToggleRangeTemplate;
import android.service.controls.templates.ToggleTemplate;

import androidx.test.InstrumentationRegistry;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Flow.Publisher;
import java.util.function.Consumer;
import java.util.stream.Collectors;

/**
  * CTS Controls Service to send known controls for testing.
  */
public class CtsControlsService extends ControlsProviderService {

    private CtsControlsPublisher mUpdatePublisher;
    private final List<Control> mAllControls = new ArrayList<>();
    private final Map<String, Control> mControlsById = new HashMap<>();
    private final Context mContext;
    private final PendingIntent mPendingIntent;
    private ColorStateList mColorStateList;
    private Icon mIcon;

    public CtsControlsService() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mPendingIntent = PendingIntent.getActivity(mContext, 1, new Intent(),
            PendingIntent.FLAG_UPDATE_CURRENT);
        mIcon = Icon.createWithResource(mContext, R.drawable.ic_device_unknown);
        mColorStateList = mContext.getResources().getColorStateList(R.color.custom_mower, null);

        mAllControls.add(buildLight(false /* isOn */, 0.0f /* intensity */));
        mAllControls.add(buildLock(false /* isLocked */));
        mAllControls.add(buildRoutine());
        mAllControls.add(buildThermostat(TemperatureControlTemplate.MODE_OFF));
        mAllControls.add(buildMower(false /* isStarted */));
        mAllControls.add(buildSwitch(false /* isOn */));
        mAllControls.add(buildGate(false /* isLocked */));

        for (Control c : mAllControls) {
            mControlsById.put(c.getControlId(), c);
        }
    }

    public Control buildLight(boolean isOn, float intensity) {
        RangeTemplate rt = new RangeTemplate("range", 0.0f, 100.0f, intensity, 1.0f, null);
        ControlTemplate template =
                new ToggleRangeTemplate("toggleRange", isOn, isOn ? "On" : "Off", rt);
        return new Control.StatefulBuilder("light", mPendingIntent)
            .setTitle("Light Title")
            .setSubtitle("Light Subtitle")
            .setStatus(Control.STATUS_OK)
            .setStatusText(isOn ? "On" : "Off")
            .setDeviceType(DeviceTypes.TYPE_LIGHT)
            .setStructure("Home")
            .setControlTemplate(template)
            .build();
    }

    public Control buildSwitch(boolean isOn) {
        ControlButton button = new ControlButton(isOn, isOn ? "On" : "Off");
        ControlTemplate template = new ToggleTemplate("toggle", button);
        return new Control.StatefulBuilder("switch", mPendingIntent)
            .setTitle("Switch Title")
            .setSubtitle("Switch Subtitle")
            .setStatus(Control.STATUS_OK)
            .setStatusText(isOn ? "On" : "Off")
            .setDeviceType(DeviceTypes.TYPE_SWITCH)
            .setStructure("Home")
            .setControlTemplate(template)
            .build();
    }


    public Control buildMower(boolean isStarted) {
        String desc = isStarted ? "Started" : "Stopped";
        ControlButton button = new ControlButton(isStarted, desc);
        ControlTemplate template = new ToggleTemplate("toggle", button);
        return new Control.StatefulBuilder("mower", mPendingIntent)
            .setTitle("Mower Title")
            .setSubtitle("Mower Subtitle")
            .setStatus(Control.STATUS_OK)
            .setStatusText(desc)
            .setDeviceType(DeviceTypes.TYPE_MOWER)
            .setStructure("Vacation")
            .setZone("Outside")
            .setControlTemplate(template)
            .setCustomIcon(mIcon)
            .setCustomColor(mColorStateList)
            .build();
    }

    public Control buildLock(boolean isLocked) {
        String desc = isLocked ? "Locked" : "Unlocked";
        ControlButton button = new ControlButton(isLocked, desc);
        ControlTemplate template = new ToggleTemplate("toggle", button);
        return new Control.StatefulBuilder("lock", mPendingIntent)
            .setTitle("Lock Title")
            .setSubtitle("Lock Subtitle")
            .setStatus(Control.STATUS_OK)
            .setStatusText(desc)
            .setDeviceType(DeviceTypes.TYPE_LOCK)
            .setControlTemplate(template)
            .build();
    }

    public Control buildGate(boolean isLocked) {
        String desc = isLocked ? "Locked" : "Unlocked";
        ControlButton button = new ControlButton(isLocked, desc);
        ControlTemplate template = new ToggleTemplate("toggle", button);
        return new Control.StatefulBuilder("gate", mPendingIntent)
            .setTitle("Gate Title")
            .setSubtitle("Gate Subtitle")
            .setStatus(Control.STATUS_OK)
            .setStatusText(desc)
            .setDeviceType(DeviceTypes.TYPE_GATE)
            .setControlTemplate(template)
            .setStructure("Other home")
            .build();
    }

    public Control buildThermostat(int mode) {
        ControlTemplate template = new TemperatureControlTemplate("temperature",
                    ControlTemplate.getNoTemplateObject(),
                    mode,
                    TemperatureControlTemplate.MODE_OFF,
                    TemperatureControlTemplate.FLAG_MODE_HEAT
                    | TemperatureControlTemplate.FLAG_MODE_COOL
                    | TemperatureControlTemplate.FLAG_MODE_OFF
                    | TemperatureControlTemplate.FLAG_MODE_ECO);

        return new Control.StatefulBuilder("thermostat", mPendingIntent)
            .setTitle("Thermostat Title")
            .setSubtitle("Thermostat Subtitle")
            .setStatus(Control.STATUS_OK)
            .setStatusText("Off")
            .setDeviceType(DeviceTypes.TYPE_THERMOSTAT)
            .setControlTemplate(template)
            .build();
    }

    public Control buildRoutine() {
        ControlTemplate template = new StatelessTemplate("stateless");
        return new Control.StatefulBuilder("routine", mPendingIntent)
            .setTitle("Routine Title")
            .setSubtitle("Routine Subtitle")
            .setStatus(Control.STATUS_OK)
            .setStatusText("Good Morning")
            .setDeviceType(DeviceTypes.TYPE_ROUTINE)
            .setControlTemplate(template)
            .build();
    }

    @Override
    public Publisher<Control> createPublisherForAllAvailable() {
        return new CtsControlsPublisher(mAllControls.stream()
            .map(c -> new Control.StatelessBuilder(c).build())
            .collect(Collectors.toList()));
    }

    @Override
    public Publisher<Control> createPublisherForSuggested() {
        return new CtsControlsPublisher(mAllControls.stream()
            .map(c -> new Control.StatelessBuilder(c).build())
            .collect(Collectors.toList()));
    }

    @Override
    public Publisher<Control> createPublisherFor(List<String> controlIds) {
        mUpdatePublisher = new CtsControlsPublisher(null);

        for (String id : controlIds) {
            Control control = mControlsById.get(id);
            if (control == null) continue;

            mUpdatePublisher.onNext(control);
        }

        return mUpdatePublisher;
    }

    @Override
    public void performControlAction(String controlId, ControlAction action,
            Consumer<Integer> consumer) {
        Control c = mControlsById.get(controlId);
        if (c == null) return;

        // all values are hardcoded for this test
        assertEquals(action.getTemplateId(), "action");
        assertNotEquals(action, ControlAction.getErrorAction());

        Control.StatefulBuilder builder = controlToBuilder(c);

        // Modify the builder in order to update the Control to have predefined, verifiable behavior
        if (action instanceof BooleanAction) {
            BooleanAction b = (BooleanAction) action;

            if (c.getDeviceType() == DeviceTypes.TYPE_LIGHT) {
                RangeTemplate rt = new RangeTemplate("range",
                        0.0f /* minValue */,
                        100.0f /* maxValue */,
                        50.0f /* currentValue */,
                        1.0f /* step */, null);
                String desc = b.getNewState() ? "On" : "Off";

                builder.setStatusText(desc);
                builder.setControlTemplate(new ToggleRangeTemplate("toggleRange", b.getNewState(),
                        desc, rt));
            } else if (c.getDeviceType() == DeviceTypes.TYPE_ROUTINE) {
                builder.setStatusText("Running");
                builder.setControlTemplate(new StatelessTemplate("stateless"));
            } else if (c.getDeviceType() == DeviceTypes.TYPE_SWITCH) {
                String desc = b.getNewState() ? "On" : "Off";
                builder.setStatusText(desc);
                ControlButton button = new ControlButton(b.getNewState(), desc);
                builder.setControlTemplate(new ToggleTemplate("toggle", button));
            } else if (c.getDeviceType() == DeviceTypes.TYPE_LOCK) {
                String value = action.getChallengeValue();
                if (value != null && value.equals("1234")) {
                    String desc = b.getNewState() ? "Locked" : "Unlocked";
                    ControlButton button = new ControlButton(b.getNewState(), desc);
                    builder.setStatusText(desc);
                    builder.setControlTemplate(new ToggleTemplate("toggle", button));
                } else {
                    consumer.accept(ControlAction.RESPONSE_CHALLENGE_PIN);
                    return;
                }
            } else if (c.getDeviceType() == DeviceTypes.TYPE_GATE) {
                String value = action.getChallengeValue();
                if (value != null && value.equals("abc123")) {
                    String desc = b.getNewState() ? "Locked" : "Unlocked";
                    ControlButton button = new ControlButton(b.getNewState(), desc);
                    builder.setStatusText(desc);
                    builder.setControlTemplate(new ToggleTemplate("toggle", button));
                } else {
                    consumer.accept(ControlAction.RESPONSE_CHALLENGE_PASSPHRASE);
                    return;
                }
            } else if (c.getDeviceType() == DeviceTypes.TYPE_MOWER) {
                String value = action.getChallengeValue();
                if (value != null && value.equals("true")) {
                    String desc = b.getNewState() ? "Started" : "Stopped";
                    ControlButton button = new ControlButton(b.getNewState(), desc);
                    builder.setStatusText(desc);
                    builder.setControlTemplate(new ToggleTemplate("toggle", button));
                } else {
                    consumer.accept(ControlAction.RESPONSE_CHALLENGE_ACK);
                    return;
                }
            }
        } else if (action instanceof FloatAction) {
            FloatAction f = (FloatAction) action;
            if (c.getDeviceType() == DeviceTypes.TYPE_LIGHT) {
                RangeTemplate rt = new RangeTemplate("range", 0.0f, 100.0f, f.getNewValue(), 1.0f,
                        null);

                ToggleRangeTemplate trt = (ToggleRangeTemplate) c.getControlTemplate();
                String desc = trt.getActionDescription().toString();
                boolean state = trt.isChecked();

                builder.setStatusText(desc);
                builder.setControlTemplate(new ToggleRangeTemplate("toggleRange", state, desc, rt));
            }
        } else if (action instanceof ModeAction) {
            ModeAction m = (ModeAction) action;
            if (c.getDeviceType() == DeviceTypes.TYPE_THERMOSTAT) {
                ControlTemplate template = new TemperatureControlTemplate("temperature",
                        ControlTemplate.getNoTemplateObject(),
                        m.getNewMode(),
                        TemperatureControlTemplate.MODE_OFF,
                        TemperatureControlTemplate.FLAG_MODE_HEAT
                        | TemperatureControlTemplate.FLAG_MODE_COOL
                        | TemperatureControlTemplate.FLAG_MODE_OFF
                        | TemperatureControlTemplate.FLAG_MODE_ECO);

                builder.setControlTemplate(template);
            }
        } else if (action instanceof CommandAction) {
            builder.setControlTemplate(new StatelessTemplate("stateless"));
        }

        // Finally build and send the default OK status
        Control updatedControl = builder.build();
        mControlsById.put(controlId, updatedControl);
        mUpdatePublisher.onNext(updatedControl);
        consumer.accept(ControlAction.RESPONSE_OK);
    }

    private Control.StatefulBuilder controlToBuilder(Control c) {
        return new Control.StatefulBuilder(c.getControlId(), c.getAppIntent())
            .setTitle(c.getTitle())
            .setSubtitle(c.getSubtitle())
            .setStructure(c.getStructure())
            .setDeviceType(c.getDeviceType())
            .setZone(c.getZone())
            .setCustomIcon(c.getCustomIcon())
            .setCustomColor(c.getCustomColor())
            .setStatus(c.getStatus())
            .setStatusText(c.getStatusText());
    }
}
