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

package com.android.compatibility.common.util;

import static android.text.TextUtils.isEmpty;

import android.app.Instrumentation;
import android.app.UiAutomation;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.view.accessibility.AccessibilityWindowInfo;

import androidx.test.InstrumentationRegistry;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.function.BiFunction;
import java.util.function.BiPredicate;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.ToIntFunction;
import java.util.stream.Stream;

/**
 * Utilities to dump the view hierrarchy as an indented tree
 *
 * @see #dumpNodes(AccessibilityNodeInfo, StringBuilder)
 * @see #wrapWithUiDump(Throwable)
 */
@SuppressWarnings({"PointlessBitwiseExpression"})
public class UiDumpUtils {
    private UiDumpUtils() {}

    private static final boolean CONCISE = false;
    private static final boolean SHOW_ACTIONS = false;
    private static final boolean IGNORE_INVISIBLE = false;

    private static final int IGNORED_ACTIONS = 0
            | AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS
            | AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS
            | AccessibilityNodeInfo.ACTION_FOCUS
            | AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY
            | AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY
            | AccessibilityNodeInfo.ACTION_SELECT
            | AccessibilityNodeInfo.ACTION_SET_SELECTION
            | AccessibilityNodeInfo.ACTION_CLEAR_SELECTION
            | AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT
            | AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT
            ;

    private static final int SPECIALLY_HANDLED_ACTIONS = 0
            | AccessibilityNodeInfo.ACTION_CLICK
            | AccessibilityNodeInfo.ACTION_LONG_CLICK
            | AccessibilityNodeInfo.ACTION_EXPAND
            | AccessibilityNodeInfo.ACTION_COLLAPSE
            | AccessibilityNodeInfo.ACTION_FOCUS
            | AccessibilityNodeInfo.ACTION_CLEAR_FOCUS
            | AccessibilityNodeInfo.ACTION_SCROLL_FORWARD
            | AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD
            | AccessibilityNodeInfo.ACTION_SET_TEXT
            ;

    /** name -> typical_value */
    private static Map<String, Boolean> sNodeFlags = new LinkedHashMap<>();
    static {
        sNodeFlags.put("focused", false);
        sNodeFlags.put("selected", false);
        sNodeFlags.put("contextClickable", false);
        sNodeFlags.put("dismissable", false);
        sNodeFlags.put("enabled", true);
        sNodeFlags.put("password", false);
        sNodeFlags.put("visibleToUser", true);
        sNodeFlags.put("contentInvalid", false);
        sNodeFlags.put("heading", false);
        sNodeFlags.put("showingHintText", false);

        // Less important flags below
        // Too spammy to report all, but can uncomment what's necessary

//        sNodeFlags.put("focusable", true);
//        sNodeFlags.put("accessibilityFocused", false);
//        sNodeFlags.put("screenReaderFocusable", true);
//        sNodeFlags.put("clickable", false);
//        sNodeFlags.put("longClickable", false);
//        sNodeFlags.put("checkable", false);
//        sNodeFlags.put("checked", false);
//        sNodeFlags.put("editable", false);
//        sNodeFlags.put("scrollable", false);
//        sNodeFlags.put("importantForAccessibility", true);
//        sNodeFlags.put("multiLine", false);
    }

    /** action -> pictogram */
    private static Map<AccessibilityAction, String> sNodeActions = new LinkedHashMap<>();
    static {
        sNodeActions.put(AccessibilityAction.ACTION_PASTE, "\uD83D\uDCCB");
        sNodeActions.put(AccessibilityAction.ACTION_CUT, "✂");
        sNodeActions.put(AccessibilityAction.ACTION_COPY, "⎘");
        sNodeActions.put(AccessibilityAction.ACTION_SCROLL_BACKWARD, "←");
        sNodeActions.put(AccessibilityAction.ACTION_SCROLL_LEFT, "←");
        sNodeActions.put(AccessibilityAction.ACTION_SCROLL_FORWARD, "→");
        sNodeActions.put(AccessibilityAction.ACTION_SCROLL_RIGHT, "→");
        sNodeActions.put(AccessibilityAction.ACTION_SCROLL_DOWN, "↓");
        sNodeActions.put(AccessibilityAction.ACTION_SCROLL_UP, "↑");
    }

    private static Instrumentation sInstrumentation = InstrumentationRegistry.getInstrumentation();
    private static UiAutomation sUiAutomation = sInstrumentation.getUiAutomation();

    private static int sScreenArea;
    static {
        Point displaySize = new Point();
        sInstrumentation.getContext()
                .getSystemService(WindowManager.class)
                .getDefaultDisplay()
                .getRealSize(displaySize);
        sScreenArea = displaySize.x * displaySize.y;
    }


    /**
     * Wraps the given exception, with one containing UI hierrarchy {@link #dumpNodes dump}
     * in its message.
     *
     * <p>
     * Can be used together with {@link ExceptionUtils#wrappingExceptions}, e.g:
     * {@code
     *     ExceptionUtils.wrappingExceptions(UiDumpUtils::wrapWithUiDump, () -> {
     *         // UI-testing code
     *     });
     * }
     */
    public static UiDumpWrapperException wrapWithUiDump(Throwable cause) {
        return (cause instanceof UiDumpWrapperException)
                ? (UiDumpWrapperException) cause
                : new UiDumpWrapperException(cause);
    }

    /**
     * Dumps UI hierarchy with a given {@code root} as indented text tree into {@code out}.
     */
    public static void dumpNodes(AccessibilityNodeInfo root, StringBuilder out) {
        if (root == null) {
            appendNode(out, root);
            return;
        }

        out.append("--- ").append(root.getPackageName()).append(" ---\n|");

        recursively(root, AccessibilityNodeInfo::getChildCount, AccessibilityNodeInfo::getChild,
                node -> {
                    if (appendNode(out, node)) {
                        out.append("\n|");
                    }
                },
                action -> node -> {
                    out.append("  ");
                    action.accept(node);
                });
    }

    private static <T> void recursively(T node,
            ToIntFunction<T> getChildCount, BiFunction<T, Integer, T> getChildAt,
            Consumer<T> action, Function<Consumer<T>, Consumer<T>> actionChange) {
        if (node == null) return;

        action.accept(node);
        Consumer<T> childAction = actionChange.apply(action);

        int size = getChildCount.applyAsInt(node);
        for (int i = 0; i < size; i++) {
            recursively(getChildAt.apply(node, i),
                    getChildCount, getChildAt, childAction, actionChange);
        }
    }

    private static StringBuilder appendWindow(AccessibilityWindowInfo window, StringBuilder out) {
        if (window == null) {
            out.append("<null window>");
        } else {
            if (!isEmpty(window.getTitle())) {
                out.append(window.getTitle());
                if (CONCISE) return out;
                out.append(" ");
            }
            out.append(valueToString(
                    AccessibilityWindowInfo.class, "TYPE_", window.getType())).append(" ");
            if (CONCISE) return out;
            appendArea(out, window::getBoundsInScreen);

            Rect bounds = new Rect();
            window.getBoundsInScreen(bounds);
            out.append(bounds.width()).append("x").append(bounds.height()).append(" ");
            if (window.isInPictureInPictureMode()) out.append("#PIP ");
        }
        return out;
    }

    private static void appendArea(StringBuilder out, Consumer<Rect> getBoundsInScreen) {
        Rect rect = new Rect();
        getBoundsInScreen.accept(rect);
        out.append("size:");
        out.append(toStringRounding((float) area(rect) * 100 / sScreenArea)).append("% ");
    }

    private static boolean appendNode(StringBuilder out, AccessibilityNodeInfo node) {
        if (node == null) {
            out.append("<null node>");
            return true;
        }

        if (IGNORE_INVISIBLE && !node.isVisibleToUser()) return false;

        boolean markedClickable = false;
        boolean markedNonFocusable = false;

        try {
            if (node.isFocused() || node.isAccessibilityFocused()) {
                out.append(">");
            }

            if ((node.getActions() & AccessibilityNodeInfo.ACTION_EXPAND) != 0) {
                out.append("[+] ");
            }
            if ((node.getActions() & AccessibilityNodeInfo.ACTION_COLLAPSE) != 0) {
                out.append("[-] ");
            }

            CharSequence txt = node.getText();
            if (node.isCheckable()) {
                out.append("[").append(node.isChecked() ? "X" : "_").append("] ");
            } else if (node.isEditable()) {
                if (txt == null) txt = "";
                out.append("[");
                appendTextWithCursor(out, node, txt);
                out.append("] ");
            } else if (node.isClickable()) {
                markedClickable = true;
                out.append("[");
            } else if (!node.isImportantForAccessibility()) {
                markedNonFocusable = true;
                out.append("(");
            }

            if (appendNodeText(out, node)) return true;
        } finally {
            backspaceIf(' ', out);
            if (markedClickable) {
                out.append("]");
                if (node.isLongClickable()) out.append("+");
                out.append(" ");
            }
            if (markedNonFocusable) out.append(") ");

            if (CONCISE) out.append(" ");

            for (Map.Entry<String, Boolean> prop : sNodeFlags.entrySet()) {
                boolean value = call(node, boolGetter(prop.getKey()));
                if (value != prop.getValue()) {
                    out.append("#");
                    if (!value) out.append("not_");
                    out.append(prop.getKey()).append(" ");
                }
            }

            if (SHOW_ACTIONS) {
                LinkedHashSet<String> symbols = new LinkedHashSet<>();
                for (AccessibilityAction accessibilityAction : node.getActionList()) {
                    String symbol = sNodeActions.get(accessibilityAction);
                    if (symbol != null) symbols.add(symbol);
                }
                merge(symbols, "←", "→", "↔");
                merge(symbols, "↑", "↓", "↕");
                symbols.forEach(out::append);
                if (!symbols.isEmpty()) out.append(" ");

                getActions(node)
                        .map(a -> "[" + actionToString(a) + "] ")
                        .forEach(out::append);
            }

            Bundle extras = node.getExtras();
            for (String extra : extras.keySet()) {
                if (extra.equals("AccessibilityNodeInfo.chromeRole")) continue;
                if (extra.equals("AccessibilityNodeInfo.roleDescription")) continue;
                String value = "" + extras.get(extra);
                if (value.isEmpty()) continue;
                out.append(extra).append(":").append(value).append(" ");
            }
        }
        return true;
    }

    private static StringBuilder appendTextWithCursor(StringBuilder out, AccessibilityNodeInfo node,
            CharSequence txt) {
        out.append(txt);
        insertAtEnd(out, txt.length() - 1 - node.getTextSelectionStart(), "ꕯ");
        if (node.getTextSelectionEnd() != node.getTextSelectionStart()) {
            insertAtEnd(out, txt.length() - 1 - node.getTextSelectionEnd(), "ꕯ");
        }
        return out;
    }

    private static boolean appendNodeText(StringBuilder out, AccessibilityNodeInfo node) {
        CharSequence txt = node.getText();

        Bundle extras = node.getExtras();
        if (extras.containsKey("AccessibilityNodeInfo.roleDescription")) {
            out.append("<").append(extras.getString("AccessibilityNodeInfo.chromeRole"))
                    .append("> ");
        } else if (extras.containsKey("AccessibilityNodeInfo.chromeRole")) {
            out.append("<").append(extras.getString("AccessibilityNodeInfo.chromeRole"))
                    .append("> ");
        }

        if (CONCISE) {
            if (!isEmpty(node.getContentDescription())) {
                out.append(escape(node.getContentDescription()));
                return true;
            }
            if (!isEmpty(node.getPaneTitle())) {
                out.append(escape(node.getPaneTitle()));
                return true;
            }
            if (!isEmpty(txt) && !node.isEditable()) {
                out.append('"');
                if (node.getTextSelectionStart() > 0 || node.getTextSelectionEnd() > 0) {
                    appendTextWithCursor(out, node, txt);
                } else {
                    out.append(escape(txt));
                }
                out.append('"');
                return true;
            }
            if (!isEmpty(node.getViewIdResourceName())) {
                out.append("@").append(fromLast("/", node.getViewIdResourceName()));
                return true;
            }
        }

        if (node.getParent() == null && node.getWindow() != null) {
            appendWindow(node.getWindow(), out);
            if (CONCISE) return true;
            out.append(" ");
        }

        if (!extras.containsKey("AccessibilityNodeInfo.chromeRole")) {
            out.append(fromLast(".", node.getClassName())).append(" ");
        }
        ifNotEmpty(node.getViewIdResourceName(),
                s -> out.append("@").append(fromLast("/", s)).append(" "));
        ifNotEmpty(node.getPaneTitle(), s -> out.append("## ").append(s).append(" "));
        ifNotEmpty(txt, s -> out.append("\"").append(s).append("\" "));

        ifNotEmpty(node.getContentDescription(), s -> out.append("//").append(s).append(" "));

        appendArea(out, node::getBoundsInScreen);
        return false;
    }

    private static <T> String valueToString(Class<?> clazz, String prefix, T value) {
        String s = flagsToString(clazz, prefix, value, Objects::equals);
        if (s.isEmpty()) s = "" + value;
        return s;
    }

    private static <T> String flagsToString(Class<?> clazz, String prefix, T flags,
            BiPredicate<T, T> test) {
        return mkStr(sb -> {
            consts(clazz, prefix)
                    .filter(f -> box(f.getType()).isInstance(flags))
                    .forEach(c -> {
                        try {
                            if (test.test(flags, read(null, c))) {
                                sb.append(c.getName().substring(prefix.length())).append("|");
                            }
                        } catch (Exception e) {
                            throw new RuntimeException("Error while dealing with " + c, e);
                        }
                    });
            backspace(sb);
        });
    }

    private static Class box(Class c) {
        return c == int.class ? Integer.class : c;
    }

    private static Stream<Field> consts(Class<?> clazz, String prefix) {
        return Arrays.stream(clazz.getDeclaredFields())
                .filter(f -> isConst(f) && f.getName().startsWith(prefix));
    }

    private static boolean isConst(Field f) {
        return Modifier.isStatic(f.getModifiers()) && Modifier.isFinal(f.getModifiers());
    }

    private static Character last(StringBuilder sb) {
        return sb.length() == 0 ? null : sb.charAt(sb.length() - 1);
    }

    private static StringBuilder backspaceIf(char c, StringBuilder sb) {
        if (Objects.equals(last(sb), c)) backspace(sb);
        return sb;
    }

    private static StringBuilder backspace(StringBuilder sb) {
        if (sb.length() != 0) {
            sb.deleteCharAt(sb.length() - 1);
        }
        return sb;
    }

    private static String toStringRounding(float f) {
        return f >= 5.0 ? "" + (int) f : String.format("%.1f", f);
    }

    private static int area(Rect r) {
        return Math.abs((r.right - r.left) * (r.bottom - r.top));
    }

    private static String escape(CharSequence s) {
        return mkStr(out -> {
            for (int i = 0; i < s.length(); i++) {
                char c = s.charAt(i);
                if (c < 127 || c == 0xa0 || c >= 0x2000 && c < 0x2070) {
                    out.append(c);
                } else {
                    out.append("\\u").append(Integer.toHexString(c));
                }
            }
        });
    }

    private static Stream<AccessibilityAction> getActions(
            AccessibilityNodeInfo node) {
        if (node == null) return Stream.empty();
        return node.getActionList().stream()
                .filter(a -> !AccessibilityAction.ACTION_SHOW_ON_SCREEN.equals(a)
                        && (a.getId()
                                & ~IGNORED_ACTIONS
                                & ~SPECIALLY_HANDLED_ACTIONS
                            ) != 0);
    }

    private static String actionToString(AccessibilityAction a) {
        if (!isEmpty(a.getLabel())) return a.getLabel().toString();
        return valueToString(AccessibilityAction.class, "ACTION_", a);
    }

    private static void merge(Set<String> symbols, String a, String b, String ab) {
        if (symbols.contains(a) && symbols.contains(b)) {
            symbols.add(ab);
            symbols.remove(a);
            symbols.remove(b);
        }
    }

    private static String fromLast(String substr, CharSequence whole) {
        String wholeStr = whole.toString();
        int idx = wholeStr.lastIndexOf(substr);
        if (idx < 0) return wholeStr;
        return wholeStr.substring(idx + substr.length());
    }

    private static String boolGetter(String propName) {
        return "is" + Character.toUpperCase(propName.charAt(0)) + propName.substring(1);
    }

    private static <T> T read(Object o, Field f) {
        try {
            f.setAccessible(true);
            return (T) f.get(o);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static <T> T call(Object o, String methodName, Object... args) {
        Class clazz = o instanceof Class ? (Class) o : o.getClass();
        try {
            Method method = clazz.getDeclaredMethod(methodName, mapToTypes(args));
            method.setAccessible(true);
            //noinspection unchecked
            return (T) method.invoke(o, args);
        } catch (NoSuchMethodException e) {
            throw new RuntimeException(
                    newlineSeparated(Arrays.asList(clazz.getDeclaredMethods())), e);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static Class[] mapToTypes(Object[] args) {
        return Arrays.stream(args).map(Object::getClass).toArray(Class[]::new);
    }

    private static void ifNotEmpty(CharSequence t, Consumer<CharSequence> f) {
        if (!isEmpty(t)) {
            f.accept(t);
        }
    }

    private static StringBuilder insertAtEnd(StringBuilder sb, int pos, String s) {
        return sb.insert(sb.length() - 1 - pos, s);
    }

    private static <T, R> R fold(List<T> l, R init, BiFunction<R, T, R> combine) {
        R result = init;
        for (T t : l) {
            result = combine.apply(result, t);
        }
        return result;
    }

    private static <T> String toString(List<T> l, String sep, Function<T, String> elemToStr) {
        return fold(l, "", (a, b) -> a + sep + elemToStr.apply(b));
    }

    private static <T> String toString(List<T> l, String sep) {
        return toString(l, sep, String::valueOf);
    }

    private static String newlineSeparated(List<?> l) {
        return toString(l, "\n");
    }

    private static String mkStr(Consumer<StringBuilder> build) {
        StringBuilder t = new StringBuilder();
        build.accept(t);
        return t.toString();
    }

    private static class UiDumpWrapperException extends RuntimeException {
        private UiDumpWrapperException(Throwable cause) {
            super(cause.getMessage() + "\n\nWhile displaying the following UI:\n"
                    + mkStr(sb -> dumpNodes(sUiAutomation.getRootInActiveWindow(), sb)), cause);
        }
    }
}
