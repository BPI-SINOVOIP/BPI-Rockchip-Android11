package android.platform.helpers;

public interface IAutoNotificationMockingHelper extends INotificationHelper, IAppHelper {
    /**
     * Setup expectations: No.
     *
     * <p>Clear all notifications generated.
     */
    void clearAllNotification();
}
