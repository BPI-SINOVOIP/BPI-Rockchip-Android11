package com.android.documentsui.files;

import static com.google.common.truth.Truth.assertThat;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.QuickViewConstants;
import android.content.pm.PackageManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.testing.TestEnv;
import com.android.documentsui.testing.TestPackageManager;
import com.android.documentsui.testing.TestProvidersAccess;
import com.android.documentsui.testing.TestResources;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class QuickViewIntentBuilderTest {

    private static String mTargetPackageName;
    private Context mContext = mock(Context.class);
    private PackageManager mPm;
    private TestEnv mEnv;
    private TestResources mRes;

    @Before
    public void setUp() {
        mTargetPackageName =
                InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName();
        mPm = TestPackageManager.create();
        when(mContext.getPackageManager()).thenReturn(mPm);
        mEnv = TestEnv.create();
        mRes = TestResources.create();

        mRes.setQuickViewerPackage(mTargetPackageName);
    }

    @Test
    public void testSetsNoFeatures_InArchiveDocument() {
        QuickViewIntentBuilder builder =
                new QuickViewIntentBuilder(
                        mContext, mRes, TestEnv.FILE_IN_ARCHIVE, mEnv.archiveModel, false);

        Intent intent = builder.build();

        String[] features = intent.getStringArrayExtra(Intent.EXTRA_QUICK_VIEW_FEATURES);
        assertEquals(0, features.length);
    }

    @Test
    public void testSetsFullFeatures_RegularDocument() {
        QuickViewIntentBuilder builder =
                new QuickViewIntentBuilder(mContext, mRes, TestEnv.FILE_JPG, mEnv.model, false);

        Intent intent = builder.build();

        Set<String> features = new HashSet<>(
                Arrays.asList(intent.getStringArrayExtra(Intent.EXTRA_QUICK_VIEW_FEATURES)));

        assertEquals("Unexpected features set: " + features, 6, features.size());
        assertTrue(features.contains(QuickViewConstants.FEATURE_VIEW));
        assertTrue(features.contains(QuickViewConstants.FEATURE_EDIT));
        assertTrue(features.contains(QuickViewConstants.FEATURE_DELETE));
        assertTrue(features.contains(QuickViewConstants.FEATURE_SEND));
        assertTrue(features.contains(QuickViewConstants.FEATURE_DOWNLOAD));
        assertTrue(features.contains(QuickViewConstants.FEATURE_PRINT));
    }

    @Test
    public void testPickerFeatures_RegularDocument() {

        QuickViewIntentBuilder builder =
                new QuickViewIntentBuilder(mContext, mRes, TestEnv.FILE_JPG, mEnv.model, true);

        Intent intent = builder.build();

        Set<String> features = new HashSet<>(
                Arrays.asList(intent.getStringArrayExtra(Intent.EXTRA_QUICK_VIEW_FEATURES)));

        assertEquals("Unexpected features set: " + features, 1, features.size());
        assertTrue(features.contains(QuickViewConstants.FEATURE_VIEW));
    }

    @Test
    public void testBuild() {
        mEnv.model.reset();
        DocumentInfo previewDoc = mEnv.model.createFile("a.png", 0);
        mEnv.model.createFile("b.png", 0);
        mEnv.model.createFile("c.png", 0);
        mEnv.model.update();

        QuickViewIntentBuilder builder =
                new QuickViewIntentBuilder(mContext, mRes, previewDoc, mEnv.model, true);

        Intent intent = builder.build();
        ClipData clip = intent.getClipData();
        assertThat(clip.getItemAt(intent.getIntExtra(Intent.EXTRA_INDEX, -1)).getUri())
                .isEqualTo(previewDoc.getDocumentUri());
        assertThat(clip.getItemCount()).isEqualTo(3);
    }

    @Test
    public void testBuild_excludeFolder() {
        mEnv.model.reset();
        mEnv.model.createFolder("folder");
        mEnv.model.createFolder("does not count");
        mEnv.model.createFile("a.png", 0);
        DocumentInfo previewDoc = mEnv.model.createFile("b.png", 0);
        mEnv.model.createFile("c.png", 0);
        mEnv.model.update();

        QuickViewIntentBuilder builder =
                new QuickViewIntentBuilder(mContext, mRes, previewDoc, mEnv.model, true);

        Intent intent = builder.build();
        ClipData clip = intent.getClipData();
        assertThat(clip.getItemAt(intent.getIntExtra(Intent.EXTRA_INDEX, -1)).getUri())
                .isEqualTo(previewDoc.getDocumentUri());
        assertThat(clip.getItemCount()).isEqualTo(3);
    }

    @Test
    public void testBuild_twoProfiles_containsOnlyPreviewDocument() {
        mEnv.model.reset();
        mEnv.model.createDocumentForUser("a.txt", "text/plain", 0,
                TestProvidersAccess.OtherUser.USER_ID);
        DocumentInfo previewDoc = mEnv.model.createFile("b.png", 0);
        mEnv.model.createFile("c.png", 0);
        mEnv.model.update();

        QuickViewIntentBuilder builder =
                new QuickViewIntentBuilder(mContext, mRes, previewDoc, mEnv.model, true);

        Intent intent = builder.build();
        ClipData clip = intent.getClipData();
        assertThat(clip.getItemAt(intent.getIntExtra(Intent.EXTRA_INDEX, -1)).getUri())
                .isEqualTo(previewDoc.getDocumentUri());
        assertThat(clip.getItemCount()).isEqualTo(1);
    }
}
