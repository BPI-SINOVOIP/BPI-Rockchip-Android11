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

package android.theme.app;

import android.os.Build;
import android.theme.app.modifiers.DatePickerModifier;
import android.theme.app.modifiers.ProgressBarModifier;
import android.theme.app.modifiers.SearchViewModifier;
import android.theme.app.modifiers.ViewCheckedModifier;
import android.theme.app.modifiers.ViewPressedModifier;
import android.theme.app.modifiers.TimePickerModifier;

/**
 * Constants defining the themes and layouts to be verified.
 */
public class TestConfiguration {
    @SuppressWarnings("deprecation")
    static final ThemeInfo[] THEMES = {
            // Holo
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo,
                    Build.VERSION_CODES.HONEYCOMB, "holo"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Dialog,
                    Build.VERSION_CODES.HONEYCOMB, "holo_dialog"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Dialog_MinWidth,
                    Build.VERSION_CODES.HONEYCOMB, "holo_dialog_minwidth"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Dialog_NoActionBar,
                    Build.VERSION_CODES.HONEYCOMB, "holo_dialog_noactionbar"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Dialog_NoActionBar_MinWidth,
                    Build.VERSION_CODES.HONEYCOMB, "holo_dialog_noactionbar_minwidth"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_DialogWhenLarge,
                    Build.VERSION_CODES.HONEYCOMB, "holo_dialogwhenlarge"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_DialogWhenLarge_NoActionBar,
                    Build.VERSION_CODES.HONEYCOMB, "holo_dialogwhenlarge_noactionbar"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_InputMethod,
                    Build.VERSION_CODES.HONEYCOMB, "holo_inputmethod"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_NoActionBar,
                    Build.VERSION_CODES.HONEYCOMB, "holo_noactionbar"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_NoActionBar_Fullscreen,
                    Build.VERSION_CODES.HONEYCOMB, "holo_noactionbar_fullscreen"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_NoActionBar_Overscan,
                    Build.VERSION_CODES.JELLY_BEAN_MR2, "holo_noactionbar_overscan"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_NoActionBar_TranslucentDecor,
                    Build.VERSION_CODES.KITKAT, "holo_noactionbar_translucentdecor"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Panel,
                    Build.VERSION_CODES.HONEYCOMB, "holo_panel"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Wallpaper,
                    Build.VERSION_CODES.HONEYCOMB, "holo_wallpaper"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Wallpaper_NoTitleBar,
                    Build.VERSION_CODES.HONEYCOMB, "holo_wallpaper_notitlebar"),

            // Holo Light
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light,
                    Build.VERSION_CODES.HONEYCOMB, "holo_light"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_DarkActionBar,
                    Build.VERSION_CODES.ICE_CREAM_SANDWICH, "holo_light_darkactionbar"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_Dialog,
                    Build.VERSION_CODES.HONEYCOMB, "holo_light_dialog"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_Dialog_MinWidth,
                    Build.VERSION_CODES.HONEYCOMB, "holo_light_dialog_minwidth"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_Dialog_NoActionBar,
                    Build.VERSION_CODES.HONEYCOMB, "holo_light_dialog_noactionbar"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_Dialog_NoActionBar_MinWidth,
                    Build.VERSION_CODES.HONEYCOMB, "holo_light_dialog_noactionbar_minwidth"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_DialogWhenLarge,
                    Build.VERSION_CODES.HONEYCOMB, "holo_light_dialogwhenlarge"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_DialogWhenLarge_NoActionBar,
                    Build.VERSION_CODES.HONEYCOMB, "holo_light_dialogwhenlarge_noactionbar"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_NoActionBar,
                    Build.VERSION_CODES.HONEYCOMB_MR2, "holo_light_noactionbar"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_NoActionBar_Fullscreen,
                    Build.VERSION_CODES.HONEYCOMB_MR2, "holo_light_noactionbar_fullscreen"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_NoActionBar_Overscan,
                    Build.VERSION_CODES.JELLY_BEAN_MR2, "holo_light_noactionbar_overscan"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_NoActionBar_TranslucentDecor,
                    Build.VERSION_CODES.KITKAT, "holo_light_noactionbar_translucentdecor"),
            new ThemeInfo(ThemeInfo.HOLO, android.R.style.Theme_Holo_Light_Panel,
                    Build.VERSION_CODES.HONEYCOMB, "holo_light_panel"),

            // Material
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material,
                    Build.VERSION_CODES.LOLLIPOP, "material"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Dialog,
                    Build.VERSION_CODES.LOLLIPOP, "material_dialog"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Dialog_Alert,
                    Build.VERSION_CODES.LOLLIPOP, "material_dialog_alert"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Dialog_MinWidth,
                    Build.VERSION_CODES.LOLLIPOP, "material_dialog_minwidth"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Dialog_NoActionBar,
                    Build.VERSION_CODES.LOLLIPOP, "material_dialog_noactionbar"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Dialog_NoActionBar_MinWidth,
                    Build.VERSION_CODES.LOLLIPOP, "material_dialog_noactionbar_minwidth"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Dialog_Presentation,
                    Build.VERSION_CODES.LOLLIPOP, "material_dialog_presentation"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_DialogWhenLarge,
                    Build.VERSION_CODES.LOLLIPOP, "material_dialogwhenlarge"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_DialogWhenLarge_NoActionBar,
                    Build.VERSION_CODES.LOLLIPOP, "material_dialogwhenlarge_noactionbar"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_InputMethod,
                    Build.VERSION_CODES.LOLLIPOP, "material_inputmethod"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_NoActionBar,
                    Build.VERSION_CODES.LOLLIPOP, "material_noactionbar"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_NoActionBar_Fullscreen,
                    Build.VERSION_CODES.LOLLIPOP, "material_noactionbar_fullscreen"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_NoActionBar_Overscan,
                    Build.VERSION_CODES.LOLLIPOP, "material_noactionbar_overscan"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_NoActionBar_TranslucentDecor,
                    Build.VERSION_CODES.LOLLIPOP, "material_noactionbar_translucentdecor"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Panel,
                    Build.VERSION_CODES.LOLLIPOP, "material_panel"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Settings,
                    Build.VERSION_CODES.LOLLIPOP, "material_settings"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Voice,
                    Build.VERSION_CODES.LOLLIPOP, "material_voice"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Wallpaper,
                    Build.VERSION_CODES.LOLLIPOP, "material_wallpaper"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Wallpaper_NoTitleBar,
                    Build.VERSION_CODES.LOLLIPOP, "material_wallpaper_notitlebar"),

            // Material Light
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light,
                    Build.VERSION_CODES.LOLLIPOP, "material_light"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_DarkActionBar,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_darkactionbar"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_Dialog,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_dialog"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_Dialog_Alert,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_dialog_alert"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_Dialog_MinWidth,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_dialog_minwidth"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_Dialog_NoActionBar,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_dialog_noactionbar"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_Dialog_NoActionBar_MinWidth,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_dialog_noactionbar_minwidth"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_Dialog_Presentation,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_dialog_presentation"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_DialogWhenLarge,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_dialogwhenlarge"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_DialogWhenLarge_NoActionBar,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_dialogwhenlarge_noactionbar"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_LightStatusBar,
                    Build.VERSION_CODES.M, "material_light_lightstatusbar"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_NoActionBar,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_noactionbar"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_NoActionBar_Fullscreen,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_noactionbar_fullscreen"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_NoActionBar_Overscan,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_noactionbar_overscan"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_NoActionBar_TranslucentDecor,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_noactionbar_translucentdecor"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_Panel,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_panel"),
            new ThemeInfo(ThemeInfo.MATERIAL, android.R.style.Theme_Material_Light_Voice,
                    Build.VERSION_CODES.LOLLIPOP, "material_light_voice")
    };

    static final LayoutInfo[] LAYOUTS = {
            new LayoutInfo(R.layout.button, "button"),
            new LayoutInfo(R.layout.button, "button_pressed",
                    new ViewPressedModifier()),
            new LayoutInfo(R.layout.checkbox, "checkbox"),
            new LayoutInfo(R.layout.checkbox, "checkbox_checked",
                    new ViewCheckedModifier()),
            new LayoutInfo(R.layout.chronometer, "chronometer"),
            new LayoutInfo(R.layout.color_blue_bright, "color_blue_bright"),
            new LayoutInfo(R.layout.color_blue_dark, "color_blue_dark"),
            new LayoutInfo(R.layout.color_blue_light, "color_blue_light"),
            new LayoutInfo(R.layout.color_green_dark, "color_green_dark"),
            new LayoutInfo(R.layout.color_green_light, "color_green_light"),
            new LayoutInfo(R.layout.color_orange_dark, "color_orange_dark"),
            new LayoutInfo(R.layout.color_orange_light, "color_orange_light"),
            new LayoutInfo(R.layout.color_purple, "color_purple"),
            new LayoutInfo(R.layout.color_red_dark, "color_red_dark"),
            new LayoutInfo(R.layout.color_red_light, "color_red_light"),
            new LayoutInfo(R.layout.datepicker, "datepicker",
                    new DatePickerModifier()),
            new LayoutInfo(R.layout.edittext, "edittext"),
            new LayoutInfo(R.layout.progressbar_horizontal_0, "progressbar_horizontal_0"),
            new LayoutInfo(R.layout.progressbar_horizontal_100, "progressbar_horizontal_100"),
            new LayoutInfo(R.layout.progressbar_horizontal_50, "progressbar_horizontal_50"),
            new LayoutInfo(R.layout.progressbar_large, "progressbar_large",
                    new ProgressBarModifier()),
            new LayoutInfo(R.layout.progressbar_small, "progressbar_small",
                    new ProgressBarModifier()),
            new LayoutInfo(R.layout.progressbar, "progressbar",
                    new ProgressBarModifier()),
            new LayoutInfo(R.layout.radiobutton_checked, "radiobutton_checked"),
            new LayoutInfo(R.layout.radiobutton, "radiobutton"),
            new LayoutInfo(R.layout.radiogroup_horizontal, "radiogroup_horizontal"),
            new LayoutInfo(R.layout.radiogroup_vertical, "radiogroup_vertical"),
            // Temporarily remove tests for the RatingBar widget. It has indeterminate rendering
            // behavior on 360dpi devices, but we don't know why yet.
            //new LayoutInfo(R.layout.ratingbar_0, "ratingbar_0"),
            //new LayoutInfo(R.layout.ratingbar_2point5, "ratingbar_2point5"),
            //new LayoutInfo(R.layout.ratingbar_5, "ratingbar_5"),
            //new LayoutInfo(R.layout.ratingbar_0, "ratingbar_0_pressed",
            //        new ViewPressedModifier()),
            //new LayoutInfo(R.layout.ratingbar_2point5, "ratingbar_2point5_pressed",
            //        new ViewPressedModifier()),
            //new LayoutInfo(R.layout.ratingbar_5, "ratingbar_5_pressed",
            //        new ViewPressedModifier()),
            // Temporarily remove tests for the SearchView widget with no hint. The "X" icon has
            // indeterminate rendering behavior on 480dpi devices, but we don't know why yet.
            //new LayoutInfo(R.layout.searchview, "searchview_query",
            //        new SearchViewModifier(SearchViewModifier.QUERY)),
            new LayoutInfo(R.layout.searchview, "searchview_query_hint",
                    new SearchViewModifier(SearchViewModifier.QUERY_HINT)),
            new LayoutInfo(R.layout.seekbar_0, "seekbar_0"),
            new LayoutInfo(R.layout.seekbar_100, "seekbar_100"),
            new LayoutInfo(R.layout.seekbar_50, "seekbar_50"),
            new LayoutInfo(R.layout.spinner, "spinner"),
            new LayoutInfo(R.layout.switch_button_checked, "switch_button_checked"),
            new LayoutInfo(R.layout.switch_button, "switch_button"),
            new LayoutInfo(R.layout.textview, "textview"),
            new LayoutInfo(R.layout.timepicker, "timepicker",
                    new TimePickerModifier()),
            new LayoutInfo(R.layout.togglebutton_checked, "togglebutton_checked"),
            new LayoutInfo(R.layout.togglebutton, "togglebutton"),
    };
}
