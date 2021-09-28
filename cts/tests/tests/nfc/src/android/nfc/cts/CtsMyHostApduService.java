package android.nfc.cts;

import android.nfc.cardemulation.*;
import android.os.Bundle;

public class CtsMyHostApduService extends HostApduService {
    @Override
    public byte[] processCommandApdu(byte[] apdu, Bundle extras) {
        return new byte[0];
    }

    @Override
    public void onDeactivated(int reason) {
        return;
    }
}
