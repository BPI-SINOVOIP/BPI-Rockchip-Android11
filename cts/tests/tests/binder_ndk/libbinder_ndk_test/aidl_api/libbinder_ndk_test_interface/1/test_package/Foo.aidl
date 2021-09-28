package test_package;
parcelable Foo {
  String a = "FOO";
  int b = 42;
  float c = 3.140000f;
  test_package.Bar d;
  test_package.Bar e;
  int f = 3;
  test_package.ByteEnum shouldBeByteBar;
  test_package.IntEnum shouldBeIntBar;
  test_package.LongEnum shouldBeLongBar;
  test_package.ByteEnum[] shouldContainTwoByteFoos;
  test_package.IntEnum[] shouldContainTwoIntFoos;
  test_package.LongEnum[] shouldContainTwoLongFoos;
}
