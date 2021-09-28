/*
 * Copyright (C) 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.aidl.tests;

import android.aidl.tests.map.Bar;
import android.aidl.tests.map.Foo;
import android.aidl.tests.map.IEmpty;
import android.os.IBinder;
import android.os.Parcel;
import java.util.HashMap;

class MapTests {
  private TestLogger mLog;

  public MapTests(TestLogger logger) { mLog = logger; }

  public void checkMap() throws TestFailException {
    mLog.log("Checking if data in a Map object is transferred well.");
    Parcel parcel = Parcel.obtain();
    IEmpty intf = new IEmpty.Stub() {};
    {
      Foo foo = new Foo();
      Bar bar = new Bar();
      bar.a = 42;
      bar.b = "Bar";
      foo.barMap = new HashMap<>();
      foo.barMap.put("Foo", bar);

      foo.stringMap = new HashMap<>();
      foo.stringMap.put("Foo", "Bar");

      foo.interfaceMap = new HashMap<>();
      foo.interfaceMap.put("Foo", intf);

      foo.ibinderMap = new HashMap<>();
      foo.ibinderMap.put("Foo", intf.asBinder());

      foo.writeToParcel(parcel, 0);
    }
    parcel.setDataPosition(0);
    {
      Foo foo = new Foo();
      foo.readFromParcel(parcel);
      if (!foo.barMap.containsKey("Foo")) {
        mLog.logAndThrow("Map foo.a must have the element of which key is \"Foo\"");
      }
      if (foo.barMap.size() != 1) {
        mLog.logAndThrow("The size of map a is expected to be 1.");
      }
      Bar bar = foo.barMap.get("Foo");
      if (bar.a != 42 || !"Bar".equals(bar.b)) {
        mLog.logAndThrow("The content of bar is expected to be {a: 42, b: \"Bar\"}.");
      }

      if (foo.stringMap.size() != 1) {
        mLog.logAndThrow("The size of map a is expected to be 1.");
      }
      String string = foo.stringMap.get("Foo");
      if (!"Bar".equals(string)) {
        mLog.logAndThrow("The content of string is expected to be \"Bar\".");
      }

      if (foo.interfaceMap.size() != 1) {
        mLog.logAndThrow("The size of map a is expected to be 1.");
      }

      if (!intf.equals(foo.interfaceMap.get("Foo"))) {
        mLog.logAndThrow("The content of service is expected to be same.");
      }

      if (foo.ibinderMap.size() != 1) {
        mLog.logAndThrow("The size of map a is expected to be 1.");
      }
      IBinder ibinder = foo.ibinderMap.get("Foo");
      if (!intf.asBinder().equals(ibinder)) {
        mLog.logAndThrow("The content of IBinder is expected to be same.");
      }
    }
  }

  public void runTests() throws TestFailException { checkMap(); }
}
