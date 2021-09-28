package com.android.tv.settings.display;

import java.io.Serializable;
import java.util.Arrays;

/**
 * 显示信息
 *
 * @author GaoFei
 */
class DisplayInfo implements Serializable {
    private int displayId;
    private int type;
    private String description;
    private String[] modes;
    private String[] colors;
    private String[] orginModes;

    public int getDisplayId() {
        return displayId;
    }

    public void setDisplayId(int displayId) {
        this.displayId = displayId;
    }

    public int getType() {
        return type;
    }

    public void setType(int type) {
        this.type = type;
    }

    public String getDescription() {
        return description;
    }

    public void setDescription(String description) {
        this.description = description;
    }

    public String[] getModes() {
        return modes;
    }

    public String[] getColors() {
        return colors;
    }

    public void setModes(String[] modes) {
        this.modes = modes;
    }

    public void setColors(String[] colors) {
        this.colors = colors;
    }

    public String[] getOrginModes() {
        return orginModes;
    }

    public void setOrginModes(String[] orginModes) {
        this.orginModes = orginModes;
    }

    @Override
    public String toString() {
        StringBuilder builder = new StringBuilder();
        builder.append("displayId:").append(displayId).append("  ")
                .append("type:").append(type).append("  ")
                .append("description:").append(description).append("  ")
                .append("modes:").append(Arrays.toString(modes));
        return builder.toString();
    }
}
