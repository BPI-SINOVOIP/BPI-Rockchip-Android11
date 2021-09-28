package com.android.settings.utils;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

import android.util.Log;

/**
 * 反射工具
 *
 * @author GaoFei
 */
public class ReflectUtils {
    private static final String TAG = "ReflectUtils";

    public static void setFieldValue(Object targetObject, String filedName, Object filedvalue) {
        try {
            Field field = targetObject.getClass().getDeclaredField(filedName);
            field.setAccessible(true);
            field.set(targetObject, filedvalue);
        } catch (Exception e) {
            Log.e(TAG, "setFieldValue->filedName:" + filedName);
            Log.e(TAG, "setFieldValue->value:" + filedvalue);
            Log.e(TAG, "setFieldValue->exception:" + e);
        }

    }

    public static Object getFieldValue(Object targetObject, String filedName) {
        try {
            Field field = targetObject.getClass().getDeclaredField(filedName);
            field.setAccessible(true);
            return field.get(targetObject);
        } catch (Exception e) {
            Log.e(TAG, "getFieldValue->filedName:" + filedName);
            Log.e(TAG, "getFieldValue->exception:" + e);
        }
        return null;
    }

    public static Object invokeMethod(Object object, String methodName, Class<?>[] paramTypes, Object[] values) {
        try {
            Method method = object.getClass().getDeclaredMethod(methodName, paramTypes);
            method.setAccessible(true);
            return method.invoke(object, values);
            //return method.invoke(object, paramTypes);
        } catch (Exception e) {
            Log.e(TAG, "invokeMethod->methodName:" + methodName);
            Log.e(TAG, "invokeMethod->exception:" + e);

        }
        return null;
    }

    public static Object invokeMethodNoParameter(Object object, String methodName) {
        try {
            Method method = object.getClass().getDeclaredMethod(methodName);
            method.setAccessible(true);
            return method.invoke(object);
            //return method.invoke(object, paramTypes);
        } catch (Exception e) {
            Log.e(TAG, "invokeMethod->methodName:" + methodName);
            Log.e(TAG, "invokeMethod->exception:" + e);

        }
        return null;
    }
}
