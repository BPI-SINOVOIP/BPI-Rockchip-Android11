package org.robolectric.shadows;

import static org.robolectric.shadow.api.Shadow.newInstanceOf;

import android.app.ActivityThread;
import android.app.Application;
import android.appwidget.AppWidgetManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;
import android.os.PowerManager;
import android.widget.ListPopupWindow;
import android.widget.PopupWindow;
import android.widget.Toast;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.robolectric.RoboSettings;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.Scheduler;

@Implements(Application.class)
public class ShadowApplication extends ShadowContextWrapper {
  @RealObject private Application realApplication;

  private Scheduler backgroundScheduler = RoboSettings.isUseGlobalScheduler()
      ? getForegroundThreadScheduler()
      : new Scheduler();
  private List<android.widget.Toast> shownToasts = new ArrayList<>();
  private PowerManager.WakeLock latestWakeLock;
  private ShadowAlertDialog latestAlertDialog;
  private ShadowDialog latestDialog;
  private ShadowPopupMenu latestPopupMenu;
  private Object bluetoothAdapter = newInstanceOf("android.bluetooth.BluetoothAdapter");
  private PopupWindow latestPopupWindow;
  private ListPopupWindow latestListPopupWindow;

  /**
   * @deprecated Use
   *     `shadowOf({@link androidx.test.core.app.ApplicationProvider#getApplicationContext})`
   *     instead.
   */
  @Deprecated
  public static ShadowApplication getInstance() {
    return RuntimeEnvironment.application == null
        ? null
        : Shadow.extract(RuntimeEnvironment.application);
  }

  /**
   * Runs any background tasks previously queued by {@link android.os.AsyncTask#execute(Object[])}.
   *
   * Note: calling this method does not pause or un-pause the scheduler.
   */
  public static void runBackgroundTasks() {
    getInstance().getBackgroundThreadScheduler().advanceBy(0);
  }

  /**
   * Attaches an application to a base context.
   *
   * @param context The context with which to initialize the application, whose base context will
   *                be attached to the application
   */
  public void callAttach(Context context) {
    ReflectionHelpers.callInstanceMethod(Application.class, realApplication, "attach",
        ReflectionHelpers.ClassParameter.from(Context.class, context));
  }

  public List<Toast> getShownToasts() {
    return shownToasts;
  }

  /**
   * Return the foreground scheduler.
   *
   * @return  Foreground scheduler.
   */
  public Scheduler getForegroundThreadScheduler() {
    return RuntimeEnvironment.getMasterScheduler();
  }

  /**
   * Return the background scheduler.
   *
   * @return  Background scheduler.
   */
  public Scheduler getBackgroundThreadScheduler() {
    return backgroundScheduler;
  }

  public void setComponentNameAndServiceForBindService(ComponentName name, IBinder service) {
    getShadowInstrumentation().setComponentNameAndServiceForBindService(name, service);
  }

  public void setComponentNameAndServiceForBindServiceForIntent(
      Intent intent, ComponentName name, IBinder service) {
    getShadowInstrumentation()
        .setComponentNameAndServiceForBindServiceForIntent(intent, name, service);
  }

  public void assertNoBroadcastListenersOfActionRegistered(ContextWrapper context, String action) {
    getShadowInstrumentation().assertNoBroadcastListenersOfActionRegistered(context, action);
  }

  public List<ServiceConnection> getBoundServiceConnections() {
    return getShadowInstrumentation().getBoundServiceConnections();
  }

  public void setUnbindServiceShouldThrowIllegalArgument(boolean flag) {
    getShadowInstrumentation().setUnbindServiceShouldThrowIllegalArgument(flag);
  }

  public List<ServiceConnection> getUnboundServiceConnections() {
    return getShadowInstrumentation().getUnboundServiceConnections();
  }

  /** @deprecated use PackageManager.queryBroadcastReceivers instead */
  @Deprecated
  public boolean hasReceiverForIntent(Intent intent) {
    return getShadowInstrumentation().hasReceiverForIntent(intent);
  }

  /** @deprecated use PackageManager.queryBroadcastReceivers instead */
  @Deprecated
  public List<BroadcastReceiver> getReceiversForIntent(Intent intent) {
    return getShadowInstrumentation().getReceiversForIntent(intent);
  }

  /**
   * @return list of {@link Wrapper}s for registered receivers
   */
  public List<Wrapper> getRegisteredReceivers() {
    return getShadowInstrumentation().getRegisteredReceivers();
  }

  /**
   * @deprecated Please use {@link Context#getSystemService(Context.APPWIDGET_SERVICE)} intstead.
   */
  @Deprecated
  public AppWidgetManager getAppWidgetManager() {
    return (AppWidgetManager) realApplication.getSystemService(Context.APPWIDGET_SERVICE);
  }

  /**
   * @deprecated Use {@link ShadowAlertDialog#getLatestAlertDialog()} instead.
   */
  @Deprecated
  public ShadowAlertDialog getLatestAlertDialog() {
    return latestAlertDialog;
  }

  protected void setLatestAlertDialog(ShadowAlertDialog latestAlertDialog) {
    this.latestAlertDialog = latestAlertDialog;
  }

  /**
   * @deprecated Use {@link ShadowDialog#getLatestDialog()} instead.
   */
  @Deprecated
  public ShadowDialog getLatestDialog() {
    return latestDialog;
  }

  protected void setLatestDialog(ShadowDialog latestDialog) {
    this.latestDialog = latestDialog;
  }

  public Object getBluetoothAdapter() {
    return bluetoothAdapter;
  }

  public void declareActionUnbindable(String action) {
    getShadowInstrumentation().declareActionUnbindable(action);
  }

  public PowerManager.WakeLock getLatestWakeLock() {
    return latestWakeLock;
  }

  public void addWakeLock( PowerManager.WakeLock wl ) {
    latestWakeLock = wl;
  }

  public void clearWakeLocks() {
    latestWakeLock = null;
  }

  private final Map<String, Object> singletons = new HashMap<>();

  public <T> T getSingleton(Class<T> clazz, Provider<T> provider) {
    synchronized (singletons) {
      //noinspection unchecked
      T item = (T) singletons.get(clazz.getName());
      if (item == null) {
        singletons.put(clazz.getName(), item = provider.get());
      }
      return item;
    }
  }

  /**
   * Set to true if you'd like Robolectric to strictly simulate the real Android behavior when
   * calling {@link Context#startActivity(android.content.Intent)}. Real Android throws a
   * {@link android.content.ActivityNotFoundException} if given
   * an {@link Intent} that is not known to the {@link android.content.pm.PackageManager}
   *
   * By default, this behavior is off (false).
   *
   * @param checkActivities True to validate activities.
   */
  public void checkActivities(boolean checkActivities) {
    ActivityThread activityThread = (ActivityThread) RuntimeEnvironment.getActivityThread();
    ShadowInstrumentation shadowInstrumentation =
        Shadow.extract(activityThread.getInstrumentation());
    shadowInstrumentation.checkActivities(checkActivities);
  }

  /**
   * @deprecated Use {@link ShadowPopupMenu#getLatestPopupMenu()} instead.
   */
  @Deprecated
  public ShadowPopupMenu getLatestPopupMenu() {
    return latestPopupMenu;
  }

  protected void setLatestPopupMenu(ShadowPopupMenu latestPopupMenu) {
    this.latestPopupMenu = latestPopupMenu;
  }

  public PopupWindow getLatestPopupWindow() {
    return latestPopupWindow;
  }

  protected void setLatestPopupWindow(PopupWindow latestPopupWindow) {
    this.latestPopupWindow = latestPopupWindow;
  }

  public ListPopupWindow getLatestListPopupWindow() {
    return latestListPopupWindow;
  }

  protected void setLatestListPopupWindow(ListPopupWindow latestListPopupWindow) {
    this.latestListPopupWindow = latestListPopupWindow;
  }

  public static class Wrapper {
    public BroadcastReceiver broadcastReceiver;
    public IntentFilter intentFilter;
    public Context context;
    public Throwable exception;
    public String broadcastPermission;
    public Handler scheduler;

    public Wrapper(
        BroadcastReceiver broadcastReceiver,
        IntentFilter intentFilter,
        Context context,
        String broadcastPermission,
        Handler scheduler) {
      this.broadcastReceiver = broadcastReceiver;
      this.intentFilter = intentFilter;
      this.context = context;
      this.broadcastPermission = broadcastPermission;
      this.scheduler = scheduler;
      exception = new Throwable();
    }

    public BroadcastReceiver getBroadcastReceiver() {
      return broadcastReceiver;
    }

    public IntentFilter getIntentFilter() {
      return intentFilter;
    }

    public Context getContext() {
      return context;
    }
  }

  /**
   * @deprecated Do not depend on this method to override services as it will be removed in a future
   * update. The preferered method is use the shadow of the corresponding service.
   */
  @Deprecated
  public void setSystemService(String key, Object service) {
    ShadowContextImpl shadowContext = Shadow.extract(realApplication.getBaseContext());
    shadowContext.setSystemService(key, service);
  }

}
