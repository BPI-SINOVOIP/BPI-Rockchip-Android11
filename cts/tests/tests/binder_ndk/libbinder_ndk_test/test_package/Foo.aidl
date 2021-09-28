package test_package;

import test_package.Bar;
import test_package.ByteEnum;
import test_package.IntEnum;
import test_package.LongEnum;

parcelable Foo {
    String a="FOO";
    int b=42;
    float c=3.14f;
    Bar d;
    Bar e;
    int f=3;
    ByteEnum shouldBeByteBar;
    IntEnum shouldBeIntBar;
    LongEnum shouldBeLongBar;
    ByteEnum[] shouldContainTwoByteFoos;
    IntEnum[] shouldContainTwoIntFoos;
    LongEnum[] shouldContainTwoLongFoos;
    // This field doesn't exist in version 1.
    @nullable String[] g;
}
