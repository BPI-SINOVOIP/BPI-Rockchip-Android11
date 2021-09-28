/*
 * Copyright (C) 2019 The Android Open Source Project
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

/*
 * This file is auto-generated in Android Studio for BinderExploitTest.  DO NOT MODIFY.
 */
package android.security.cts;

public interface IBinderExchange extends android.os.IInterface {
    /** Local-side IPC implementation stub class. */
    public static abstract class Stub extends android.os.Binder
        implements android.security.cts.IBinderExchange {
        private static final java.lang.String DESCRIPTOR = "android.security.cts.IBinderExchange";

        /** Construct the stub at attach it to the interface. */
        public Stub() {
            this.attachInterface(this, DESCRIPTOR);
        }

        /**
         * Cast an IBinder object into an android.security.cts.IBinderExchange
         * interface, generating a proxy if needed.
         */
        public static android.security.cts.IBinderExchange asInterface(android.os.IBinder obj) {
            if ((obj == null)) {
                return null;
            }
            android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (((iin != null) && (iin instanceof android.security.cts.IBinderExchange))) {
                return ((android.security.cts.IBinderExchange) iin);
            }
            return new android.security.cts.IBinderExchange.Stub.Proxy(obj);
        }

        @Override
        public android.os.IBinder asBinder() {
            return this;
        }

        @Override
        public boolean onTransact(
            int code, android.os.Parcel data, android.os.Parcel reply, int flags)
                throws android.os.RemoteException {
            java.lang.String descriptor = DESCRIPTOR;
            switch (code) {
            case INTERFACE_TRANSACTION: {
                reply.writeString(descriptor);
                return true;
            }
            case TRANSACTION_putBinder: {
                data.enforceInterface(descriptor);
                android.os.IBinder _arg0;
                _arg0 = data.readStrongBinder();
                this.putBinder(_arg0);
                reply.writeNoException();
                return true;
            }
            case TRANSACTION_getBinder: {
                data.enforceInterface(descriptor);
                android.os.IBinder _result = this.getBinder();
                reply.writeNoException();
                reply.writeStrongBinder(_result);
                return true;
            }
            default: {
                return super.onTransact(code, data, reply, flags);
            }
            }
        }

        private static class Proxy implements android.security.cts.IBinderExchange {
            private android.os.IBinder mRemote;

            Proxy(android.os.IBinder remote) {
                mRemote = remote;
            }

            @Override
            public android.os.IBinder asBinder() {
                return mRemote;
            }

            public java.lang.String getInterfaceDescriptor() {
                return DESCRIPTOR;
            }

            @Override
            public void putBinder(android.os.IBinder bnd) throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeStrongBinder(bnd);
                    mRemote.transact(Stub.TRANSACTION_putBinder, _data, _reply, 0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }

            @Override
            public android.os.IBinder getBinder() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                android.os.IBinder _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    mRemote.transact(Stub.TRANSACTION_getBinder, _data, _reply, 0);
                    _reply.readException();
                    _result = _reply.readStrongBinder();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }
        }

        static final int TRANSACTION_putBinder = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
        static final int TRANSACTION_getBinder = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
    }

    public void putBinder(android.os.IBinder bnd) throws android.os.RemoteException;

    public android.os.IBinder getBinder() throws android.os.RemoteException;
}
