package android.os.audio;

/**
 * @hide
 */
interface IRkAudioSettingService {
    int getSelect(int device);
    void setSelect(int device);
    int getMode(int device);
    void setMode(int device, int mode);
    int getFormat(int device, String format);
    void setFormat(int device, int close, String format);
}
