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

package com.android.tv.twopanelsettings.slices;

import static android.app.slice.Slice.HINT_PARTIAL;
import static android.app.slice.Slice.HINT_SUMMARY;
import static android.app.slice.Slice.HINT_TITLE;
import static android.app.slice.SliceItem.FORMAT_ACTION;
import static android.app.slice.SliceItem.FORMAT_IMAGE;
import static android.app.slice.SliceItem.FORMAT_INT;
import static android.app.slice.SliceItem.FORMAT_LONG;
import static android.app.slice.SliceItem.FORMAT_SLICE;
import static android.app.slice.SliceItem.FORMAT_TEXT;

import static com.android.tv.twopanelsettings.slices.SlicesConstants.CHECKMARK;
import static com.android.tv.twopanelsettings.slices.SlicesConstants.EXTRA_ACTION_ID;
import static com.android.tv.twopanelsettings.slices.SlicesConstants.EXTRA_PAGE_ID;
import static com.android.tv.twopanelsettings.slices.SlicesConstants.EXTRA_PREFERENCE_INFO_IMAGE;
import static com.android.tv.twopanelsettings.slices.SlicesConstants.EXTRA_PREFERENCE_INFO_TEXT;
import static com.android.tv.twopanelsettings.slices.SlicesConstants.RADIO;
import static com.android.tv.twopanelsettings.slices.SlicesConstants.SWITCH;

import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;
import android.view.ContextThemeWrapper;

import androidx.core.graphics.drawable.IconCompat;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.slice.Slice;
import androidx.slice.SliceItem;
import androidx.slice.core.SliceActionImpl;
import androidx.slice.core.SliceQuery;
import androidx.slice.widget.SliceContent;

import com.android.tv.twopanelsettings.IconUtil;
import com.android.tv.twopanelsettings.R;

import java.util.ArrayList;
import java.util.List;

/**
 * Generate corresponding preference based upon the slice data.
 */
public final class SlicePreferencesUtil {

    static Preference getPreference(SliceItem item, ContextThemeWrapper contextThemeWrapper,
            String className) {
        Preference preference = null;
        if (item == null) {
            return null;
        }
        Data data = extract(item);
        if (item.getSubType() != null) {
            String subType = item.getSubType();
            if (subType.equals(SlicesConstants.TYPE_PREFERENCE)
                    || subType.equals(SlicesConstants.TYPE_PREFERENCE_EMBEDDED)) {
                // TODO: Figure out all the possible cases and reorganize the logic
                if (data.mInfoItems.size() > 0) {
                    preference = new InfoPreference(
                                contextThemeWrapper, getInfoList(data.mInfoItems));
                } else if (data.mIntentItem != null) {
                    SliceActionImpl action = new SliceActionImpl(data.mIntentItem);
                    if (action != null) {
                        // Currently if we don't set icon for the SliceAction, slice lib will
                        // automatically treat it as a toggle. To distinguish preference action and
                        // toggle action, we need to add a subtype if this is a preference action.
                        preference = new SlicePreference(contextThemeWrapper);
                        ((SlicePreference) preference).setSliceAction(action);
                        ((SlicePreference) preference).setActionId(getActionId(item));
                        if (data.mFollowupIntentItem != null) {
                            SliceActionImpl followUpAction =
                                    new SliceActionImpl(data.mFollowupIntentItem);
                            ((SlicePreference) preference).setFollowupSliceAction(followUpAction);
                        }
                    }
                } else if (data.mEndItems.size() > 0 && data.mEndItems.get(0) != null) {
                    SliceActionImpl action = new SliceActionImpl(data.mEndItems.get(0));
                    if (action != null) {
                        int buttonStyle = SlicePreferencesUtil.getButtonStyle(item);
                        switch (buttonStyle) {
                            case CHECKMARK :
                                preference = new SliceCheckboxPreference(
                                        contextThemeWrapper, action);
                                break;
                            case SWITCH :
                                preference = new SliceSwitchPreference(contextThemeWrapper, action);
                                break;
                            case RADIO:
                                preference = new SliceRadioPreference(contextThemeWrapper, action);
                                preference.setLayoutResource(R.layout.preference_reversed_widget);
                                if (getRadioGroup(item) != null) {
                                    ((SliceRadioPreference) preference).setRadioGroup(
                                            getRadioGroup(item).toString());
                                }
                                break;
                        }
                        if (preference instanceof HasSliceAction) {
                            ((HasSliceAction) preference).setActionId(getActionId(item));
                        }
                        if (data.mFollowupIntentItem != null) {
                            SliceActionImpl followUpAction =
                                    new SliceActionImpl(data.mFollowupIntentItem);
                            ((HasSliceAction) preference).setFollowupSliceAction(followUpAction);

                        }
                    }
                }

                CharSequence uri = getText(data.mTargetSliceItem);
                if (uri == null || TextUtils.isEmpty(uri)) {
                    if (preference == null) {
                        preference = new Preference(contextThemeWrapper);
                    }
                } else {
                    if (preference == null) {
                        preference = new SlicePreference(contextThemeWrapper);
                    }
                    ((HasSliceUri) preference).setUri(uri.toString());
                    if (preference instanceof HasSliceAction) {
                        ((HasSliceAction) preference).setActionId(getActionId(item));
                    }
                    preference.setFragment(className);
                }
            } else if (item.getSubType().equals(SlicesConstants.TYPE_PREFERENCE_CATEGORY)) {
                preference = new PreferenceCategory(contextThemeWrapper);
            }
        }

        if (preference != null) {
            // Set whether preference is enabled.
            if (preference instanceof InfoPreference || !enabled(item)) {
                preference.setEnabled(false);
            }
            // Set whether preference is selectable
            if (!selectable(item)) {
                preference.setSelectable(false);
            }
            // Set the key for the preference
            CharSequence key = getKey(item);
            if (key != null) {
                preference.setKey(key.toString());
            }

            Icon icon = getIcon(data.mStartItem);
            if (icon != null) {
                boolean isIconNeedToBeProcessed =
                        SlicePreferencesUtil.isIconNeedsToBeProcessed(item);
                Drawable iconDrawable = icon.loadDrawable(contextThemeWrapper);
                if (isIconNeedToBeProcessed) {
                    preference.setIcon(IconUtil.getCompoundIcon(contextThemeWrapper, iconDrawable));
                } else {
                    preference.setIcon(iconDrawable);
                }
            }

            if (data.mTitleItem != null) {
                preference.setTitle(getText(data.mTitleItem));
            }

            //Set summary
            CharSequence subtitle =
                    data.mSubtitleItem != null ? data.mSubtitleItem.getText() : null;
            boolean subtitleExists = !TextUtils.isEmpty(subtitle)
                    || (data.mSubtitleItem != null && data.mSubtitleItem.hasHint(HINT_PARTIAL));
            if (subtitleExists) {
                preference.setSummary(subtitle);
            } else {
                if (data.mSummaryItem != null) {
                    preference.setSummary(getText(data.mSummaryItem));
                }
            }
            // Set preview info image and text
            CharSequence infoText = getInfoText(item);
            IconCompat infoImage = getInfoImage(item);
            Bundle b = preference.getExtras();
            if (infoImage != null) {
                b.putParcelable(EXTRA_PREFERENCE_INFO_IMAGE, infoImage.toIcon());
            }
            if (infoText != null) {
                b.putCharSequence(EXTRA_PREFERENCE_INFO_TEXT, infoText);
            }
            if (infoImage != null || infoText != null) {
                preference.setFragment(InfoFragment.class.getCanonicalName());
            }
        }

        return preference;
    }

    static class Data {
        SliceItem mStartItem;
        SliceItem mTitleItem;
        SliceItem mSubtitleItem;
        SliceItem mSummaryItem;
        SliceItem mTargetSliceItem;
        SliceItem mRadioGroupItem;
        SliceItem mIntentItem;
        SliceItem mFollowupIntentItem;
        List<SliceItem> mEndItems = new ArrayList<>();
        List<SliceItem> mInfoItems = new ArrayList<>();
    }

    static Data extract(SliceItem sliceItem) {
        Data data = new Data();
        List<SliceItem> possibleStartItems =
                SliceQuery.findAll(sliceItem, null, HINT_TITLE, null);
        if (possibleStartItems.size() > 0) {
            // The start item will be at position 0 if it exists
            String format = possibleStartItems.get(0).getFormat();
            if ((FORMAT_ACTION.equals(format)
                    && SliceQuery.find(possibleStartItems.get(0), FORMAT_IMAGE) != null)
                    || FORMAT_SLICE.equals(format)
                    || FORMAT_LONG.equals(format)
                    || FORMAT_IMAGE.equals(format)) {
                data.mStartItem = possibleStartItems.get(0);
            }
        }

        List<SliceItem> items = sliceItem.getSlice().getItems();
        for (int i = 0; i < items.size(); i++) {
            final SliceItem item = items.get(i);
            String subType = item.getSubType();
            if (subType != null) {
                switch (subType) {
                    case SlicesConstants.SUBTYPE_INFO_PREFERENCE :
                        data.mInfoItems.add(item);
                        break;
                    case SlicesConstants.SUBTYPE_INTENT :
                        data.mIntentItem = item;
                        break;
                    case SlicesConstants.SUBTYPE_FOLLOWUP_INTENT :
                        data.mFollowupIntentItem = item;
                        break;
                    case SlicesConstants.TAG_TARGET_URI :
                        data.mTargetSliceItem = item;
                        break;
                }
            } else if (FORMAT_TEXT.equals(item.getFormat()) && (item.getSubType() == null)) {
                if ((data.mTitleItem == null || !data.mTitleItem.hasHint(HINT_TITLE))
                        && item.hasHint(HINT_TITLE) && !item.hasHint(HINT_SUMMARY)) {
                    data.mTitleItem = item;
                } else if (data.mSubtitleItem == null && !item.hasHint(HINT_SUMMARY)) {
                    data.mSubtitleItem = item;
                } else if (data.mSummaryItem == null && item.hasHint(HINT_SUMMARY)) {
                    data.mSummaryItem = item;
                }
            } else {
                data.mEndItems.add(item);
            }
        }
        data.mEndItems.remove(data.mStartItem);
        return data;
    }

    private static List<Pair<CharSequence, CharSequence>> getInfoList(List<SliceItem> sliceItems) {
        List<Pair<CharSequence, CharSequence>> infoList = new ArrayList<>();
        for (SliceItem item : sliceItems) {
            Slice itemSlice = item.getSlice();
            if (itemSlice != null) {
                CharSequence title = null;
                CharSequence summary = null;
                for (SliceItem element : itemSlice.getItems()) {
                    if (element.getHints().contains(HINT_TITLE)) {
                        title = element.getText();
                    } else if (element.getHints().contains(HINT_SUMMARY)) {
                        summary = element.getText();
                    }
                }
                infoList.add(new Pair<CharSequence, CharSequence>(title, summary));
            }
        }
        return infoList;
    }

    private static CharSequence getKey(SliceItem item) {
        SliceItem target = SliceQuery.findSubtype(item, FORMAT_TEXT, SlicesConstants.TAG_KEY);
        return target != null ? target.getText() : null;
    }

    private static CharSequence getRadioGroup(SliceItem item) {
        SliceItem target = SliceQuery.findSubtype(
                item, FORMAT_TEXT, SlicesConstants.TAG_RADIO_GROUP);
        return target != null ? target.getText() : null;
    }

    /**
     * Get the screen title item for the slice.
     * @param sliceItems list of SliceItem extracted from slice data.
     * @return screen title item.
     */
    static SliceItem getScreenTitleItem(List<SliceContent> sliceItems) {
        for (SliceContent contentItem : sliceItems)  {
            SliceItem item = contentItem.getSliceItem();
            if (item.getSubType() != null
                    && item.getSubType().equals(SlicesConstants.TYPE_PREFERENCE_SCREEN_TITLE)) {
                return item;
            }
        }
        return null;
    }

    static SliceItem getFocusedPreferenceItem(List<SliceContent> sliceItems) {
        for (SliceContent contentItem : sliceItems)  {
            SliceItem item = contentItem.getSliceItem();
            if (item.getSubType() != null
                    && item.getSubType().equals(SlicesConstants.TYPE_FOCUSED_PREFERENCE)) {
                return item;
            }
        }
        return null;
    }

    static SliceItem getEmbeddedItem(List<SliceContent> sliceItems) {
        for (SliceContent contentItem : sliceItems)  {
            SliceItem item = contentItem.getSliceItem();
            if (item.getSubType() != null
                    && item.getSubType().equals(SlicesConstants.TYPE_PREFERENCE_EMBEDDED)) {
                return item;
            }
        }
        return null;
    }

    private static boolean isIconNeedsToBeProcessed(SliceItem sliceItem) {
        List<SliceItem> items = sliceItem.getSlice().getItems();
        for (SliceItem item : items)  {
            if (item.getSubType() != null && item.getSubType().equals(
                    SlicesConstants.SUBTYPE_ICON_NEED_TO_BE_PROCESSED)) {
                return item.getInt() == 1;
            }
        }
        return false;
    }

    private static int getButtonStyle(SliceItem sliceItem) {
        List<SliceItem> items = sliceItem.getSlice().getItems();
        for (SliceItem item : items)  {
            if (item.getSubType() != null
                    && item.getSubType().equals(SlicesConstants.SUBTYPE_BUTTON_STYLE)) {
                return item.getInt();
            }
        }
        return -1;
    }

    private static boolean enabled(SliceItem sliceItem) {
        List<SliceItem> items = sliceItem.getSlice().getItems();
        for (SliceItem item : items)  {
            if (item.getSubType() != null
                    && item.getSubType().equals(SlicesConstants.SUBTYPE_IS_ENABLED)) {
                return item.getInt() == 1;
            }
        }
        return true;
    }

    private static boolean selectable(SliceItem sliceItem) {
        List<SliceItem> items = sliceItem.getSlice().getItems();
        for (SliceItem item : items)  {
            if (item.getSubType() != null
                    && item.getSubType().equals(SlicesConstants.SUBTYPE_IS_SELECTABLE)) {
                return item.getInt() == 1;
            }
        }
        return true;
    }

    /**
     * Get the text from the SliceItem.
     */
    static CharSequence getText(SliceItem item) {
        if (item == null) {
            return null;
        }
        return item.getText();
    }

    /** Get the icon from the SlicItem if available */
    static Icon getIcon(SliceItem startItem) {
        if (startItem != null && startItem.getSlice() != null
                && startItem.getSlice().getItems() != null
                && startItem.getSlice().getItems().size() > 0) {
            SliceItem iconItem = startItem.getSlice().getItems().get(0);
            if (FORMAT_IMAGE.equals(iconItem.getFormat())) {
                IconCompat icon = iconItem.getIcon();
                return icon.toIcon();
            }
        }
        return null;
    }

    static Uri getStatusPath(String uriString) {
        Uri statusUri = Uri.parse(uriString)
                .buildUpon().path("/" + SlicesConstants.PATH_STATUS).build();
        return statusUri;
    }

    static int getPageId(SliceItem item) {
        SliceItem target = SliceQuery.findSubtype(item, FORMAT_INT, EXTRA_PAGE_ID);
        return target != null ? target.getInt() : 0;
    }

    private static int getActionId(SliceItem item) {
        SliceItem target = SliceQuery.findSubtype(item, FORMAT_INT, EXTRA_ACTION_ID);
        return target != null ? target.getInt() : 0;
    }


    private static CharSequence getInfoText(SliceItem item) {
        SliceItem target = SliceQuery.findSubtype(item, FORMAT_TEXT, EXTRA_PREFERENCE_INFO_TEXT);
        return target != null ? target.getText() : null;
    }

    private static IconCompat getInfoImage(SliceItem item) {
        SliceItem target = SliceQuery.findSubtype(item, FORMAT_IMAGE, EXTRA_PREFERENCE_INFO_IMAGE);
        return target != null ? target.getIcon() : null;
    }
}
