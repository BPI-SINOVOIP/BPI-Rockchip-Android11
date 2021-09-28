package com.android.documentsui;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.graphics.drawable.Drawable;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import com.android.documentsui.testing.TestProvidersAccess;

import org.junit.Before;
import org.junit.Test;

@SmallTest
public class IconUtilsTest {
    private static final String AUDIO_MIME_TYPE = "audio";
    private static final String IMAGE_MIME_TYPE = "image";
    private static final String TEXT_MIME_TYPE = "text";
    private static final String VIDEO_MIME_TYPE = "video";
    private static final String GENERIC_MIME_TYPE = "generic";

    private Context mTargetContext;

    @Before
    public void setUp() throws Exception {
        mTargetContext = InstrumentationRegistry.getTargetContext();
    }

    @Test
    public void testLoadMimeIcon_isAudioMimeType() {
        assertThat(IconUtils.loadMimeIcon(mTargetContext, AUDIO_MIME_TYPE)).isNotNull();
    }

    @Test
    public void testLoadMimeIcon_isImageMimeType() {
        assertThat(IconUtils.loadMimeIcon(mTargetContext, IMAGE_MIME_TYPE)).isNotNull();
    }

    @Test
    public void testLoadMimeIcon_isGenericMimeType() {
        assertThat(IconUtils.loadMimeIcon(mTargetContext, GENERIC_MIME_TYPE)).isNotNull();
    }

    @Test
    public void testLoadMimeIcon_isVideoMimeType() {
        assertThat(IconUtils.loadMimeIcon(mTargetContext, VIDEO_MIME_TYPE)).isNotNull();
    }

    @Test
    public void testLoadMimeIcon_isTextMimeType() {
        assertThat(IconUtils.loadMimeIcon(mTargetContext, TEXT_MIME_TYPE)).isNotNull();
    }

    @Test
    public void testLoadMimeIcon_isMimeTypeNull_shouldReturnNull() {
        assertThat(IconUtils.loadMimeIcon(mTargetContext, null)).isNull();
    }

    @Test
    public void testLoadPackageIcon() throws PackageManager.NameNotFoundException {
        final String authority = "a";
        final int icon = 1234;
        final ProviderInfo providerInfo = new ProviderInfo();

        Context context = mock(Context.class);
        PackageManager packageManager = mock(PackageManager.class);
        Drawable drawable = mock(Drawable.class);

        when(context.getPackageManager()).thenReturn(packageManager);
        when(packageManager.resolveContentProvider(eq(authority), anyInt())).thenReturn(
                providerInfo);
        when(packageManager.getDrawable(anyString(), eq(icon), any(ApplicationInfo.class)))
                .thenReturn(drawable);

        IconUtils.loadPackageIcon(context, TestProvidersAccess.USER_ID, authority, icon,
                /* maybeShowBadge= */false);

        verify(packageManager).getDrawable(any(), eq(icon), any());
        verify(packageManager, never()).getUserBadgedIcon(drawable,
                TestProvidersAccess.USER_HANDLE);
    }

    @Test
    public void testLoadPackageIcon_maybeShowBadge() throws PackageManager.NameNotFoundException {
        final String authority = "a";
        final int icon = 1234;
        final ProviderInfo providerInfo = new ProviderInfo();

        Context context = mock(Context.class);
        PackageManager packageManager = mock(PackageManager.class);
        Drawable drawable = mock(Drawable.class);

        when(context.getPackageManager()).thenReturn(packageManager);
        when(packageManager.resolveContentProvider(eq(authority), anyInt())).thenReturn(
                providerInfo);
        when(packageManager.getDrawable(any(), eq(icon), any())).thenReturn(drawable);

        IconUtils.loadPackageIcon(context, TestProvidersAccess.USER_ID, authority, icon,
                /* maybeShowBadge= */true);

        verify(packageManager).getDrawable(any(), eq(icon), any());
        verify(packageManager).getUserBadgedIcon(drawable, TestProvidersAccess.USER_HANDLE);
    }
}
