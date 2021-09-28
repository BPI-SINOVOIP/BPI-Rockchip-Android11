package org.robolectric.shadows;

import java.lang.annotation.Annotation;
import com.android.internal.util.AnnotationValidations;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

@Implements(value = AnnotationValidations.class)
public class ShadowAnnotationValidations {
    @Implementation
    public static void validate(Class<? extends Annotation> annotation,
            Annotation ignored, int value) {
    }

    @Implementation
    public static void validate(Class<? extends Annotation> annotation,
            Annotation ignored, long value) {
    }
}
